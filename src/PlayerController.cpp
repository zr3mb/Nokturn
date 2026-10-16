#include "PlayerController.h"

#include "AppPaths.h"
#include "Library.h"
#include "TrackModel.h"
#include "audio/Equalizer.h"
#include "youtube/YouTubeService.h"

#include <QAudioFormat>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QtMultimedia/qtaudio.h>

#include <algorithm>

PlayerController::PlayerController(QObject *parent)
    : QObject(parent)
    , m_tracks(Library::instance()->tracks())
    , m_analyzer(kBars)
    , m_spectrum(kBars, 0.0)
    , m_targets(kBars, 0.0)
{
    // Odtwarzacz gra do wyciszonego wyjścia (to ono wyznacza tempo odtwarzania), a słyszalny
    // dźwięk idzie osobnym torem: próbki -> korektor -> AudioEngine (QAudioSink).
    m_output.setVolume(0.0f);
    m_player.setAudioOutput(&m_output);
    m_bufferOutput = new QAudioBufferOutput(m_engine.bufferFormat(), this);
    m_player.setAudioBufferOutput(m_bufferOutput);
    connect(m_bufferOutput, &QAudioBufferOutput::audioBufferReceived, this,
            [this](const QAudioBuffer &buffer) { m_engine.push(buffer); });

    auto settings = AppPaths::settings();
    m_volume = std::clamp(settings->value("player/volume", 0.72).toDouble(), 0.0, 1.0);
    m_muted = settings->value("player/muted", false).toBool();
    m_shuffle = settings->value("player/shuffle", false).toBool();
    m_repeat = std::clamp(settings->value("player/repeat", 0).toInt(), 0, 2);
    applyVolume();

    connect(&m_player, &QMediaPlayer::playbackStateChanged, this, [this] {
        m_engine.setPaused(!playing());
        emit playingChanged();
        emit positionChanged();
        ensureTicking();
    });
    connect(&m_player, &QMediaPlayer::mediaStatusChanged, this, &PlayerController::onMediaStatus);
    connect(&m_player, &QMediaPlayer::durationChanged, this, &PlayerController::durationChanged);
    connect(&m_player, &QMediaPlayer::positionChanged, this, [this] {
        if (!playing())
            emit positionChanged();
    });
    connect(&m_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString &message) {
        emit notice(QStringLiteral("Nie można odtworzyć: ") + message, QStringLiteral("close"));
    });

    connect(m_tracks, &QAbstractItemModel::modelReset, this, &PlayerController::onModelReset);
    connect(m_tracks, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &from, const QModelIndex &to) {
        if (m_index >= from.row() && m_index <= to.row())
            emit colorsChanged();   // np. paleta wyliczona z okładki pobranej z sieci
    });

    // Utwory z YouTube: adres strumienia dostarcza yt-dlp (asynchronicznie).
    StreamResolver *resolver = YouTubeService::instance()->resolver();
    connect(resolver, &StreamResolver::resolved, this, [this](const QString &videoId, const QUrl &url) {
        if (videoId != m_resolvingId)
            return;   // np. wynik prefetchu albo utwór już zmieniony
        m_resolvingId.clear();
        m_player.setSource(url);
        if (m_autoplayAfterResolve)
            m_player.play();
        emit bufferingChanged();
    });
    connect(resolver, &StreamResolver::failed, this, [this](const QString &videoId, const QString &error) {
        if (videoId != m_resolvingId)
            return;
        m_resolvingId.clear();
        emit bufferingChanged();
        emit notice(QStringLiteral("Nie udało się odtworzyć z YouTube: ") + error, QStringLiteral("close"));
    });
    connect(m_tracks, &TrackModel::likedChanged, this, [this](int row, bool) {
        if (row == m_index)
            emit likedChanged();
    });

    m_timer.setInterval(16);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &PlayerController::tick);
    m_clock.start();

    // Przywróć ostatnio słuchany utwór (wstrzymany, w tym samym miejscu).
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

// ---- informacje o bieżącym utworze ------------------------------------------

const Track *PlayerController::current() const
{
    return m_tracks->isValidRow(m_index) ? &m_tracks->at(m_index) : nullptr;
}

QString PlayerController::title() const
{
    return current() ? current()->title : QStringLiteral("Nic nie gra");
}

QString PlayerController::artist() const
{
    return current() ? current()->artist : QStringLiteral("Wybierz utwór z biblioteki");
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
    return int(m_player.duration() / 1000);
}

double PlayerController::position() const
{
    if (m_pendingSeek >= 0)
        return m_pendingSeek / 1000.0;
    return m_player.position() / 1000.0;
}

bool PlayerController::buffering() const
{
    if (!m_resolvingId.isEmpty())
        return true;
    const auto s = m_player.mediaStatus();
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

// ---- ustawienia ---------------------------------------------------------------

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
    // Suwak działa w skali "słyszalnej", wyjście audio oczekuje liniowej.
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
    emit shuffleChanged();
}

void PlayerController::setRepeatMode(int r)
{
    r = std::clamp(r, 0, 2);
    if (r == m_repeat)
        return;
    m_repeat = r;
    emit repeatModeChanged();
    emit queueChanged();
}

// ---- sterowanie ---------------------------------------------------------------

bool PlayerController::canPlay(int row, bool explain)
{
    if (!m_tracks->isValidRow(row))
        return false;
    const Track &t = m_tracks->at(row);
    if (t.demo) {
        if (explain) {
            emit notice(t.source == QLatin1String("youtube")
                            ? QStringLiteral("YouTube Music nie jest jeszcze podłączony — to przykładowy utwór")
                            : QStringLiteral("Radio internetowe pojawi się w kolejnym etapie"),
                        t.source == QLatin1String("youtube") ? QStringLiteral("playCircle") : QStringLiteral("radio"));
        }
        return false;
    }
    if (t.source == QLatin1String("youtube")) {
        if (!YouTubeService::instance()->engineReady()) {
            if (explain)
                emit notice(QStringLiteral("Pobierz silnik odtwarzania (yt-dlp) w Źródła → YouTube Music"),
                            QStringLiteral("playCircle"));
            return false;
        }
        return true;
    }
    if (!QFileInfo::exists(t.path)) {
        if (explain)
            emit notice(QStringLiteral("Plik nie istnieje: ") + QFileInfo(t.path).fileName(), QStringLiteral("close"));
        return false;
    }
    return true;
}

void PlayerController::load(const QString &id, bool autoplay, bool flushAudio)
{
    const int row = m_tracks->rowOf(id);
    if (row < 0)
        return;

    if (!m_currentId.isEmpty() && m_currentId != id) {
        m_history << m_currentId;
        if (m_history.size() > 200)
            m_history.removeFirst();
    }
    m_currentId = id;
    m_index = row;
    // Przy automatycznym przejściu nie czyścimy bufora — końcówka poprzedniego utworu dogra się płynnie.
    if (flushAudio) {
        m_engine.flush();
        m_analyzer.clear();
    }

    const Track &t = m_tracks->at(row);
    if (t.source == QLatin1String("youtube")) {
        m_player.stop();
        m_player.setSource(QUrl());
        m_resolvingId = t.path;
        m_autoplayAfterResolve = autoplay;
        YouTubeService::instance()->resolver()->resolve(t.path);
        Library::instance()->ensurePalette(row);
    } else {
        m_resolvingId.clear();
        m_player.setSource(QUrl::fromLocalFile(t.path));
        if (autoplay)
            m_player.play();
    }

    emitTrackChanged();
    emit bufferingChanged();
}

void PlayerController::emitTrackChanged()
{
    emit currentChanged();
    emit colorsChanged();
    emit durationChanged();
    emit likedChanged();
    emit positionChanged();
    emit queueChanged();
}

void PlayerController::prefetchNext()
{
    // Adres następnego utworu z YouTube przygotowujemy zawczasu — przejście jest wtedy natychmiastowe.
    if (m_queue.isEmpty() || m_shuffle)
        return;
    const int next = m_queuePos + 1 < m_queue.size() ? m_queuePos + 1 : (m_repeat == 1 ? 0 : -1);
    if (next < 0)
        return;
    const int row = m_tracks->rowOf(m_queue.at(next));
    if (row >= 0 && m_tracks->at(row).source == QLatin1String("youtube") && !m_tracks->at(row).demo)
        YouTubeService::instance()->resolver()->prefetch(m_tracks->at(row).path);
}

void PlayerController::playFrom(const QVariantList &rows, int row)
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

    if (id == m_currentId) {
        if (!playing())
            m_player.play();
        emit queueChanged();
        return;
    }
    m_pendingSeek = -1;
    load(id, true);
}

void PlayerController::playIndex(int row)
{
    playFrom(Library::instance()->libraryRows(), row);
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
            playFrom(local, local.first().toInt());
        else
            emit notice(QStringLiteral("Dodaj folder z muzyką, żeby zacząć słuchać"), QStringLiteral("folder"));
        return;
    }
    if (!m_resolvingId.isEmpty()) {
        m_autoplayAfterResolve = true;
        return;
    }
    m_player.play();
}

void PlayerController::pause()
{
    m_autoplayAfterResolve = false;
    m_player.pause();
}
void PlayerController::toggle()
{
    const bool active = playing() || (!m_resolvingId.isEmpty() && m_autoplayAfterResolve);
    active ? pause() : play();
}
void PlayerController::next()   { advance(true); }

void PlayerController::advance(bool userInitiated)
{
    if (m_queue.isEmpty())
        return;

    int pos = m_queuePos + 1;
    if (m_shuffle && m_queue.size() > 1) {
        do {
            pos = int(QRandomGenerator::global()->bounded(int(m_queue.size())));
        } while (pos == m_queuePos);
    } else if (pos >= m_queue.size()) {
        if (!userInitiated && m_repeat == 0) {
            // Koniec kolejki — zatrzymaj się na początku ostatniego utworu.
            m_player.pause();
            m_player.setPosition(0);
            emit positionChanged();
            return;
        }
        pos = 0;
    }
    m_queuePos = pos;
    m_pendingSeek = -1;
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
        m_history.removeLast();   // cofanie się nie powinno dopisywać historii
}

void PlayerController::seek(double seconds)
{
    if (!hasTrack())
        return;
    const qint64 ms = qint64(std::max(0.0, seconds) * 1000);
    if (m_player.mediaStatus() == QMediaPlayer::LoadingMedia || m_player.mediaStatus() == QMediaPlayer::NoMedia)
        m_pendingSeek = ms;
    else
        m_player.setPosition(ms);
    m_engine.flush();
    emit positionChanged();
}

void PlayerController::cycleRepeat()
{
    setRepeatMode((m_repeat + 1) % 3);
}

void PlayerController::toggleLiked()
{
    m_tracks->toggleLiked(m_index);
}

void PlayerController::onMediaStatus(QMediaPlayer::MediaStatus status)
{
    emit bufferingChanged();
    switch (status) {
    case QMediaPlayer::LoadedMedia:
    case QMediaPlayer::BufferedMedia:
        if (m_pendingSeek >= 0) {
            m_player.setPosition(m_pendingSeek);
            m_pendingSeek = -1;
            emit positionChanged();
        }
        prefetchNext();
        break;
    case QMediaPlayer::EndOfMedia:
        if (m_repeat == 2) {
            m_player.setPosition(0);
            m_player.play();
        } else {
            advance(false);
        }
        break;
    case QMediaPlayer::InvalidMedia:
        emit notice(QStringLiteral("Nieobsługiwany lub uszkodzony plik"), QStringLiteral("close"));
        break;
    default:
        break;
    }
}

void PlayerController::onModelReset()
{
    // Po ponownym skanowaniu wiersze mogły się przesunąć — szukamy utworów po identyfikatorach.
    m_index = m_tracks->rowOf(m_currentId);
    if (m_index < 0 && !m_currentId.isEmpty()) {
        m_player.stop();
        m_player.setSource(QUrl());
        m_currentId.clear();
        m_resolvingId.clear();
    }
    m_queue.erase(std::remove_if(m_queue.begin(), m_queue.end(),
                                 [this](const QString &id) { return m_tracks->rowOf(id) < 0; }),
                  m_queue.end());
    m_queuePos = int(m_queue.indexOf(m_currentId));
    m_history.erase(std::remove_if(m_history.begin(), m_history.end(),
                                   [this](const QString &id) { return m_tracks->rowOf(id) < 0; }),
                    m_history.end());

    emit currentChanged();
    emit colorsChanged();
    emit durationChanged();
    emit likedChanged();
    emit queueChanged();
}

// ---- widmo --------------------------------------------------------------------

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

    // Próbki, które faktycznie zagrały (po korekcji) -> analizator widma.
    m_engine.takePlayed(m_played);
    if (!m_played.empty())
        m_analyzer.pushSamples(m_played.data(), m_played.size(), m_engine.sampleRate());
    Equalizer::instance()->tickMeters(dt);

    if (active) {
        m_analyzer.compute(m_targets);
        emit positionChanged();
    } else {
        std::fill(m_targets.begin(), m_targets.end(), 0.0);
    }

    // Szybki atak, wolne opadanie — słupki "skaczą" w rytm i łagodnie gasną.
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

    if (!active && peak < 0.002) {
        std::fill(m_spectrum.begin(), m_spectrum.end(), 0.0);
        m_level = 0.0;
        emit spectrumChanged();
        m_timer.stop();
    }
}
