#include "PlayerController.h"

#include "AppPaths.h"
#include "Language.h"
#include "Library.h"
#include "TrackModel.h"
#include "audio/Equalizer.h"
#include "lyrics/LyricsService.h"
#include "platform/DiscordPresence.h"
#include "youtube/YouTubeService.h"

#ifdef NOKTURN_MPRIS
#  include "platform/Mpris.h"
#endif
#ifdef NOKTURN_SMTC
#  include "platform/WindowsMediaControls.h"
#endif

#include <QAudioFormat>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QtMultimedia/qtaudio.h>

#include <algorithm>

namespace {

const QStringList kMixStyles = { QStringLiteral("auto"), QStringLiteral("fade"), QStringLiteral("rise"),
                                 QStringLiteral("bass"), QStringLiteral("echo") };

AudioEngine::Style styleFromId(const QString &id)
{
    if (id == QLatin1String("rise"))
        return AudioEngine::Style::Rise;
    if (id == QLatin1String("bass"))
        return AudioEngine::Style::BassSwap;
    if (id == QLatin1String("echo"))
        return AudioEngine::Style::Echo;
    return AudioEngine::Style::Fade;
}

} // namespace

PlayerController::PlayerController(QObject *parent)
    : QObject(parent)
    , m_tracks(Library::instance()->tracks())
    , m_analyzer(kBars)
    , m_spectrum(kBars, 0.0)
    , m_targets(kBars, 0.0)
{
    // Every deck plays into a muted output (it sets the playback pace), while the audible
    // sound takes a separate path: samples -> mixer -> equalizer -> AudioEngine (QAudioSink).
    for (int d = 0; d < AudioEngine::kDecks; ++d)
        setupDeck(d);

    connect(&m_engine, &AudioEngine::transitionFinished, this, [this](int fromDeck) {
        Deck &old = m_decks[fromDeck];
        old.player->stop();
        old.player->setSource(QUrl());
        old.trackId.clear();
        old.resolving.clear();
        m_transitionLabel.clear();
        emit transitionChanged();
    });

    auto settings = AppPaths::settings();
    m_volume = std::clamp(settings->value("player/volume", 0.72).toDouble(), 0.0, 1.0);
    m_muted = settings->value("player/muted", false).toBool();
    m_shuffle = settings->value("player/shuffle", false).toBool();
    m_repeat = std::clamp(settings->value("player/repeat", 0).toInt(), 0, 2);
    applyVolume();
    loadMixSettings();

    connect(m_tracks, &QAbstractItemModel::modelReset, this, &PlayerController::onModelReset);
    connect(m_tracks, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &from, const QModelIndex &to) {
        if (m_index >= from.row() && m_index <= to.row())
            emit colorsChanged();   // e.g. a palette computed from a cover downloaded from the web
    });

    // Tracks from YouTube: the stream URL comes from yt-dlp (asynchronously).
    StreamResolver *resolver = YouTubeService::instance()->resolver();
    connect(resolver, &StreamResolver::resolved, this, [this](const QString &videoId, const QUrl &url) {
        for (int d = 0; d < AudioEngine::kDecks; ++d) {
            Deck &deck = m_decks[d];
            if (deck.resolving != videoId)
                continue;   // e.g. a prefetch result or the track has already changed
            deck.resolving.clear();
            deck.player->setSource(url);
            if (d == m_active && m_autoplayAfterResolve)
                deck.player->play();
        }
        emit bufferingChanged();
    });
    connect(resolver, &StreamResolver::failed, this, [this](const QString &videoId, const QString &error) {
        if (m_decks[standby()].resolving == videoId)
            cancelPreload();   // the next track won't load — the transition will be a plain one
        if (m_decks[m_active].resolving != videoId)
            return;
        m_decks[m_active].resolving.clear();
        emit bufferingChanged();
        emit notice(tr("Couldn't play from YouTube: %1").arg(error), QStringLiteral("close"));
    });
    connect(m_tracks, &TrackModel::likedChanged, this, [this](int row, bool) {
        if (row == m_index)
            emit likedChanged();
    });

    m_timer.setInterval(16);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &PlayerController::tick);
    m_clock.start();

    DiscordPresence::instance()->attach(this);
#ifdef NOKTURN_MPRIS
    new Mpris(this);   // Linux: MPRIS (system panel, media keys)
#endif
#ifdef NOKTURN_SMTC
    new WindowsMediaControls(this);   // Windows: media flyout, lock screen, media keys
#endif

    // Labels computed here ("Nothing playing", mix style names) follow the interface language.
    connect(Language::instance(), &Language::changed, this, [this] {
        emit currentChanged();
        emit mixChanged();
    });

    // Restore the last played track (paused, at the same position).
    const QString lastId = settings->value("player/lastId").toString();
    const int row = m_tracks->rowOf(lastId);
    if (row >= 0 && canPlay(row, false)) {
        for (const QVariant &v : Library::instance()->libraryRows())
            m_queue << m_tracks->at(v.toInt()).id;
        m_queuePos = int(m_queue.indexOf(lastId));
        m_pendingSeek = settings->value("player/lastPosition", 0).toLongLong();
        load(lastId, false);
    }
}

PlayerController::~PlayerController()
{
    saveState();
}

void PlayerController::setupDeck(int d)
{
    Deck &deck = m_decks[d];
    deck.player = new QMediaPlayer(this);
    deck.clock = new QAudioOutput(this);
    deck.clock->setVolume(0.0f);
    deck.player->setAudioOutput(deck.clock);
    deck.buffers = new QAudioBufferOutput(m_engine.bufferFormat(), this);
    deck.player->setAudioBufferOutput(deck.buffers);
    connect(deck.buffers, &QAudioBufferOutput::audioBufferReceived, this,
            [this, d](const QAudioBuffer &buffer) { m_engine.push(d, buffer); });

    connect(deck.player, &QMediaPlayer::playbackStateChanged, this, [this, d] {
        if (d != m_active)
            return;
        m_engine.setPaused(!playing());
        emit playingChanged();
        emit positionChanged();
        ensureTicking();
    });
    connect(deck.player, &QMediaPlayer::mediaStatusChanged, this,
            [this, d](QMediaPlayer::MediaStatus status) { onDeckMediaStatus(d, status); });
    connect(deck.player, &QMediaPlayer::durationChanged, this, [this, d] {
        if (d == m_active)
            emit durationChanged();
    });
    connect(deck.player, &QMediaPlayer::positionChanged, this, [this, d] {
        if (d == m_active && !playing())
            emit positionChanged();
    });
    connect(deck.player, &QMediaPlayer::errorOccurred, this, [this, d](QMediaPlayer::Error, const QString &message) {
        if (d == m_active)
            emit notice(tr("Can't play: %1").arg(message), QStringLiteral("close"));
        else if (!m_engine.inTransition())
            cancelPreload();
    });
}

void PlayerController::saveState() const
{
    auto settings = AppPaths::settings();
    settings->setValue("player/volume", m_volume);
    settings->setValue("player/muted", m_muted);
    settings->setValue("player/shuffle", m_shuffle);
    settings->setValue("player/repeat", m_repeat);
    settings->setValue("player/lastId", m_currentId);
    settings->setValue("player/lastPosition", qint64(position() * 1000));
    settings->sync();
}

// ---- current track info ----------------------------------------------------

const Track *PlayerController::current() const
{
    return m_tracks->isValidRow(m_index) ? &m_tracks->at(m_index) : nullptr;
}

QString PlayerController::title() const
{
    return current() ? current()->title : tr("Nothing playing");
}

QString PlayerController::artist() const
{
    return current() ? current()->artist : tr("Pick a track from the library");
}

QString PlayerController::album() const    { return current() ? current()->album : QString(); }
QString PlayerController::albumKey() const { return current() ? TrackModel::albumKey(*current()) : QString(); }
QString PlayerController::source() const   { return current() ? current()->source : QString(); }
QString PlayerController::quality() const  { return current() ? current()->quality : QString(); }
QString PlayerController::cover() const    { return current() ? current()->cover : QString(); }
QColor PlayerController::colorA() const    { return current() ? current()->colorA : QColor("#9fb5a8"); }
QColor PlayerController::colorB() const    { return current() ? current()->colorB : QColor("#3e4f5a"); }
int PlayerController::seed() const         { return current() ? current()->seed : 0; }
bool PlayerController::liked() const       { return current() && current()->liked; }
bool PlayerController::live() const        { return current() && current()->source == QLatin1String("radio"); }

int PlayerController::duration() const
{
    if (current() && current()->duration > 0)
        return current()->duration;
    return int(player().duration() / 1000);
}

double PlayerController::position() const
{
    if (m_pendingSeek >= 0)
        return m_pendingSeek / 1000.0;
    return player().position() / 1000.0;
}

bool PlayerController::buffering() const
{
    if (!m_decks[m_active].resolving.isEmpty())
        return true;
    const auto s = player().mediaStatus();
    return s == QMediaPlayer::LoadingMedia || s == QMediaPlayer::StalledMedia;
}

QVariantList PlayerController::upNext() const
{
    QVariantList out;
    if (m_queue.isEmpty())
        return out;
    for (int k = 1; k < m_queue.size() && out.size() < 6; ++k) {
        int pos = m_queuePos + k;
        if (pos >= m_queue.size()) {
            if (m_repeat != 1)
                break;
            pos %= m_queue.size();
        }
        const int row = m_tracks->rowOf(m_queue.at(pos));
        if (row >= 0)
            out << row;
    }
    return out;
}

// ---- settings -----------------------------------------------------------------

void PlayerController::setVolume(double v)
{
    v = std::clamp(v, 0.0, 1.0);
    if (qFuzzyCompare(v + 1.0, m_volume + 1.0))
        return;
    m_volume = v;
    applyVolume();
    emit volumeChanged();
}

void PlayerController::applyVolume()
{
    // The slider works on an "audible" scale, the audio output expects a linear one.
    m_engine.setVolume(m_muted ? 0.0f
                               : float(QtAudio::convertVolume(m_volume, QtAudio::LogarithmicVolumeScale,
                                                              QtAudio::LinearVolumeScale)));
}

void PlayerController::setMuted(bool m)
{
    if (m == m_muted)
        return;
    m_muted = m;
    applyVolume();
    emit mutedChanged();
}

void PlayerController::setShuffle(bool s)
{
    if (s == m_shuffle)
        return;
    m_shuffle = s;
    if (!m_engine.inTransition())
        cancelPreload();   // the planned next track may have been picked a different way
    emit shuffleChanged();
}

void PlayerController::setRepeatMode(int r)
{
    r = std::clamp(r, 0, 2);
    if (r == m_repeat)
        return;
    m_repeat = r;
    if (!m_engine.inTransition())
        cancelPreload();
    emit repeatModeChanged();
    emit queueChanged();
}

// ---- controls -----------------------------------------------------------------

bool PlayerController::canPlay(int row, bool explain)
{
    if (!m_tracks->isValidRow(row))
        return false;
    const Track &t = m_tracks->at(row);
    if (t.demo) {
        if (explain) {
            emit notice(t.source == QLatin1String("youtube")
                            ? tr("YouTube Music isn't connected yet — this is a sample track")
                            : tr("Internet radio is coming in a later stage"),
                        t.source == QLatin1String("youtube") ? QStringLiteral("playCircle") : QStringLiteral("radio"));
        }
        return false;
    }
    if (t.source == QLatin1String("youtube")) {
        if (!YouTubeService::instance()->engineReady()) {
            if (explain)
                emit notice(tr("Download the playback engine (yt-dlp) in Sources → YouTube Music"),
                            QStringLiteral("playCircle"));
            return false;
        }
        return true;
    }
    if (!QFileInfo::exists(t.path)) {
        if (explain)
            emit notice(tr("The file doesn't exist: %1").arg(QFileInfo(t.path).fileName()), QStringLiteral("close"));
        return false;
    }
    return true;
}

void PlayerController::load(const QString &id, bool autoplay, bool flushAudio)
{
    const int row = m_tracks->rowOf(id);
    if (row < 0)
        return;

    // A track change by the user interrupts the transition and drops the prepared next track.
    stopTransition();
    cancelPreload();

    if (!m_currentId.isEmpty() && m_currentId != id) {
        m_history << m_currentId;
        if (m_history.size() > 200)
            m_history.removeFirst();
    }
    m_currentId = id;
    m_index = row;
    // On an automatic transition we don't clear the buffer — the end of the previous track plays out smoothly.
    if (flushAudio) {
        m_engine.flush();
        m_analyzer.clear();
    }
    m_engine.setActiveDeck(m_active);
    m_engine.resetLevels(m_active);
    m_silence = 0;

    Deck &deck = m_decks[m_active];
    deck.trackId = id;
    const Track &t = m_tracks->at(row);
    if (t.source == QLatin1String("youtube")) {
        deck.player->stop();
        deck.player->setSource(QUrl());
        deck.resolving = t.path;
        m_autoplayAfterResolve = autoplay;
        YouTubeService::instance()->resolver()->resolve(t.path);
        Library::instance()->ensurePalette(row);
    } else {
        deck.resolving.clear();
        deck.player->setSource(QUrl::fromLocalFile(t.path));
        if (autoplay)
            deck.player->play();
    }

    emitTrackChanged();
    emit bufferingChanged();
}

void PlayerController::emitTrackChanged()
{
    LyricsService::instance()->setTrack(current());
    emit currentChanged();
    emit colorsChanged();
    emit durationChanged();
    emit likedChanged();
    emit positionChanged();
    emit queueChanged();
}

void PlayerController::prefetchNext()
{
    // We prepare the URL of the next YouTube track in advance — the transition is then instant.
    if (m_queue.isEmpty())
        return;
    int next;
    if (m_shuffle && m_queue.size() > 1) {
        // With shuffle we pick the next track right away, so there's time to prepare its URL.
        if (!m_queue.contains(m_shuffleNextId) || m_shuffleNextId == m_currentId)
            m_shuffleNextId = m_queue.at(nextQueuePos());
        next = int(m_queue.indexOf(m_shuffleNextId));
    } else {
        next = m_queuePos + 1 < m_queue.size() ? m_queuePos + 1 : (m_repeat == 1 ? 0 : -1);
    }
    if (next < 0)
        return;
    const int row = m_tracks->rowOf(m_queue.at(next));
    if (row >= 0 && m_tracks->at(row).source == QLatin1String("youtube") && !m_tracks->at(row).demo)
        YouTubeService::instance()->resolver()->prefetch(m_tracks->at(row).path);
}

void PlayerController::prefetch(int row)
{
    if (m_tracks->isValidRow(row) && m_tracks->at(row).source == QLatin1String("youtube") && !m_tracks->at(row).demo)
        YouTubeService::instance()->resolver()->prefetch(m_tracks->at(row).path);
}

void PlayerController::playFrom(const QVariantList &rows, int row, const QString &context)
{
    if (!canPlay(row, true))
        return;

    const QString id = m_tracks->at(row).id;
    QStringList queue;
    for (const QVariant &v : rows) {
        const int r = v.toInt();
        if (m_tracks->isValidRow(r) && !m_tracks->at(r).demo)
            queue << m_tracks->at(r).id;
    }
    if (!queue.contains(id))
        queue.prepend(id);
    m_queue = queue;
    m_queuePos = int(m_queue.indexOf(id));
    if (context != m_context) {
        m_context = context;
        emit mixChanged();
    }

    if (id == m_currentId) {
        if (!m_engine.inTransition())
            cancelPreload();
        if (!playing())
            player().play();
        emit queueChanged();
        return;
    }
    m_pendingSeek = -1;
    load(id, true);
}

void PlayerController::playIndex(int row)
{
    playFrom(Library::instance()->libraryRows(), row, QStringLiteral("library"));
}

void PlayerController::jumpTo(int row)
{
    if (!canPlay(row, true))
        return;
    const QString id = m_tracks->at(row).id;
    const int pos = int(m_queue.indexOf(id));
    if (pos < 0) {
        playIndex(row);
        return;
    }
    m_queuePos = pos;
    m_pendingSeek = -1;
    load(id, true);
}

void PlayerController::play()
{
    if (!hasTrack()) {
        const QVariantList local = Library::instance()->libraryRows();
        if (!local.isEmpty())
            playFrom(local, local.first().toInt(), QStringLiteral("library"));
        else
            emit notice(tr("Add a music folder to start listening"), QStringLiteral("folder"));
        return;
    }
    if (!m_decks[m_active].resolving.isEmpty()) {
        m_autoplayAfterResolve = true;
        return;
    }
    if (m_engine.inTransition())
        m_decks[standby()].player->play();   // the previous track still playing out
    player().play();
}

void PlayerController::pause()
{
    m_autoplayAfterResolve = false;
    if (m_engine.inTransition())
        m_decks[standby()].player->pause();
    player().pause();
}

void PlayerController::toggle()
{
    const bool active = playing() || (!m_decks[m_active].resolving.isEmpty() && m_autoplayAfterResolve);
    active ? pause() : play();
}

void PlayerController::next() { advance(true); }

int PlayerController::nextQueuePos() const
{
    if (m_queue.isEmpty())
        return -1;
    if (m_shuffle && m_queue.size() > 1) {
        if (const int planned = int(m_queue.indexOf(m_shuffleNextId)); planned >= 0 && planned != m_queuePos)
            return planned;   // the one whose URL we've already prepared
        int pos;
        do {
            pos = int(QRandomGenerator::global()->bounded(int(m_queue.size())));
        } while (pos == m_queuePos);
        return pos;
    }
    const int pos = m_queuePos + 1;
    if (pos < m_queue.size())
        return pos;
    return m_repeat == 0 ? -1 : 0;
}

void PlayerController::advance(bool userInitiated)
{
    if (m_queue.isEmpty())
        return;

    int pos;
    if (!userInitiated && m_plannedPos >= 0 && m_plannedPos < m_queue.size()) {
        pos = m_plannedPos;   // the same choice that was prepared for the mix (matters with shuffle)
    } else if (m_shuffle && m_queue.size() > 1) {
        pos = nextQueuePos();
    } else {
        pos = m_queuePos + 1;
        if (pos >= m_queue.size()) {
            if (!userInitiated && m_repeat == 0) {
                // End of the queue — stop at the start of the last track.
                player().pause();
                player().setPosition(0);
                emit positionChanged();
                return;
            }
            pos = 0;
        }
    }
    m_queuePos = pos;
    m_pendingSeek = -1;
    m_shuffleNextId.clear();
    load(m_queue.at(pos), true, userInitiated);
}

void PlayerController::previous()
{
    if (position() > 3.0 || m_queue.isEmpty()) {
        seek(0.0);
        return;
    }
    QString id;
    if (m_shuffle && !m_history.isEmpty()) {
        id = m_history.takeLast();
        m_queuePos = int(m_queue.indexOf(id));
    } else {
        m_queuePos = (m_queuePos - 1 + int(m_queue.size())) % int(m_queue.size());
        id = m_queue.at(m_queuePos);
    }
    const QString before = m_currentId;
    m_pendingSeek = -1;
    load(id, true);
    if (!m_history.isEmpty() && m_history.last() == before && m_shuffle)
        m_history.removeLast();   // going back shouldn't add to the history
}

void PlayerController::seek(double seconds)
{
    if (!hasTrack())
        return;
    stopTransition();
    const qint64 ms = qint64(std::max(0.0, seconds) * 1000);
    if (player().mediaStatus() == QMediaPlayer::LoadingMedia || player().mediaStatus() == QMediaPlayer::NoMedia)
        m_pendingSeek = ms;
    else
        player().setPosition(ms);
    m_engine.flush();
    m_engine.setActiveDeck(m_active);
    // Seeking before the ending: the prepared next track stays, but silence detection starts over.
    m_silence = 0;
    emit positionChanged();
    emit seeked(ms / 1000.0);
}

void PlayerController::cycleRepeat()
{
    setRepeatMode((m_repeat + 1) % 3);
}

void PlayerController::toggleLiked()
{
    m_tracks->toggleLiked(m_index);
}

void PlayerController::onDeckMediaStatus(int d, QMediaPlayer::MediaStatus status)
{
    if (d != m_active) {
        // The other deck: either the prepared next track or the previous one playing out.
        if (status == QMediaPlayer::EndOfMedia && m_engine.inTransition())
            m_engine.setDeckEnded(d);
        else if (status == QMediaPlayer::InvalidMedia && !m_engine.inTransition())
            cancelPreload();
        return;
    }
    emit bufferingChanged();
    switch (status) {
    case QMediaPlayer::LoadedMedia:
    case QMediaPlayer::BufferedMedia:
        if (m_pendingSeek >= 0) {
            player().setPosition(m_pendingSeek);
            m_pendingSeek = -1;
            emit positionChanged();
        }
        prefetchNext();
        break;
    case QMediaPlayer::EndOfMedia:
        if (m_repeat == 2) {
            player().setPosition(0);
            player().play();
        } else {
            advance(false);
        }
        break;
    case QMediaPlayer::InvalidMedia:
        emit notice(tr("Unsupported or damaged file"), QStringLiteral("close"));
        break;
    default:
        break;
    }
}

void PlayerController::onModelReset()
{
    // After a rescan the rows may have moved — we look tracks up by their ids.
    m_index = m_tracks->rowOf(m_currentId);
    if (m_index < 0 && !m_currentId.isEmpty()) {
        stopTransition();
        player().stop();
        player().setSource(QUrl());
        m_currentId.clear();
        m_decks[m_active].resolving.clear();
    }
    if (!m_plannedId.isEmpty() && m_tracks->rowOf(m_plannedId) < 0 && !m_engine.inTransition())
        cancelPreload();
    m_queue.erase(std::remove_if(m_queue.begin(), m_queue.end(),
                                 [this](const QString &id) { return m_tracks->rowOf(id) < 0; }),
                  m_queue.end());
    m_queuePos = int(m_queue.indexOf(m_currentId));
    if (!m_plannedId.isEmpty())
        m_plannedPos = int(m_queue.indexOf(m_plannedId));
    m_history.erase(std::remove_if(m_history.begin(), m_history.end(),
                                   [this](const QString &id) { return m_tracks->rowOf(id) < 0; }),
                    m_history.end());

    LyricsService::instance()->setTrack(current());
    emit currentChanged();
    emit colorsChanged();
    emit durationChanged();
    emit likedChanged();
    emit queueChanged();
}

// ---- mixing -------------------------------------------------------------------

QStringList PlayerController::mixStyles() const
{
    return kMixStyles;
}

QString PlayerController::mixStyleName(const QString &style) const
{
    if (style == QLatin1String("fade"))
        return tr("Crossfade");
    if (style == QLatin1String("rise"))
        return tr("Rise");
    if (style == QLatin1String("bass"))
        return tr("Bass swap");
    if (style == QLatin1String("echo"))
        return tr("Echo");
    return tr("Automatic");
}

QString PlayerController::mixStyleDescription(const QString &style) const
{
    if (style == QLatin1String("fade"))
        return tr("Classic constant-power crossfade.");
    if (style == QLatin1String("rise"))
        return tr("A filter gradually cuts the lows of the ending track while the next one comes in ever fuller.");
    if (style == QLatin1String("bass"))
        return tr("Like a DJ: the highs crossfade and the bass switches halfway — never two kick drums at once.");
    if (style == QLatin1String("echo"))
        return tr("The ending track fades away into an echo and the next one emerges from it.");
    return tr("A transition picked for the track's ending: fade-outs and silence are detected, energetic endings get a bass swap.");
}

void PlayerController::loadMixSettings()
{
    auto settings = AppPaths::settings();
    m_crossfade = std::clamp(settings->value("mix/crossfade", 0).toInt(), 0, 12);
    m_crossfadeStyle = settings->value("mix/style", QStringLiteral("fade")).toString();
    if (!kMixStyles.contains(m_crossfadeStyle))
        m_crossfadeStyle = QStringLiteral("fade");
    m_smart = settings->value("mix/smart", true).toBool();
    const QJsonObject mixes = QJsonDocument::fromJson(settings->value("mix/playlists").toString().toUtf8()).object();
    for (auto it = mixes.begin(); it != mixes.end(); ++it) {
        const QJsonObject o = it.value().toObject();
        MixConfig c;
        c.enabled = o.value("enabled").toBool();
        c.style = kMixStyles.contains(o.value("style").toString()) ? o.value("style").toString() : QStringLiteral("auto");
        c.length = std::clamp(o.value("length").toInt(8), 2, 16);
        m_mixes.insert(it.key(), c);
    }
}

void PlayerController::saveMixSettings()
{
    auto settings = AppPaths::settings();
    settings->setValue("mix/crossfade", m_crossfade);
    settings->setValue("mix/style", m_crossfadeStyle);
    settings->setValue("mix/smart", m_smart);
    QJsonObject mixes;
    for (auto it = m_mixes.cbegin(); it != m_mixes.cend(); ++it)
        mixes.insert(it.key(), QJsonObject { { "enabled", it->enabled }, { "style", it->style }, { "length", it->length } });
    settings->setValue("mix/playlists", QString::fromUtf8(QJsonDocument(mixes).toJson(QJsonDocument::Compact)));
    ++m_mixRevision;
    emit mixChanged();
}

void PlayerController::setCrossfade(int seconds)
{
    seconds = std::clamp(seconds, 0, 12);
    if (seconds == m_crossfade)
        return;
    m_crossfade = seconds;
    saveMixSettings();
}

void PlayerController::setCrossfadeStyle(const QString &style)
{
    if (style == m_crossfadeStyle || !kMixStyles.contains(style))
        return;
    m_crossfadeStyle = style;
    saveMixSettings();
}

void PlayerController::setSmartTransitions(bool on)
{
    if (on == m_smart)
        return;
    m_smart = on;
    saveMixSettings();
}

QVariantMap PlayerController::mixFor(const QString &context) const
{
    const MixConfig c = m_mixes.value(context);
    return { { "enabled", c.enabled }, { "style", c.style }, { "length", c.length } };
}

void PlayerController::setMix(const QString &context, bool enabled, const QString &style, int length)
{
    if (context.isEmpty())
        return;
    MixConfig c;
    c.enabled = enabled;
    c.style = kMixStyles.contains(style) ? style : QStringLiteral("auto");
    c.length = std::clamp(length, 2, 16);
    m_mixes.insert(context, c);
    saveMixSettings();
}

PlayerController::MixConfig PlayerController::effectiveMix() const
{
    // The playlist's mix takes precedence over the global crossfade.
    const auto it = m_mixes.constFind(m_context);
    if (it != m_mixes.cend() && it->enabled)
        return *it;
    MixConfig global;
    global.enabled = m_crossfade > 0;
    global.style = m_crossfadeStyle;
    global.length = m_crossfade;
    return global;
}

void PlayerController::preloadNext()
{
    const int pos = nextQueuePos();
    if (pos < 0)
        return;
    const QString id = m_queue.at(pos);
    const int row = m_tracks->rowOf(id);
    if (row < 0 || id == m_currentId || !canPlay(row, false))
        return;

    const int d = standby();
    Deck &deck = m_decks[d];
    deck.player->stop();
    m_engine.dropDeck(d);
    m_engine.resetLevels(d);
    deck.trackId = id;
    m_plannedPos = pos;
    m_plannedId = id;

    const Track &t = m_tracks->at(row);
    if (t.source == QLatin1String("youtube")) {
        StreamResolver *resolver = YouTubeService::instance()->resolver();
        const QUrl url = resolver->cachedUrl(t.path);
        if (url.isValid()) {
            deck.resolving.clear();
            deck.player->setSource(url);
        } else {
            deck.player->setSource(QUrl());
            deck.resolving = t.path;
            resolver->resolve(t.path);
        }
    } else {
        deck.resolving.clear();
        deck.player->setSource(QUrl::fromLocalFile(t.path));
    }
}

void PlayerController::cancelPreload()
{
    Deck &deck = m_decks[standby()];
    if (deck.trackId.isEmpty() && m_plannedPos < 0)
        return;
    deck.player->stop();
    deck.player->setSource(QUrl());
    deck.resolving.clear();
    deck.trackId.clear();
    m_engine.dropDeck(standby());
    m_plannedPos = -1;
    m_plannedId.clear();
}

bool PlayerController::standbyReady() const
{
    const Deck &deck = m_decks[standby()];
    if (deck.trackId.isEmpty() || !deck.resolving.isEmpty())
        return false;
    const auto s = deck.player->mediaStatus();
    return s == QMediaPlayer::LoadedMedia || s == QMediaPlayer::BufferedMedia;
}

void PlayerController::updateMix(double dt)
{
    if (m_engine.inTransition()) {
        emit transitionChanged();
        return;
    }
    const MixConfig mix = effectiveMix();
    if (!mix.enabled || !playing() || m_repeat == 2 || live() || !m_decks[m_active].resolving.isEmpty())
        return;
    const double total = player().duration() > 0 ? player().duration() / 1000.0 : double(duration());
    if (total <= 0)
        return;
    const double pos = position();
    const double remaining = total - pos;
    const double length = std::min<double>(mix.length, std::max(2.0, total / 3));

    if (remaining <= length + 12 && m_plannedPos < 0)
        preloadNext();
    if (m_plannedPos < 0 || !standbyReady())
        return;

    // Smart transitions: silence at the end of the recording and fade-outs recorded in the track itself.
    if (m_smart && pos > 20 && remaining < 25) {
        const double level = m_engine.shortLevelDb(m_active);
        const double average = m_engine.averageLevelDb(m_active);
        m_silence = level < -50 ? m_silence + dt : 0;
        // A fade-out in the recording: there's still sound, but clearly quieter than the track's average (for ≥ 1 s).
        m_quiet = level >= -50 && level < average - 14 ? m_quiet + dt : 0;
        if (m_silence > 0.8) {
            // The old track is already silent — the next one comes in at once, without the quiet intro.
            startTransition(AudioEngine::Style::Fade, 0.3, tr("Skipped silence"));
            return;
        }
        if (m_quiet > 1.0 && remaining < length + 6 && remaining > 1.5) {
            startTransition(AudioEngine::Style::Fade, std::clamp(remaining, 2.0, length),
                            tr("Crossfade"));
            return;
        }
    }
    if (remaining > length)
        return;

    AudioEngine::Style style = styleFromId(mix.style);
    QString label = mixStyleName(mix.style);
    if (mix.style == QLatin1String("auto")) {
        const bool energetic = m_engine.shortLevelDb(m_active) > m_engine.averageLevelDb(m_active) - 4;
        style = energetic ? AudioEngine::Style::BassSwap : AudioEngine::Style::Fade;
        label = energetic ? tr("Bass swap") : tr("Crossfade");
    }
    startTransition(style, std::max(1.0, remaining), label);
}

void PlayerController::startTransition(AudioEngine::Style style, double seconds, const QString &label)
{
    const int from = m_active;
    const int to = standby();
    const QString id = m_plannedId;
    const int row = m_tracks->rowOf(id);
    if (row < 0) {
        cancelPreload();
        return;
    }

    m_engine.resetLevels(to);
    m_engine.beginTransition(from, to, { style, seconds, m_smart });
    m_decks[to].player->play();

    if (!m_currentId.isEmpty()) {
        m_history << m_currentId;
        if (m_history.size() > 200)
            m_history.removeFirst();
    }
    m_active = to;
    m_currentId = id;
    m_index = row;
    m_queuePos = m_plannedPos;
    m_plannedPos = -1;
    m_plannedId.clear();
    m_pendingSeek = -1;
    m_silence = 0;
    m_quiet = 0;
    m_transitionLabel = label;

    emitTrackChanged();
    emit playingChanged();
    emit bufferingChanged();
    emit transitionChanged();
}

void PlayerController::stopTransition()
{
    if (m_engine.inTransition())
        m_engine.finishTransitionNow();   // the transitionFinished signal will stop the old deck
}

// ---- spectrum -----------------------------------------------------------------

void PlayerController::ensureTicking()
{
    if (!m_timer.isActive()) {
        m_clock.restart();
        m_timer.start();
    }
}

void PlayerController::tick()
{
    const double dt = std::min(0.1, m_clock.restart() / 1000.0);
    const bool active = playing();

    // Samples that actually played (after EQ) -> spectrum analyzer.
    m_engine.takePlayed(m_played);
    if (!m_played.empty())
        m_analyzer.pushSamples(m_played.data(), m_played.size(), m_engine.sampleRate());
    Equalizer::instance()->tickMeters(dt);
    updateMix(dt);

    if (active) {
        m_analyzer.compute(m_targets);
        emit positionChanged();
    } else {
        std::fill(m_targets.begin(), m_targets.end(), 0.0);
    }

    // Fast attack, slow decay — the bars "jump" to the beat and gently fade out.
    qreal sum = 0.0, peak = 0.0;
    for (int i = 0; i < kBars; ++i) {
        const qreal rate = m_targets[i] > m_spectrum[i] ? 0.6 : 0.13;
        const qreal k = std::min(1.0, rate * dt * 60.0);
        m_spectrum[i] += (m_targets[i] - m_spectrum[i]) * k;
        peak = std::max(peak, m_spectrum[i]);
        if (i < 10)
            sum += m_spectrum[i];
    }
    m_level = sum / 10.0;
    emit spectrumChanged();

    if (!active && peak < 0.002 && !m_engine.inTransition()) {
        std::fill(m_spectrum.begin(), m_spectrum.end(), 0.0);
        m_level = 0.0;
        emit spectrumChanged();
        m_timer.stop();
    }
}
