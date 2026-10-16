#include "BrowserLogin.h"

#include "Language.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace {

// Google sign-in page that returns to YouTube Music after signing in
// (the same URL YouTube itself uses; with a signed-in profile it goes straight through).
// {lang} = interface language.
const char *kLoginUrl =
    "https://accounts.google.com/ServiceLogin?service=youtube&uilel=3&passive=true&hl={lang}"
    "&continue=https%3A%2F%2Fwww.youtube.com%2Fsignin%3Faction_handle_signin%3Dtrue%26app%3Ddesktop"
    "%26hl%3D{lang}%26next%3Dhttps%253A%252F%252Fmusic.youtube.com%252F";

#ifdef Q_OS_WIN
QString appPath(const QString &exe)
{
    // Registry: HKLM/HKCU\Software\Microsoft\Windows\CurrentVersion\App Paths\<exe>
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
#endif

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

    // When the window doesn't close by itself (e.g. a "Leave site?" dialog), we kill the process.
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
#ifndef Q_OS_WIN
    // Linux: Chromium-family browsers in PATH. Snap versions last — their sandbox
    // doesn't allow a profile in a hidden directory (~/.local/share), so they're only a fallback.
    struct Candidate { const char *exe; const char *name; };
    const QList<Candidate> candidates = {
        { "google-chrome-stable", "Google Chrome" }, { "google-chrome", "Google Chrome" },
        { "chromium", "Chromium" }, { "chromium-browser", "Chromium" },
        { "brave-browser", "Brave" }, { "brave", "Brave" },
        { "microsoft-edge-stable", "Microsoft Edge" }, { "microsoft-edge", "Microsoft Edge" },
        { "vivaldi-stable", "Vivaldi" },
    };
    QString fallback, fallbackName;
    for (const Candidate &c : candidates) {
        const QString path = QStandardPaths::findExecutable(QString::fromLatin1(c.exe));
        if (path.isEmpty())
            continue;
        if (QFileInfo(path).canonicalFilePath().startsWith(QLatin1String("/snap/")) || path.startsWith(QLatin1String("/snap/"))) {
            if (fallback.isEmpty()) {
                fallback = path;
                fallbackName = QString::fromLatin1(c.name);
            }
            continue;
        }
        if (displayName)
            *displayName = QString::fromLatin1(c.name);
        return path;
    }
    if (displayName && !fallback.isEmpty())
        *displayName = fallbackName;
    return fallback;
#else
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
#endif
}

QString BrowserLogin::status() const
{
    switch (m_stage) {
    case Stage::Interactive:
#ifdef Q_OS_WIN
        return tr("Sign in in the window that opened. When you see YouTube Music, the window closes by itself.");
#else
        // On Linux (especially Wayland) we can't peek at another app's window title.
        return tr("Sign in in the window that opened, and close it once you see YouTube Music.");
#endif
    case Stage::Closing:
    case Stage::Reading:
        return tr("Reading the YouTube session…");
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
        emit failed(tr("Chrome, Edge or Brave was not found"));
        return;
    }

    m_profileDir = profileDir;
    QDir().mkpath(profileDir);
    // Chrome keeps a "lockfile" while the profile is open — a second launch would only
    // hand the window to the existing instance, so in that state we don't start over.
    const QString lock = profileDir + QStringLiteral("/lockfile");
    if (QFile::exists(lock) && !QFile::remove(lock)) {
        emit failed(tr("The sign-in window is already open — finish signing in there or close it"));
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
        QStringLiteral("--app=") + QString::fromLatin1(kLoginUrl).replace(QStringLiteral("{lang}"), Language::instance()->effective()),
    });
    m_process.start();
    m_titleTimer.start();
}

void BrowserLogin::cancel()
{
    if (!running())
        return;
    stopAll();
    emit failed(tr("Sign-in cancelled"));
}

// ---- stage 1: regular sign-in window ------------------------------------------------

void BrowserLogin::checkWindowTitle()
{
#ifdef Q_OS_WIN
    // The app window's title is the page title. After a successful sign-in Google redirects
    // to music.youtube.com, and the window starts being called "YouTube Music".
    const WindowScan scan = scanWindows(m_process.processId());
    for (const QString &title : scan.titles) {
        // Before the page provides a title, the window is named after the URL (e.g. "music.youtube.com_/").
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
    // A gentle close (like clicking X) — Chrome then writes the cookies to disk.
    for (HWND hwnd : scanWindows(m_process.processId()).windows)
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
#else
    m_process.terminate();   // SIGTERM — Chrome closes properly and saves the cookies
#endif
    m_closeTimer.start();
}

void BrowserLogin::onProcessFinished()
{
    switch (m_stage) {
    case Stage::Interactive:
        // The user closed the window — they may already have signed in, so we check the session anyway.
        m_titleTimer.stop();
        startReading();
        break;
    case Stage::Closing:
        m_closeTimer.stop();
        startReading();
        break;
    case Stage::Reading:
        if (m_socket.state() == QAbstractSocket::UnconnectedState && !m_portTimer.isActive())
            fail(tr("Couldn't read the session from the browser"));
        break;
    case Stage::Idle:
        break;
    }
}

// ---- stage 2: invisible cookie readout ---------------------------------------------

void BrowserLogin::startReading()
{
    setStage(Stage::Reading);
    m_portAttempts = 0;
    m_readAttempts = 0;
    QFile::remove(m_profileDir + QStringLiteral("/DevToolsActivePort"));

    // A moment for the closing browser to release the profile.
    QTimer::singleShot(500, this, [this] {
        if (m_stage != Stage::Reading)
            return;
        m_process.setProgram(m_browser);
        m_process.setArguments({
            QStringLiteral("--headless=new"),
            QStringLiteral("--remote-debugging-port=0"),   // the browser picks a free port itself
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
    // Chrome writes the chosen port and the DevTools path to the DevToolsActivePort file.
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
        fail(tr("Couldn't read the session from the browser (DevTools isn't responding)"));
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
    // The cookie store loads from disk with a delay — try a few more times.
    if (++m_readAttempts < 4) {
        m_retryTimer.start();
        return;
    }
    fail(tr("No sign-in detected — sign in in the window until you see YouTube Music"));
}

void BrowserLogin::fail(const QString &reason)
{
    stopAll();
    emit failed(reason);
}

void BrowserLogin::stopAll()
{
    const Stage stage = m_stage;
    // First the "idle" state, so closing the process doesn't trigger further stages.
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
#else
        m_process.terminate();
#endif
        if (!m_process.waitForFinished(4000))
            m_process.kill();
    }

    if (stage != Stage::Idle) {
        emit runningChanged();
        emit statusChanged();
    }
}
