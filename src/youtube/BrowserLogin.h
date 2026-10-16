#pragma once

#include "YtCookies.h"

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QWebSocket>

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
// The user's regular browser profiles are not touched.
class BrowserLogin : public QObject
{
    Q_OBJECT

public:
    enum class Stage { Idle, Interactive, Closing, Reading };

    explicit BrowserLogin(QObject *parent = nullptr);
    ~BrowserLogin() override;

    static QString findBrowser(QString *displayName = nullptr);

    bool running() const { return m_stage != Stage::Idle; }
    QString status() const;
    void start(const QString &profileDir);
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

    QString m_browser;
    QString m_profileDir;
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
