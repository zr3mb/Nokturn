#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QLocalSocket>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class PlayerController;
class QQmlEngine;
class QJSEngine;

// Status na Discordzie ("Słucha …"): lokalne IPC klienta Discord (Rich Presence).
// Windows: potok nazwany \\.\pipe\discord-ipc-N, Linux/macOS: gniazdo discord-ipc-N
// w $XDG_RUNTIME_DIR (także wersje Flatpak / Snap). Ramka = [opcode u32][długość u32][JSON].
// Discord wymaga identyfikatora aplikacji (Application ID z discord.com/developers) —
// jej nazwa jest wyświetlana jako "Słucha <nazwa>".
class DiscordPresence : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Discord)
    QML_SINGLETON

    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY settingsChanged)
    Q_PROPERTY(QString clientId READ clientId WRITE setClientId NOTIFY settingsChanged)
    Q_PROPERTY(bool builtInClientId READ builtInClientId CONSTANT)
    Q_PROPERTY(bool showWhenPaused READ showWhenPaused WRITE setShowWhenPaused NOTIFY settingsChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString userName READ userName NOTIFY stateChanged)

public:
    static DiscordPresence *instance();
    static DiscordPresence *create(QQmlEngine *, QJSEngine *);

    void attach(PlayerController *player);

    bool enabled() const { return m_enabled; }
    void setEnabled(bool on);
    QString clientId() const { return m_clientId; }
    void setClientId(const QString &id);
    bool builtInClientId() const;
    bool showWhenPaused() const { return m_showWhenPaused; }
    void setShowWhenPaused(bool on);
    bool connected() const { return m_ready; }
    QString status() const;
    QString userName() const { return m_userName; }

    Q_INVOKABLE void reconnect();

signals:
    void settingsChanged();
    void stateChanged();

private:
    enum Opcode : quint32 { Handshake = 0, Frame = 1, Close = 2, Ping = 3, Pong = 4 };

    explicit DiscordPresence(QObject *parent = nullptr);

    QStringList socketCandidates() const;
    void connectNext();
    void onConnected();
    void onReadyRead();
    void onDisconnected();
    void send(Opcode op, const QJsonObject &payload);
    void handle(Opcode op, const QJsonObject &payload);
    void scheduleUpdate();
    void pushActivity();
    QJsonObject buildActivity() const;
    void lookupCover();

    QPointer<PlayerController> m_player;
    QLocalSocket m_socket;
    QStringList m_candidates;
    int m_candidate = 0;
    QByteArray m_buffer;
    bool m_ready = false;
    bool m_minimal = false;           // Discord odrzucił pola rozszerzone — wysyłamy podstawowe
    QString m_userName;
    QString m_lastError;
    QTimer m_retry;
    QTimer m_debounce;
    qint64 m_lastPush = 0;
    QByteArray m_lastSent;

    bool m_enabled = false;
    QString m_clientId;
    bool m_showWhenPaused = false;

    QHash<QString, QString> m_covers;   // id utworu lokalnego -> okładka z YouTube Music (https)
    QString m_coverLookup;
};
