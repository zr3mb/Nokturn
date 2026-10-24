#pragma once

#include "YtCookies.h"

#include <QObject>
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
