#pragma once

#include "YtCookies.h"

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QWebSocket>

// Logowanie do YouTube Music w prawdziwej przeglądarce — w dwóch etapach:
//
// 1. Interaktywny: zainstalowany Chrome / Edge / Brave startuje jako osobne okno-aplikacja
//    z własnym, czystym profilem i BEZ DevTools. Google blokuje logowanie w przeglądarkach
//    z włączonym zdalnym debugowaniem ("Ta przeglądarka może nie być bezpieczna"), więc to okno
//    jest zwykłą przeglądarką. Gdy tytuł okna zmieni się na "YouTube Music" (po zalogowaniu Google
//    przekierowuje na music.youtube.com), zamykamy je łagodnie, żeby ciasteczka trafiły na dysk.
//
// 2. Odczyt: ten sam profil uruchamiamy niewidocznie (headless) z DevTools Protocol tylko po to,
//    by przez Storage.getCookies odczytać zapisaną sesję. Nic się wtedy nie loguje.
//
// Zwykłe profile przeglądarki użytkownika nie są dotykane.
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
