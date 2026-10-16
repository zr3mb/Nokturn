#include "DiscordPresence.h"

#include "AppPaths.h"
#include "PlayerController.h"
#include "TrackModel.h"
#include "youtube/YouTubeService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUuid>
#include <QtEndian>

#ifndef NOKTURN_DISCORD_CLIENT_ID
#  define NOKTURN_DISCORD_CLIENT_ID ""
#endif

namespace {

constexpr int kMinPushIntervalMs = 4000;   // Discord accepts about 5 status changes per 20 s

// Discord requires 2–128 characters in text fields.
QString clip(QString s, int max = 128)
{
    s = s.simplified();
    if (s.size() > max)
        s = s.left(max - 1) + QChar(0x2026);
    while (s.size() < 2)
        s += QChar(' ');
    return s;
}

} // namespace

DiscordPresence *DiscordPresence::instance()
{
    static DiscordPresence *inst = new DiscordPresence;
    return inst;
}

DiscordPresence *DiscordPresence::create(QQmlEngine *, QJSEngine *)
{
    DiscordPresence *d = instance();
    QJSEngine::setObjectOwnership(d, QJSEngine::CppOwnership);
    return d;
}

DiscordPresence::DiscordPresence(QObject *parent)
    : QObject(parent)
{
    auto settings = AppPaths::settings();
    m_enabled = settings->value("discord/enabled", false).toBool();
    m_clientId = settings->value("discord/clientId").toString();
    if (m_clientId.isEmpty())
        m_clientId = QStringLiteral(NOKTURN_DISCORD_CLIENT_ID);
    m_showWhenPaused = settings->value("discord/showPaused", false).toBool();

    m_retry.setSingleShot(true);
    m_retry.setInterval(15000);
    connect(&m_retry, &QTimer::timeout, this, &DiscordPresence::connectNext);

    m_debounce.setSingleShot(true);
    m_debounce.setInterval(1200);
    connect(&m_debounce, &QTimer::timeout, this, &DiscordPresence::pushActivity);

    connect(&m_socket, &QLocalSocket::connected, this, &DiscordPresence::onConnected);
    connect(&m_socket, &QLocalSocket::readyRead, this, &DiscordPresence::onReadyRead);
    connect(&m_socket, &QLocalSocket::disconnected, this, &DiscordPresence::onDisconnected);
    connect(&m_socket, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError) {
        // A failed connection attempt — try the next socket (discord-ipc-0…9, different directories).
        if (!m_ready && m_socket.state() == QLocalSocket::UnconnectedState)
            QTimer::singleShot(0, this, &DiscordPresence::connectNext);
    });

    QTimer::singleShot(1500, this, &DiscordPresence::connectNext);
}

void DiscordPresence::attach(PlayerController *player)
{
    m_player = player;
    connect(player, &PlayerController::currentChanged, this, [this] {
        lookupCover();
        scheduleUpdate();
    });
    connect(player, &PlayerController::playingChanged, this, &DiscordPresence::scheduleUpdate);
    connect(player, &PlayerController::durationChanged, this, &DiscordPresence::scheduleUpdate);
    connect(player, &PlayerController::seeked, this, &DiscordPresence::scheduleUpdate);
}

// ---- settings -------------------------------------------------------------------

void DiscordPresence::setEnabled(bool on)
{
    if (on == m_enabled)
        return;
    m_enabled = on;
    AppPaths::settings()->setValue("discord/enabled", on);
    emit settingsChanged();
    if (on) {
        reconnect();
    } else {
        if (m_ready)
            send(Frame, { { "cmd", "SET_ACTIVITY" },
                          { "args", QJsonObject { { "pid", qint64(QCoreApplication::applicationPid()) } } },
                          { "nonce", QUuid::createUuid().toString(QUuid::WithoutBraces) } });
        m_socket.flush();
        m_socket.disconnectFromServer();
        m_retry.stop();
        emit stateChanged();
    }
}

void DiscordPresence::setClientId(const QString &id)
{
    static const QRegularExpression nonDigits(QStringLiteral("\\D"));
    QString clean = id;
    clean.remove(nonDigits);
    if (clean == m_clientId)
        return;
    m_clientId = clean;
    AppPaths::settings()->setValue("discord/clientId", clean);
    emit settingsChanged();
    if (m_enabled)
        reconnect();
}

bool DiscordPresence::builtInClientId() const
{
    return !QStringLiteral(NOKTURN_DISCORD_CLIENT_ID).isEmpty();
}

void DiscordPresence::setShowWhenPaused(bool on)
{
    if (on == m_showWhenPaused)
        return;
    m_showWhenPaused = on;
    AppPaths::settings()->setValue("discord/showPaused", on);
    emit settingsChanged();
    scheduleUpdate();
}

QString DiscordPresence::status() const
{
    if (!m_enabled)
        return tr("Off");
    if (m_clientId.isEmpty())
        return tr("Enter the Discord application ID");
    if (m_ready)
        return m_userName.isEmpty() ? tr("Connected") : tr("Connected as %1").arg(m_userName);
    if (!m_lastError.isEmpty())
        return m_lastError;
    return tr("Connecting to Discord…");
}

// ---- connection -----------------------------------------------------------------

QStringList DiscordPresence::socketCandidates() const
{
    QStringList out;
#ifdef Q_OS_WIN
    // QLocalSocket turns the name into \\.\pipe\discord-ipc-N.
    for (int i = 0; i < 10; ++i)
        out << QStringLiteral("discord-ipc-%1").arg(i);
#else
    QStringList dirs;
    for (const char *var : { "XDG_RUNTIME_DIR", "TMPDIR", "TMP", "TEMP" }) {
        const QString v = qEnvironmentVariable(var);
        if (!v.isEmpty() && !dirs.contains(v))
            dirs << v;
    }
    if (!dirs.contains(QStringLiteral("/tmp")))
        dirs << QStringLiteral("/tmp");
    // Discord from Flatpak / Snap and popular alternative clients keep the socket in subdirectories.
    const QStringList subdirs = {
        QString(), QStringLiteral("app/com.discordapp.Discord/"), QStringLiteral("app/com.discordapp.DiscordCanary/"),
        QStringLiteral("snap.discord/"), QStringLiteral("snap.discord-canary/"),
        QStringLiteral(".flatpak/dev.vencord.Vesktop/xdg-run/"),
    };
    for (const QString &dir : std::as_const(dirs))
        for (const QString &sub : subdirs)
            for (int i = 0; i < 10; ++i) {
                const QString path = QDir(dir).filePath(sub + QStringLiteral("discord-ipc-%1").arg(i));
                if (QFileInfo::exists(path))
                    out << path;
            }
#endif
    return out;
}

void DiscordPresence::reconnect()
{
    m_retry.stop();
    m_socket.abort();
    m_ready = false;
    m_minimal = false;
    m_candidate = 0;
    m_lastError.clear();
    m_lastSent.clear();
    emit stateChanged();
    connectNext();
}

void DiscordPresence::connectNext()
{
    if (!m_enabled || m_clientId.isEmpty() || m_socket.state() != QLocalSocket::UnconnectedState)
        return;
    if (m_candidate == 0)
        m_candidates = socketCandidates();
    if (m_candidate >= m_candidates.size()) {
        m_candidate = 0;
        m_lastError = tr("Discord isn't running");
        emit stateChanged();
        m_retry.start();
        return;
    }
    m_socket.connectToServer(m_candidates.at(m_candidate++));
}

void DiscordPresence::onConnected()
{
    m_buffer.clear();
    send(Handshake, { { "v", 1 }, { "client_id", m_clientId } });
}

void DiscordPresence::onDisconnected()
{
    const bool wasReady = m_ready;
    m_ready = false;
    m_buffer.clear();
    m_candidate = 0;
    if (wasReady && m_lastError.isEmpty())
        m_lastError = tr("Disconnected from Discord");
    emit stateChanged();
    if (m_enabled)
        m_retry.start();
}

void DiscordPresence::send(Opcode op, const QJsonObject &payload)
{
    const QByteArray json = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QByteArray frame(8, Qt::Uninitialized);
    qToLittleEndian<quint32>(op, frame.data());
    qToLittleEndian<quint32>(quint32(json.size()), frame.data() + 4);
    m_socket.write(frame + json);
}

void DiscordPresence::onReadyRead()
{
    m_buffer += m_socket.readAll();
    while (m_buffer.size() >= 8) {
        const quint32 op = qFromLittleEndian<quint32>(m_buffer.constData());
        const quint32 len = qFromLittleEndian<quint32>(m_buffer.constData() + 4);
        if (len > 1 << 20) {   // damaged stream
            m_socket.abort();
            return;
        }
        if (quint32(m_buffer.size()) < 8 + len)
            break;
        const QJsonObject payload = QJsonDocument::fromJson(m_buffer.mid(8, len)).object();
        m_buffer.remove(0, 8 + qsizetype(len));
        handle(Opcode(op), payload);
    }
}

void DiscordPresence::handle(Opcode op, const QJsonObject &payload)
{
    switch (op) {
    case Frame: {
        const QString evt = payload.value("evt").toString();
        const QJsonObject data = payload.value("data").toObject();
        if (evt == QLatin1String("READY")) {
            m_ready = true;
            m_candidate = 0;
            m_lastError.clear();
            const QJsonObject user = data.value("user").toObject();
            m_userName = user.value("global_name").toString();
            if (m_userName.isEmpty())
                m_userName = user.value("username").toString();
            m_lastSent.clear();
            emit stateChanged();
            pushActivity();
        } else if (evt == QLatin1String("ERROR")) {
            // Older Discord clients don't know the newer fields (status type, buttons) — try without them.
            if (payload.value("cmd").toString() == QLatin1String("SET_ACTIVITY") && !m_minimal) {
                m_minimal = true;
                m_lastSent.clear();
                pushActivity();
            } else {
                m_lastError = data.value("message").toString();
                emit stateChanged();
            }
        }
        break;
    }
    case Close:
        m_lastError = payload.value("message").toString();
        if (m_lastError.contains(QLatin1String("client id"), Qt::CaseInsensitive))
            m_lastError = tr("Discord didn't recognize the application ID");
        emit stateChanged();
        m_socket.disconnectFromServer();
        break;
    case Ping:
        send(Pong, payload);
        break;
    default:
        break;
    }
}

// ---- status ---------------------------------------------------------------------

void DiscordPresence::scheduleUpdate()
{
    if (m_ready)
        m_debounce.start();
}

void DiscordPresence::pushActivity()
{
    if (!m_ready)
        return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastPush < kMinPushIntervalMs) {
        m_debounce.start(int(kMinPushIntervalMs - (now - m_lastPush)));
        return;
    }

    QJsonObject args { { "pid", qint64(QCoreApplication::applicationPid()) } };
    const QJsonObject activity = buildActivity();
    if (!activity.isEmpty())
        args.insert("activity", activity);   // no "activity" = clear the status

    // No change (ignoring timestamps shifted by a fraction of a second) — we send nothing.
    QJsonObject key = args;
    QJsonObject keyActivity = activity;
    if (keyActivity.contains("timestamps")) {
        QJsonObject ts = keyActivity.value("timestamps").toObject();
        ts.insert("start", double(qint64(ts.value("start").toDouble()) / 2000));
        ts.insert("end", double(qint64(ts.value("end").toDouble()) / 2000));
        keyActivity.insert("timestamps", ts);
        key.insert("activity", keyActivity);
    }
    const QByteArray fingerprint = QJsonDocument(key).toJson(QJsonDocument::Compact);
    if (fingerprint == m_lastSent)
        return;
    m_lastSent = fingerprint;
    m_lastPush = now;

    send(Frame, { { "cmd", "SET_ACTIVITY" }, { "args", args },
                  { "nonce", QUuid::createUuid().toString(QUuid::WithoutBraces) } });
}

QJsonObject DiscordPresence::buildActivity() const
{
    if (!m_player || !m_player->hasTrack())
        return {};
    const Track *t = m_player->currentTrack();
    if (!t)
        return {};
    const bool playing = m_player->playing() || m_player->buffering();
    if (!playing && !m_showWhenPaused)
        return {};

    QJsonObject a;
    a.insert("type", 2);   // "Listening"
    a.insert("details", clip(t->title));
    a.insert("state", clip(playing ? t->artist : tr("Paused · %1").arg(t->artist)));
    if (!m_minimal)
        a.insert("status_display_type", 2);   // in the member list: the track title instead of the app name

    const int duration = m_player->duration();
    if (playing && duration > 0) {
        const qint64 start = QDateTime::currentMSecsSinceEpoch() - qint64(m_player->position() * 1000);
        a.insert("timestamps", QJsonObject { { "start", double(start) }, { "end", double(start + qint64(duration) * 1000) } });
    }

    // Cover: an https URL (YouTube Music) or the cover of the matched YouTube Music track
    // for local files; otherwise the app icon (the "nokturn" asset in the Discord application).
    QString image = t->cover.startsWith(QLatin1String("https://")) ? t->cover : m_covers.value(t->id);
    if (image.isEmpty() || image.size() > 256)
        image = QStringLiteral("nokturn");
    QJsonObject assets { { "large_image", image } };
    assets.insert("large_text", clip(t->album.isEmpty() ? t->title : t->album));
    a.insert("assets", assets);

    if (!m_minimal && t->source == QLatin1String("youtube")) {
        a.insert("buttons", QJsonArray { QJsonObject {
            { "label", tr("Listen on YouTube Music") },
            { "url", QStringLiteral("https://music.youtube.com/watch?v=") + t->path } } });
    }
    return a;
}

void DiscordPresence::lookupCover()
{
    if (!m_enabled || !m_player)
        return;
    const Track *t = m_player->currentTrack();
    if (!t || t->source != QLatin1String("local") || t->title.isEmpty() || m_covers.contains(t->id)
        || m_coverLookup == t->id)
        return;
    const Track local = *t;
    m_coverLookup = local.id;
    YouTubeService::instance()->api()->search(local.artist + QChar(' ') + local.title,
                                              [this, local](const QList<Track> &results, const QString &error) {
        m_coverLookup.clear();
        const int match = error.isEmpty() ? YtMusicApi::bestMatch(local, results) : -1;
        m_covers.insert(local.id, match >= 0 ? results.at(match).cover : QString());
        if (m_player && m_player->currentTrackId() == local.id)
            scheduleUpdate();
    });
}
