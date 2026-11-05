#pragma once

#include "YtCookies.h"

#include <QObject>
#include <QElapsedTimer>
#include <QProcess>
#include <QTimer>
#include <QWebSocket>

#include <functional>

// Signing in to YouTube Music in a real browser — in two stages:
//
// 1. Interactive: the installed Chrome / Edge / Brave starts as a separate app window
//    with its own clean profile and WITHOUT DevTools. Google blocks signing in in browsers
//    with remote debugging turned on ("This browser may not be secure"), so this window
//    is a regular browser. When the window title changes to "YouTube Music" (after signing in Google
//    redirects to music.youtube.com), we close it gently so the cookies get written to disk.
//
// 2. Readout: the same profile is started invisibly (headless) with the DevTools Protocol only
//    to read the saved session through Storage.getCookies. Nothing signs in at that point.
//
// The user's regular browser profiles are not touched. SoundCloud signs in the same way (its own profile).
class BrowserLogin : public QObject
{
    Q_OBJECT

public:
    enum class Stage { Idle, Interactive, Closing, Reading };
    enum class Site { YouTube, SoundCloud };

    explicit BrowserLogin(QObject *parent = nullptr);
    void setSite(Site site) { m_site = site; }
    ~BrowserLogin() override;

    static QString findBrowser(QString *displayName = nullptr);
    // Titles of the visible windows of a process / closing them gently (like clicking X). Windows only —
    // elsewhere the titles are empty and closing does nothing.
    static QStringList windowTitles(qint64 pid);
    static void closeWindows(qint64 pid);

    // The browser's own processes (not its helpers) running with this profile, wherever they came from:
    // a launcher may hand the window to another process and quit, so the pid we started isn't enough.
    // Windows only — elsewhere just `extra`, when it still runs.
    static QList<qint64> profileProcesses(const QString &profileDir, qint64 extra = 0);
    static QStringList profileWindowTitles(const QString &profileDir, qint64 extra = 0);
    static void closeProfileWindows(const QString &profileDir, qint64 extra = 0);
    // Ends what still holds the profile (a window left open, a hidden browser from an earlier attempt).
    static void killProfileProcesses(const QString &profileDir);
    // Calls `then` (with `context` alive) once nothing holds the profile: waits up to `waitMs`,
    // then ends the leftovers. The hidden readout has to start on a free profile, otherwise the browser
    // hands it to the open one and DevTools never answers.
    static void whenProfileFree(QObject *context, const QString &profileDir, int waitMs, std::function<void()> then);
    // A policy (company or school computer) that turns off remote debugging — the readout can't work then.
    static bool debuggingBlocked(const QString &browser);
    // Appends a line to <data>/login.log: what happened during sign-in, for a tester to send.
    static void log(const QString &line);

    bool running() const { return m_stage != Stage::Idle; }
    QString status() const;
    void start(const QString &profileDir);
    // SoundCloud: no window — the signed-in profile opens the site invisibly (which renews its own session),
    // then the cookies are read; done when `accept` likes them (a fresh token), within ~15 s.
    void refresh(const QString &profileDir, std::function<bool(const YtCookies &)> accept);
    void cancel();

signals:
    void runningChanged();
    void statusChanged();
    void succeeded(const YtCookies &cookies);
    void failed(const QString &reason);

private:
    void setStage(Stage stage);
    void checkWindowTitle();
    void launchWindow();
    void closeInteractiveWindow();
    void startReading();
    void waitForDevTools();
    void requestCookies();
    void onMessage(const QString &message);
    void onProcessFinished();
    void fail(const QString &reason);
    void stopAll();

    bool signedIn(const YtCookies &cookies) const;

    QString m_browser;
    QString m_profileDir;
    Site m_site = Site::YouTube;
    bool m_closePending = false;   // SoundCloud: signed in, the window closes in a moment
    bool m_refreshMode = false;    // refresh(): only the invisible stage, on the site itself
    bool m_readerLaunched = false; // the hidden browser of the readout has been started
    QStringList m_lastTitles;      // for the log: only changes are written
    QElapsedTimer m_launchClock;
    std::function<bool(const YtCookies &)> m_accept;
    Stage m_stage = Stage::Idle;
    QProcess m_process;
    QWebSocket m_socket;
    QTimer m_titleTimer;
    QTimer m_closeTimer;
    QTimer m_portTimer;
    QTimer m_retryTimer;
    int m_portAttempts = 0;
    int m_readAttempts = 0;
    int m_requestId = 0;
};
