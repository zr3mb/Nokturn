#include "BrowserLogin.h"

#include "AppPaths.h"
#include "Language.h"

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QSet>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <tlhelp32.h>
#  include <winternl.h>
#else
#  include <signal.h>
#endif

namespace {

// Google sign-in page that returns to YouTube Music after signing in
// (the same URL YouTube itself uses; with a signed-in profile it goes straight through).
// {lang} = interface language.
const char *kLoginUrl =
    "https://accounts.google.com/ServiceLogin?service=youtube&uilel=3&passive=true&hl={lang}"
    "&continue=https%3A%2F%2Fwww.youtube.com%2Fsignin%3Faction_handle_signin%3Dtrue%26app%3Ddesktop"
    "%26hl%3D{lang}%26next%3Dhttps%253A%252F%252Fmusic.youtube.com%252F";
const char *kSoundCloudLoginUrl = "https://soundcloud.com/signin";

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
    QSet<DWORD> pids;
    QList<HWND> windows;
    QStringList titles;
};

BOOL CALLBACK collectWindow(HWND hwnd, LPARAM param)
{
    auto *scan = reinterpret_cast<WindowScan *>(param);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (scan->pids.contains(pid) && IsWindowVisible(hwnd)) {
        wchar_t title[512];
        const int len = GetWindowTextW(hwnd, title, 512);
        scan->windows << hwnd;
        scan->titles << QString::fromWCharArray(title, len);
    }
    return TRUE;
}

WindowScan scanWindows(const QList<qint64> &pids)
{
    WindowScan scan;
    for (qint64 pid : pids)
        if (pid > 0)
            scan.pids.insert(DWORD(pid));
    if (!scan.pids.isEmpty())
        EnumWindows(collectWindow, reinterpret_cast<LPARAM>(&scan));
    return scan;
}

WindowScan scanWindows(qint64 pid)
{
    return scanWindows(QList<qint64> { pid });
}

// Another process's command line (Windows 8.1+), empty when it can't be read.
QString commandLine(DWORD pid)
{
    using Query = LONG(WINAPI *)(HANDLE, ULONG, PVOID, ULONG, PULONG);
    static const auto query = reinterpret_cast<Query>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationProcess"));
    if (!query)
        return {};
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process)
        return {};
    constexpr ULONG kCommandLineInformation = 60;
    ULONG size = 0;
    query(process, kCommandLineInformation, nullptr, 0, &size);
    QString line;
    if (size > sizeof(UNICODE_STRING) && size < 1024 * 1024) {
        QByteArray buffer(int(size), 0);
        if (query(process, kCommandLineInformation, buffer.data(), size, &size) >= 0) {
            const auto *text = reinterpret_cast<const UNICODE_STRING *>(buffer.constData());
            line = QString::fromWCharArray(text->Buffer, text->Length / 2);
        }
    }
    CloseHandle(process);
    return line;
}

QString normalized(QString path)
{
    path = QDir::toNativeSeparators(QDir::cleanPath(path)).toLower();
    while (path.endsWith(QLatin1Char('\\')))
        path.chop(1);
    return path;
}
#endif

QString policyValueName(const QString &browser)
{
    const QString exe = QFileInfo(browser).fileName().toLower();
    if (exe == QLatin1String("msedge.exe"))
        return QStringLiteral("Microsoft\\Edge");
    if (exe == QLatin1String("brave.exe"))
        return QStringLiteral("BraveSoftware\\Brave");
    return QStringLiteral("Google\\Chrome");
}

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

    connect(&m_socket, &QWebSocket::connected, this, [this] {
        // Renewing: the site needs a few seconds to load and refresh its session first.
        if (m_refreshMode)
            QTimer::singleShot(6000, this, &BrowserLogin::requestCookies);
        else
            requestCookies();
    });
    connect(&m_socket, &QWebSocket::textMessageReceived, this, &BrowserLogin::onMessage);
    connect(&m_socket, &QWebSocket::errorOccurred, this, [this] {
        log(QStringLiteral("DevTools connection: %1").arg(m_socket.errorString()));
    });
    connect(&m_process, &QProcess::finished, this, &BrowserLogin::onProcessFinished);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        log(QStringLiteral("browser process error %1: %2").arg(int(error)).arg(m_process.errorString()));
    });
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
    // The first installed one whose policy allows the readout; a blocked one only when there's nothing else.
    QString blocked, blockedName;
    for (const Candidate &c : candidates) {
        QStringList paths = c.paths;
        paths.prepend(appPath(QString::fromLatin1(c.exe)));
        for (const QString &p : paths) {
            if (p.isEmpty() || !QFileInfo::exists(p))
                continue;
            if (debuggingBlocked(p)) {
                if (blocked.isEmpty()) {
                    blocked = QDir::toNativeSeparators(p);
                    blockedName = QString::fromLatin1(c.name);
                }
                break;
            }
            if (displayName)
                *displayName = QString::fromLatin1(c.name);
            return QDir::toNativeSeparators(p);
        }
    }
    if (displayName && !blocked.isEmpty())
        *displayName = blockedName;
    return blocked;
#endif
}

QList<qint64> BrowserLogin::profileProcesses(const QString &profileDir, qint64 extra)
{
    QList<qint64> pids;
#ifdef Q_OS_WIN
    const QString profile = normalized(profileDir);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry;
        entry.dwSize = sizeof(entry);
        for (bool ok = Process32FirstW(snapshot, &entry); ok; ok = Process32NextW(snapshot, &entry)) {
            const QString exe = QString::fromWCharArray(entry.szExeFile).toLower();
            if (exe != QLatin1String("chrome.exe") && exe != QLatin1String("msedge.exe") && exe != QLatin1String("brave.exe"))
                continue;
            // The browser process itself: its helpers carry "--type=renderer" and the like.
            const QString line = commandLine(entry.th32ProcessID).toLower();
            if (line.contains(QLatin1String("--type=")))
                continue;
            const int at = line.indexOf(QLatin1String("--user-data-dir="));
            if (at < 0)
                continue;
            QString dir = line.mid(at + 16).trimmed();
            if (dir.startsWith(QLatin1Char('"')))
                dir = dir.mid(1, dir.indexOf(QLatin1Char('"'), 1) - 1);
            else if (const int space = dir.indexOf(QLatin1String(" --")); space >= 0)
                dir = dir.left(space);
            if (normalized(dir) == profile)
                pids << qint64(entry.th32ProcessID);
        }
        CloseHandle(snapshot);
    }
#else
    // Chrome on Linux keeps a "SingletonLock" symlink to "<host>-<pid>" while the profile is open.
    const QFileInfo lock(profileDir + QStringLiteral("/SingletonLock"));
    if (lock.isSymLink()) {
        const qint64 pid = lock.symLinkTarget().section(QLatin1Char('-'), -1).toLongLong();
        if (pid > 0 && QFileInfo::exists(QStringLiteral("/proc/%1").arg(pid)))
            pids << pid;
    }
#endif
    if (extra > 0 && !pids.contains(extra)) {
#ifdef Q_OS_WIN
        if (HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, DWORD(extra))) {
            DWORD code = 0;
            if (GetExitCodeProcess(process, &code) && code == STILL_ACTIVE)
                pids << extra;
            CloseHandle(process);
        }
#else
        pids << extra;
#endif
    }
    return pids;
}

QStringList BrowserLogin::profileWindowTitles(const QString &profileDir, qint64 extra)
{
#ifdef Q_OS_WIN
    return scanWindows(profileProcesses(profileDir, extra)).titles;
#else
    Q_UNUSED(profileDir) Q_UNUSED(extra)
    return {};
#endif
}

void BrowserLogin::closeProfileWindows(const QString &profileDir, qint64 extra)
{
#ifdef Q_OS_WIN
    for (HWND hwnd : scanWindows(profileProcesses(profileDir, extra)).windows)
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
#else
    Q_UNUSED(profileDir) Q_UNUSED(extra)
#endif
}

void BrowserLogin::killProfileProcesses(const QString &profileDir)
{
#ifdef Q_OS_WIN
    for (qint64 pid : profileProcesses(profileDir)) {
        if (HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, DWORD(pid))) {
            TerminateProcess(process, 1);
            CloseHandle(process);
            log(QStringLiteral("ended a browser still holding the profile (pid %1)").arg(pid));
        }
    }
#else
    for (qint64 pid : profileProcesses(profileDir)) {
        ::kill(pid_t(pid), SIGKILL);
        log(QStringLiteral("ended a browser still holding the profile (pid %1)").arg(pid));
    }
#endif
}

void BrowserLogin::whenProfileFree(QObject *context, const QString &profileDir, int waitMs, std::function<void()> then)
{
    auto *timer = new QTimer(context);
    auto *clock = new QElapsedTimer;
    clock->start();
    timer->setInterval(250);
    QObject::connect(timer, &QTimer::timeout, context, [=] {
        const bool held = !profileProcesses(profileDir).isEmpty();
        if (held && clock->elapsed() < waitMs)
            return;
        if (held) {
            log(QStringLiteral("the profile is still in use after %1 ms — ending its browser").arg(clock->elapsed()));
            killProfileProcesses(profileDir);
        }
        timer->deleteLater();
        delete clock;
        // A moment for the ended browser to release its files.
        QTimer::singleShot(held ? 600 : 150, context, then);
    });
    timer->start();
}

bool BrowserLogin::debuggingBlocked(const QString &browser)
{
#ifdef Q_OS_WIN
    for (const char *root : { "HKEY_LOCAL_MACHINE", "HKEY_CURRENT_USER" }) {
        QSettings reg(QStringLiteral("%1\\Software\\Policies\\%2").arg(QLatin1String(root), policyValueName(browser)),
                      QSettings::NativeFormat);
        const QVariant allowed = reg.value(QStringLiteral("RemoteDebuggingAllowed"));
        if (allowed.isValid() && allowed.toInt() == 0)
            return true;
    }
#else
    Q_UNUSED(browser)
#endif
    return false;
}

void BrowserLogin::log(const QString &line)
{
    const QString path = AppPaths::dataDir() + QStringLiteral("/login.log");
    QFile file(path);
    if (file.size() > 256 * 1024)
        file.remove();   // only recent attempts matter
    if (file.open(QIODevice::Append | QIODevice::Text))
        file.write((QDateTime::currentDateTime().toString(Qt::ISODate) + QLatin1Char(' ') + line + QLatin1Char('\n')).toUtf8());
    if (qEnvironmentVariableIsSet("NOKTURN_LOGIN_DEBUG"))
        qInfo("login: %s", qPrintable(line));
}

QStringList BrowserLogin::windowTitles(qint64 pid)
{
#ifdef Q_OS_WIN
    return scanWindows(pid).titles;
#else
    Q_UNUSED(pid)
    return {};
#endif
}

void BrowserLogin::closeWindows(qint64 pid)
{
#ifdef Q_OS_WIN
    for (HWND hwnd : scanWindows(pid).windows)
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
#else
    Q_UNUSED(pid)
#endif
}

QString BrowserLogin::status() const
{
    if (m_site == Site::SoundCloud) {
        switch (m_stage) {
        case Stage::Interactive:
#ifdef Q_OS_WIN
            return tr("Sign in in the window that opened. Once SoundCloud shows your feed, the window closes by itself.");
#else
            return tr("Sign in in the window that opened, and close it once SoundCloud shows your feed.");
#endif
        case Stage::Closing:
        case Stage::Reading:
            return tr("Reading the SoundCloud session…");
        case Stage::Idle:
            break;
        }
        return {};
    }
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
    m_refreshMode = false;
    m_closePending = false;
    m_readerLaunched = false;
    m_lastTitles.clear();
    m_launchClock.invalidate();
    log(QStringLiteral("--- %1 sign-in with %2%3").arg(m_site == Site::SoundCloud ? QStringLiteral("SoundCloud") : QStringLiteral("YouTube"),
                                                       m_browser, debuggingBlocked(m_browser) ? QStringLiteral(" (remote debugging blocked by policy)") : QString()));
    setStage(Stage::Interactive);
    // A browser still on this profile (a window from an earlier attempt, a hidden readout that never ended)
    // would get the new window handed over and keep it out of our sight, so it goes first.
    if (!profileProcesses(profileDir).isEmpty())
        log(QStringLiteral("the profile is in use by an earlier browser, ending it"));
    whenProfileFree(this, profileDir, 0, [this] {
        if (m_stage == Stage::Interactive)
            launchWindow();
    });
}

void BrowserLogin::launchWindow()
{
    QFile::remove(m_profileDir + QStringLiteral("/lockfile"));
    QFile::remove(m_profileDir + QStringLiteral("/DevToolsActivePort"));
    const QString url = m_site == Site::SoundCloud
                            ? QString::fromLatin1(kSoundCloudLoginUrl)
                            : QString::fromLatin1(kLoginUrl).replace(QStringLiteral("{lang}"), Language::instance()->effective());
    m_process.setProgram(m_browser);
    m_process.setArguments({
        QStringLiteral("--user-data-dir=") + QDir::toNativeSeparators(m_profileDir),
        QStringLiteral("--no-first-run"),
        QStringLiteral("--no-default-browser-check"),
        QStringLiteral("--window-size=520,760"),
        QStringLiteral("--app=") + url,
    });
    m_process.start();
    m_launchClock.start();
    log(QStringLiteral("sign-in window started (pid %1)").arg(m_process.processId()));
    m_titleTimer.start();
}

void BrowserLogin::refresh(const QString &profileDir, std::function<bool(const YtCookies &)> accept)
{
    if (running())
        return;
    m_browser = findBrowser();
    if (m_browser.isEmpty()) {
        emit failed(tr("Chrome, Edge or Brave was not found"));
        return;
    }
    m_profileDir = profileDir;
    m_refreshMode = true;
    m_readerLaunched = false;
    m_launchClock.invalidate();
    log(QStringLiteral("--- renewing the session with %1").arg(m_browser));
    m_accept = std::move(accept);
    emit runningChanged();
    startReading();
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
    const QList<qint64> owners = profileProcesses(m_profileDir, m_process.processId());
    const WindowScan scan = scanWindows(owners);
    if (scan.titles != m_lastTitles) {
        m_lastTitles = scan.titles;
        log(QStringLiteral("window titles (processes %1): %2").arg(owners.size()).arg(scan.titles.join(QStringLiteral(" | "))));
    }
    // Nothing of the browser left on the profile: the user closed the window, maybe already signed in.
    // (The pid we started isn't the measure: a launcher may have handed the window over and quit.)
    if (owners.isEmpty() && m_launchClock.isValid() && m_launchClock.elapsed() > 1500) {
        log(QStringLiteral("the sign-in window was closed"));
        m_titleTimer.stop();
        startReading();
        return;
    }
    if (m_site == Site::SoundCloud) {
        // Signed in, SoundCloud lands on one of its own pages (the feed, Discover, the home page) — named by
        // their titles. Pages on the way (Google / Apple sign-in, secure.soundcloud.com, a window still named
        // after its URL) don't count. The window stays a moment longer, so the session cookie gets saved.
        if (m_closePending)
            return;
        for (const QString &title : scan.titles) {
            const QString t = title.toLower();
            if (!t.contains(QLatin1String("soundcloud")) || t.contains(QLatin1String("sign")) || t.contains(QLatin1String("register"))
                || t.contains(QLatin1String("log in")) || t.contains(QLatin1String("login")) || t.contains(QLatin1String(".com"))
                || t.contains(QLatin1String("secure")) || t.contains(QLatin1String("connect")))
                continue;
            if (t.contains(QLatin1String("discover")) || t.contains(QLatin1String("feed")) || t.contains(QLatin1String("stream"))
                || t.contains(QLatin1String("hear the world")) || t.contains(QLatin1String("library"))) {
                m_closePending = true;
                QTimer::singleShot(3000, this, [this] {
                    m_closePending = false;
                    if (m_stage == Stage::Interactive)
                        closeInteractiveWindow();
                });
                return;
            }
        }
        return;
    }
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
    log(QStringLiteral("signed in, closing the window"));
#ifdef Q_OS_WIN
    // A gentle close (like clicking X): Chrome then writes the cookies to disk. The readout waits
    // until the profile is free, and ends the browser if it doesn't close.
    closeProfileWindows(m_profileDir, m_process.processId());
#else
    m_process.terminate();   // SIGTERM — Chrome closes properly and saves the cookies
#endif
    startReading();
}

void BrowserLogin::onProcessFinished()
{
    log(QStringLiteral("browser process ended (exit code %1, %2 ms after start)")
            .arg(m_process.exitCode()).arg(m_launchClock.isValid() ? m_launchClock.elapsed() : -1));
    switch (m_stage) {
    case Stage::Interactive:
#ifdef Q_OS_WIN
        // Not necessarily the window closing: a launcher hands the window to another process and quits.
        // checkWindowTitle() watches the profile and notices when its window is really gone.
#else
        // The user closed the window — they may already have signed in, so we check the session anyway.
        m_titleTimer.stop();
        startReading();
#endif
        break;
    case Stage::Closing:
        break;   // the readout already waits for the profile
    case Stage::Reading:
        if (m_readerLaunched && m_socket.state() == QAbstractSocket::UnconnectedState && !m_portTimer.isActive())
            fail(tr("Couldn't read the session from the browser"));
        break;
    case Stage::Idle:
        break;
    }
}

// ---- stage 2: invisible cookie readout ---------------------------------------------

void BrowserLogin::startReading()
{
    if (m_stage == Stage::Reading)
        return;
    setStage(Stage::Reading);
    m_portAttempts = 0;
    m_readAttempts = 0;
    m_readerLaunched = false;

    // The hidden browser needs the profile to itself: a window still open on it would take the launch
    // over, and DevTools would never answer. A closing window gets a few seconds to save its cookies.
    whenProfileFree(this, m_profileDir, 12000, [this] {
        if (m_stage != Stage::Reading || m_readerLaunched)
            return;
        m_readerLaunched = true;
        QFile::remove(m_profileDir + QStringLiteral("/lockfile"));
        QFile::remove(m_profileDir + QStringLiteral("/DevToolsActivePort"));
        m_process.setProgram(m_browser);
        m_process.setArguments({
            QStringLiteral("--headless=new"),
            QStringLiteral("--remote-debugging-port=0"),   // the browser picks a free port itself
            QStringLiteral("--user-data-dir=") + QDir::toNativeSeparators(m_profileDir),
            QStringLiteral("--no-first-run"),
            QStringLiteral("--no-default-browser-check"),
            m_refreshMode ? QStringLiteral("https://soundcloud.com/discover") : QStringLiteral("about:blank"),
        });
        m_process.start();
        m_launchClock.start();
        log(QStringLiteral("hidden readout started (pid %1)").arg(m_process.processId()));
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
            log(QStringLiteral("DevTools on port %1").arg(lines.at(0).trimmed()));
            const QUrl ws(QStringLiteral("ws://127.0.0.1:%1%2").arg(lines.at(0).trimmed(), lines.at(1).trimmed()));
            m_socket.open(ws);
            return;
        }
    }
    if (++m_portAttempts > 80) {   // ~20 s
        log(QStringLiteral("DevTools never answered; browser processes on the profile: %1")
                .arg(profileProcesses(m_profileDir).size()));
        fail(debuggingBlocked(m_browser)
                 ? tr("A policy on this computer turns off remote debugging in the browser, so the session can't be read")
                 : tr("Couldn't read the session from the browser (DevTools isn't responding)"));
    }
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

    const YtCookies cookies = YtCookies::fromCdp(msg.value("result").toObject().value("cookies").toArray(),
                                                 m_site == Site::SoundCloud ? QStringLiteral("soundcloud.com")
                                                                            : QStringLiteral("youtube.com"));
    if (signedIn(cookies) && (!m_refreshMode || !m_accept || m_accept(cookies))) {
        log(QStringLiteral("signed in, session read"));
        stopAll();
        emit succeeded(cookies);
        return;
    }
    // The cookie store loads from disk with a delay (and the site renews its session in a moment) — try again.
    if (++m_readAttempts < (m_refreshMode ? 12 : 4)) {
        m_retryTimer.start();
        return;
    }
    if (m_refreshMode) {
        fail(tr("Couldn't renew the session"));
        return;
    }
    fail(m_site == Site::SoundCloud ? tr("No sign-in detected — sign in in the window until SoundCloud shows your feed")
                                    : tr("No sign-in detected — sign in in the window until you see YouTube Music"));
}

bool BrowserLogin::signedIn(const YtCookies &cookies) const
{
    if (m_site == Site::SoundCloud)
        return !cookies.value(QStringLiteral("oauth_token")).isEmpty();
    return cookies.isLoggedIn();
}

void BrowserLogin::fail(const QString &reason)
{
    log(QStringLiteral("failed: %1").arg(reason));
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
            closeProfileWindows(m_profileDir, m_process.processId());
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
