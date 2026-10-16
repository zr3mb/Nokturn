#include "BrowserLogin.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace {

// Strona logowania Google, która po zalogowaniu wraca do YouTube Music
// (ten sam adres, którego używa sam YouTube; z zalogowanym profilem przechodzi od razu).
const char *kLoginUrl =
    "https://accounts.google.com/ServiceLogin?service=youtube&uilel=3&passive=true&hl=pl"
    "&continue=https%3A%2F%2Fwww.youtube.com%2Fsignin%3Faction_handle_signin%3Dtrue%26app%3Ddesktop"
    "%26hl%3Dpl%26next%3Dhttps%253A%252F%252Fmusic.youtube.com%252F";

QString appPath(const QString &exe)
{
    // Rejestr: HKLM/HKCU\Software\Microsoft\Windows\CurrentVersion\App Paths\<exe>
    for (const char *root : { "HKEY_CURRENT_USER", "HKEY_LOCAL_MACHINE" }) {
        QSettings reg(QStringLiteral("%1\\Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\%2")
                          .arg(QLatin1String(root), exe),
                      QSettings::NativeFormat);
        const QString path = reg.value(".").toString();
        if (!path.isEmpty() && QFileInfo::exists(path))
            return path;
    }
    return {};
}

#ifdef Q_OS_WIN
struct WindowScan
{
    DWORD pid = 0;
    QList<HWND> windows;
    QStringList titles;
};

BOOL CALLBACK collectWindow(HWND hwnd, LPARAM param)
{
    auto *scan = reinterpret_cast<WindowScan *>(param);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == scan->pid && IsWindowVisible(hwnd)) {
        wchar_t title[512];
        const int len = GetWindowTextW(hwnd, title, 512);
        scan->windows << hwnd;
        scan->titles << QString::fromWCharArray(title, len);
    }
    return TRUE;
}

WindowScan scanWindows(qint64 pid)
{
    WindowScan scan;
    scan.pid = DWORD(pid);
    if (pid > 0)
        EnumWindows(collectWindow, reinterpret_cast<LPARAM>(&scan));
    return scan;
}
#endif

} // namespace

BrowserLogin::BrowserLogin(QObject *parent)
    : QObject(parent)
{
    m_titleTimer.setInterval(700);
    connect(&m_titleTimer, &QTimer::timeout, this, &BrowserLogin::checkWindowTitle);

    // Gdy okno nie zamknie się samo (np. dialog "Opuścić stronę?"), kończymy proces na siłę.
    m_closeTimer.setSingleShot(true);
    m_closeTimer.setInterval(12000);
    connect(&m_closeTimer, &QTimer::timeout, this, [this] { m_process.kill(); });

    m_portTimer.setInterval(250);
    connect(&m_portTimer, &QTimer::timeout, this, &BrowserLogin::waitForDevTools);

    m_retryTimer.setSingleShot(true);
    m_retryTimer.setInterval(800);
    connect(&m_retryTimer, &QTimer::timeout, this, &BrowserLogin::requestCookies);

    connect(&m_socket, &QWebSocket::connected, this, &BrowserLogin::requestCookies);
    connect(&m_socket, &QWebSocket::textMessageReceived, this, &BrowserLogin::onMessage);
    connect(&m_process, &QProcess::finished, this, &BrowserLogin::onProcessFinished);
}

BrowserLogin::~BrowserLogin()
{
    if (running())
        stopAll();
}

QString BrowserLogin::findBrowser(QString *displayName)
{
    struct Candidate { const char *exe; const char *name; QStringList paths; };
    const QString pf = qEnvironmentVariable("ProgramFiles");
    const QString pf86 = qEnvironmentVariable("ProgramFiles(x86)");
    const QString local = qEnvironmentVariable("LOCALAPPDATA");
    const QList<Candidate> candidates = {
        { "chrome.exe", "Google Chrome",
          { pf + "/Google/Chrome/Application/chrome.exe", pf86 + "/Google/Chrome/Application/chrome.exe",
            local + "/Google/Chrome/Application/chrome.exe" } },
        { "msedge.exe", "Microsoft Edge",
          { pf86 + "/Microsoft/Edge/Application/msedge.exe", pf + "/Microsoft/Edge/Application/msedge.exe" } },
        { "brave.exe", "Brave",
          { pf + "/BraveSoftware/Brave-Browser/Application/brave.exe",
            local + "/BraveSoftware/Brave-Browser/Application/brave.exe" } },
    };
    for (const Candidate &c : candidates) {
        QStringList paths = c.paths;
        paths.prepend(appPath(QString::fromLatin1(c.exe)));
        for (const QString &p : paths) {
            if (!p.isEmpty() && QFileInfo::exists(p)) {
                if (displayName)
                    *displayName = QString::fromLatin1(c.name);
                return QDir::toNativeSeparators(p);
            }
        }
    }
    return {};
}

QString BrowserLogin::status() const
{
    switch (m_stage) {
    case Stage::Interactive:
        return QStringLiteral("Zaloguj się w otwartym oknie. Gdy zobaczysz YouTube Music, okno zamknie się samo.");
    case Stage::Closing:
    case Stage::Reading:
        return QStringLiteral("Odczytuję sesję YouTube…");
    case Stage::Idle:
        break;
    }
    return {};
}

void BrowserLogin::setStage(Stage stage)
{
    const bool wasRunning = running();
    m_stage = stage;
    if (wasRunning != running())
        emit runningChanged();
    emit statusChanged();
}

void BrowserLogin::start(const QString &profileDir)
{
    if (running())
        return;
    m_browser = findBrowser();
    if (m_browser.isEmpty()) {
        emit failed(QStringLiteral("Nie znaleziono Chrome, Edge ani Brave"));
        return;
    }

    m_profileDir = profileDir;
    QDir().mkpath(profileDir);
    // Chrome trzyma "lockfile", dopóki profil jest otwarty — drugie uruchomienie tylko by
    // przekazało okno istniejącej instancji, więc w tym stanie nie zaczynamy od nowa.
    const QString lock = profileDir + QStringLiteral("/lockfile");
    if (QFile::exists(lock) && !QFile::remove(lock)) {
        emit failed(QStringLiteral("Okno logowania jest już otwarte — dokończ w nim logowanie albo je zamknij"));
        return;
    }
    QFile::remove(profileDir + QStringLiteral("/DevToolsActivePort"));

    setStage(Stage::Interactive);
    m_process.setProgram(m_browser);
    m_process.setArguments({
        QStringLiteral("--user-data-dir=") + QDir::toNativeSeparators(profileDir),
        QStringLiteral("--no-first-run"),
        QStringLiteral("--no-default-browser-check"),
        QStringLiteral("--window-size=520,760"),
        QStringLiteral("--app=") + QString::fromLatin1(kLoginUrl),
    });
    m_process.start();
    m_titleTimer.start();
}

void BrowserLogin::cancel()
{
    if (!running())
        return;
    stopAll();
    emit failed(QStringLiteral("Anulowano logowanie"));
}

// ---- etap 1: zwykłe okno logowania ------------------------------------------------

void BrowserLogin::checkWindowTitle()
{
#ifdef Q_OS_WIN
    // Tytuł okna-aplikacji to tytuł strony. Po udanym logowaniu Google przekierowuje
    // na music.youtube.com, a okno zaczyna nazywać się "YouTube Music".
    const WindowScan scan = scanWindows(m_process.processId());
    for (const QString &title : scan.titles) {
        // Zanim strona poda tytuł, okno nazywa się adresem (np. "music.youtube.com_/").
        if (title.contains(QLatin1String("YouTube Music"), Qt::CaseInsensitive)
            || title.contains(QLatin1String("music.youtube.com"), Qt::CaseInsensitive)) {
            closeInteractiveWindow();
            return;
        }
    }
#endif
}

void BrowserLogin::closeInteractiveWindow()
{
    m_titleTimer.stop();
    setStage(Stage::Closing);
#ifdef Q_OS_WIN
    // Łagodne zamknięcie (jak kliknięcie X) — Chrome zapisze wtedy ciasteczka na dysk.
    for (HWND hwnd : scanWindows(m_process.processId()).windows)
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
#endif
    m_closeTimer.start();
}

void BrowserLogin::onProcessFinished()
{
    switch (m_stage) {
    case Stage::Interactive:
        // Użytkownik sam zamknął okno — mógł się już zalogować, więc i tak sprawdzamy sesję.
        m_titleTimer.stop();
        startReading();
        break;
    case Stage::Closing:
        m_closeTimer.stop();
        startReading();
        break;
    case Stage::Reading:
        if (m_socket.state() == QAbstractSocket::UnconnectedState && !m_portTimer.isActive())
            fail(QStringLiteral("Nie udało się odczytać sesji z przeglądarki"));
        break;
    case Stage::Idle:
        break;
    }
}

// ---- etap 2: niewidoczny odczyt ciasteczek ---------------------------------------------

void BrowserLogin::startReading()
{
    setStage(Stage::Reading);
    m_portAttempts = 0;
    m_readAttempts = 0;
    QFile::remove(m_profileDir + QStringLiteral("/DevToolsActivePort"));

    // Chwila na zwolnienie profilu przez zamykającą się przeglądarkę.
    QTimer::singleShot(500, this, [this] {
        if (m_stage != Stage::Reading)
            return;
        m_process.setProgram(m_browser);
        m_process.setArguments({
            QStringLiteral("--headless=new"),
            QStringLiteral("--remote-debugging-port=0"),   // przeglądarka sama wybierze wolny port
            QStringLiteral("--user-data-dir=") + QDir::toNativeSeparators(m_profileDir),
            QStringLiteral("--no-first-run"),
            QStringLiteral("--no-default-browser-check"),
            QStringLiteral("about:blank"),
        });
        m_process.start();
        m_portTimer.start();
    });
}

void BrowserLogin::waitForDevTools()
{
    // Chrome zapisuje wybrany port i ścieżkę do DevTools w pliku DevToolsActivePort.
    QFile file(m_profileDir + QStringLiteral("/DevToolsActivePort"));
    if (file.open(QIODevice::ReadOnly)) {
        const QStringList lines = QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts);
        if (lines.size() >= 2) {
            m_portTimer.stop();
            const QUrl ws(QStringLiteral("ws://127.0.0.1:%1%2").arg(lines.at(0).trimmed(), lines.at(1).trimmed()));
            m_socket.open(ws);
            return;
        }
    }
    if (++m_portAttempts > 80)   // ~20 s
        fail(QStringLiteral("Nie udało się odczytać sesji z przeglądarki (DevTools nie odpowiada)"));
}

void BrowserLogin::requestCookies()
{
    if (m_socket.state() != QAbstractSocket::ConnectedState)
        return;
    const QJsonObject request { { "id", ++m_requestId }, { "method", "Storage.getCookies" } };
    m_socket.sendTextMessage(QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Compact)));
}

void BrowserLogin::onMessage(const QString &message)
{
    if (m_stage != Stage::Reading)
        return;
    const QJsonObject msg = QJsonDocument::fromJson(message.toUtf8()).object();
    if (!msg.contains("id") || !msg.value("result").isObject())
        return;

    const YtCookies cookies = YtCookies::fromCdp(msg.value("result").toObject().value("cookies").toArray());
    if (cookies.isLoggedIn()) {
        stopAll();
        emit succeeded(cookies);
        return;
    }
    // Magazyn ciasteczek wczytuje się z dysku z opóźnieniem — spróbuj jeszcze kilka razy.
    if (++m_readAttempts < 4) {
        m_retryTimer.start();
        return;
    }
    fail(QStringLiteral("Nie wykryto zalogowania — zaloguj się w oknie, aż zobaczysz YouTube Music"));
}

void BrowserLogin::fail(const QString &reason)
{
    stopAll();
    emit failed(reason);
}

void BrowserLogin::stopAll()
{
    const Stage stage = m_stage;
    // Najpierw stan "bezczynny", żeby zamykanie procesu nie uruchomiło kolejnych etapów.
    m_stage = Stage::Idle;
    m_titleTimer.stop();
    m_closeTimer.stop();
    m_portTimer.stop();
    m_retryTimer.stop();

    if (m_socket.state() == QAbstractSocket::ConnectedState) {
        const QJsonObject close { { "id", ++m_requestId }, { "method", "Browser.close" } };
        m_socket.sendTextMessage(QString::fromUtf8(QJsonDocument(close).toJson(QJsonDocument::Compact)));
        m_socket.flush();
    }
    m_socket.abort();

    if (m_process.state() != QProcess::NotRunning) {
#ifdef Q_OS_WIN
        if (stage == Stage::Interactive || stage == Stage::Closing)
            for (HWND hwnd : scanWindows(m_process.processId()).windows)
                PostMessageW(hwnd, WM_CLOSE, 0, 0);
#endif
        if (!m_process.waitForFinished(4000))
            m_process.kill();
    }

    if (stage != Stage::Idle) {
        emit runningChanged();
        emit statusChanged();
    }
}
