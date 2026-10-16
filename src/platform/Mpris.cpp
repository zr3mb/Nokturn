#include "Mpris.h"

#include "PlayerController.h"
#include "TrackModel.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QGuiApplication>
#include <QUrl>
#include <QWindow>

#include <algorithm>

namespace {

const QString kService = QStringLiteral("org.mpris.MediaPlayer2.nokturn");
const QString kPath = QStringLiteral("/org/mpris/MediaPlayer2");
const QString kPlayerInterface = QStringLiteral("org.mpris.MediaPlayer2.Player");

QDBusObjectPath trackPath(const QString &id)
{
    if (id.isEmpty())
        return QDBusObjectPath(QStringLiteral("/org/mpris/MediaPlayer2/TrackList/NoTrack"));
    // A D-Bus object path only allows [A-Za-z0-9_] — we use a hash of the id.
    return QDBusObjectPath(QStringLiteral("/org/nokturn/track/")
                           + QString::fromLatin1(QCryptographicHash::hash(id.toUtf8(), QCryptographicHash::Sha1).toHex()));
}

} // namespace

Mpris::Mpris(PlayerController *player)
    : QObject(player)
    , m_player(player)
{
    auto *root = new MprisRoot(this);
    auto *adaptor = new MprisPlayer(this);
    Q_UNUSED(root);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return;
    // A second instance of the program gets a name with a suffix (required by the spec with multiple instances).
    m_registered = bus.registerObject(kPath, this)
                   && (bus.registerService(kService)
                       || bus.registerService(kService + QStringLiteral(".instance")
                                              + QString::number(QCoreApplication::applicationPid())));
    if (!m_registered)
        return;

    connect(player, &PlayerController::currentChanged, this, [this] {
        notify(kPlayerInterface, { { "Metadata", metadata() } });
    });
    connect(player, &PlayerController::durationChanged, this, [this] {
        notify(kPlayerInterface, { { "Metadata", metadata() } });
    });
    connect(player, &PlayerController::playingChanged, this, [this] {
        notify(kPlayerInterface, { { "PlaybackStatus", playbackStatus() } });
    });
    connect(player, &PlayerController::volumeChanged, this, [this, adaptor] {
        notify(kPlayerInterface, { { "Volume", adaptor->volume() } });
    });
    connect(player, &PlayerController::mutedChanged, this, [this, adaptor] {
        notify(kPlayerInterface, { { "Volume", adaptor->volume() } });
    });
    connect(player, &PlayerController::shuffleChanged, this, [this, adaptor] {
        notify(kPlayerInterface, { { "Shuffle", adaptor->shuffle() } });
    });
    connect(player, &PlayerController::repeatModeChanged, this, [this, adaptor] {
        notify(kPlayerInterface, { { "LoopStatus", adaptor->loopStatus() } });
    });
    connect(player, &PlayerController::seeked, this, [adaptor](double seconds) {
        emit adaptor->Seeked(qlonglong(seconds * 1e6));
    });
}

Mpris::~Mpris()
{
    if (m_registered)
        QDBusConnection::sessionBus().unregisterObject(kPath);
}

QString Mpris::playbackStatus() const
{
    if (!m_player || !m_player->hasTrack())
        return QStringLiteral("Stopped");
    return m_player->playing() ? QStringLiteral("Playing") : QStringLiteral("Paused");
}

QVariantMap Mpris::metadata() const
{
    QVariantMap m;
    const Track *t = m_player ? m_player->currentTrack() : nullptr;
    m.insert(QStringLiteral("mpris:trackid"), QVariant::fromValue(trackPath(t ? t->id : QString())));
    if (!t)
        return m;
    if (const int duration = m_player->duration(); duration > 0)
        m.insert(QStringLiteral("mpris:length"), qlonglong(duration) * 1000000);
    m.insert(QStringLiteral("xesam:title"), t->title);
    m.insert(QStringLiteral("xesam:artist"), QStringList { t->artist });
    if (!t->album.isEmpty())
        m.insert(QStringLiteral("xesam:album"), t->album);
    if (!t->albumArtist.isEmpty())
        m.insert(QStringLiteral("xesam:albumArtist"), QStringList { t->albumArtist });
    if (t->trackNo > 0)
        m.insert(QStringLiteral("xesam:trackNumber"), t->trackNo);
    if (!t->genre.isEmpty())
        m.insert(QStringLiteral("xesam:genre"), QStringList { t->genre });
    if (!t->cover.isEmpty())
        m.insert(QStringLiteral("mpris:artUrl"), t->cover);
    if (t->source == QLatin1String("youtube"))
        m.insert(QStringLiteral("xesam:url"), QStringLiteral("https://music.youtube.com/watch?v=") + t->path);
    else if (t->source == QLatin1String("local"))
        m.insert(QStringLiteral("xesam:url"), QUrl::fromLocalFile(t->path).toString());
    return m;
}

void Mpris::notify(const QString &interface, const QVariantMap &changed)
{
    QDBusMessage signal = QDBusMessage::createSignal(kPath, QStringLiteral("org.freedesktop.DBus.Properties"),
                                                     QStringLiteral("PropertiesChanged"));
    signal << interface << changed << QStringList();
    QDBusConnection::sessionBus().send(signal);
}

// ---- org.mpris.MediaPlayer2 -----------------------------------------------------

void MprisRoot::Raise()
{
    for (QWindow *w : QGuiApplication::topLevelWindows()) {
        if (w->isVisible() || w->visibility() == QWindow::Minimized) {
            w->showNormal();
            w->raise();
            w->requestActivate();
        }
    }
}

void MprisRoot::Quit()
{
    QCoreApplication::quit();
}

// ---- org.mpris.MediaPlayer2.Player ----------------------------------------------

QString MprisPlayer::loopStatus() const
{
    const int mode = m_mpris->player() ? m_mpris->player()->repeatMode() : 0;
    return mode == 2 ? QStringLiteral("Track") : mode == 1 ? QStringLiteral("Playlist") : QStringLiteral("None");
}

void MprisPlayer::setLoopStatus(const QString &status)
{
    if (auto *p = m_mpris->player())
        p->setRepeatMode(status == QLatin1String("Track") ? 2 : status == QLatin1String("Playlist") ? 1 : 0);
}

bool MprisPlayer::shuffle() const
{
    return m_mpris->player() && m_mpris->player()->shuffle();
}

void MprisPlayer::setShuffle(bool on)
{
    if (auto *p = m_mpris->player())
        p->setShuffle(on);
}

double MprisPlayer::volume() const
{
    auto *p = m_mpris->player();
    return p && !p->muted() ? p->volume() : 0.0;
}

void MprisPlayer::setVolume(double v)
{
    if (auto *p = m_mpris->player()) {
        p->setMuted(false);
        p->setVolume(std::clamp(v, 0.0, 1.0));
    }
}

qlonglong MprisPlayer::position() const
{
    return m_mpris->player() ? qlonglong(m_mpris->player()->position() * 1e6) : 0;
}

bool MprisPlayer::canSeek() const
{
    return m_mpris->player() && m_mpris->player()->hasTrack() && !m_mpris->player()->live();
}

void MprisPlayer::Next()      { if (auto *p = m_mpris->player()) p->next(); }
void MprisPlayer::Previous()  { if (auto *p = m_mpris->player()) p->previous(); }
void MprisPlayer::Pause()     { if (auto *p = m_mpris->player()) p->pause(); }
void MprisPlayer::PlayPause() { if (auto *p = m_mpris->player()) p->toggle(); }
void MprisPlayer::Stop()      { if (auto *p = m_mpris->player()) { p->pause(); p->seek(0); } }
void MprisPlayer::Play()      { if (auto *p = m_mpris->player()) p->play(); }

void MprisPlayer::Seek(qlonglong offsetUs)
{
    if (auto *p = m_mpris->player()) {
        const double target = p->position() + offsetUs / 1e6;
        if (p->duration() > 0 && target > p->duration())
            p->next();
        else
            p->seek(std::max(0.0, target));
    }
}

void MprisPlayer::SetPosition(const QDBusObjectPath &trackId, qlonglong positionUs)
{
    auto *p = m_mpris->player();
    if (!p || trackId != trackPath(p->currentTrackId()))
        return;   // per the spec: a command for a different track is ignored
    if (positionUs >= 0 && (p->duration() <= 0 || positionUs <= qlonglong(p->duration()) * 1000000))
        p->seek(positionUs / 1e6);
}
