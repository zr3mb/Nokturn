#include "LyricsService.h"

#include "AppPaths.h"
#include "LyricsAligner.h"
#include "TagLibPath.h"
#include "youtube/YouTubeService.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJSEngine>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringDecoder>
#include <QtConcurrent/QtConcurrentRun>

#include <taglib/fileref.h>
#include <taglib/tpropertymap.h>

#include <algorithm>

namespace {

constexpr qint64 kMissingRetryMs = 3LL * 24 * 3600 * 1000;   // brak tekstu sprawdzamy ponownie po 3 dniach

// Tekst z pliku: najpierw .lrc obok utworu (może mieć czasy), potem tag LYRICS / UNSYNCEDLYRICS.
// Wykonywane w wątku roboczym.
Lyrics readLocalLyrics(const QString &path)
{
    const QFileInfo fi(path);
    for (const QString &ext : { QStringLiteral("lrc"), QStringLiteral("LRC") }) {
        QFile lrc(fi.absolutePath() + QChar('/') + fi.completeBaseName() + QChar('.') + ext);
        if (lrc.open(QIODevice::ReadOnly)) {
            const QByteArray data = lrc.read(1024 * 1024);
            QStringDecoder utf8(QStringDecoder::Utf8);
            QString text = utf8(data);
            if (utf8.hasError())
                text = QString::fromLocal8Bit(data);
            Lyrics l = Lyrics::fromLrc(text);
            if (!l.isEmpty()) {
                l.source = QStringLiteral("Plik ") + QFileInfo(lrc).fileName();
                l.origin = QStringLiteral("local");
                return l;
            }
        }
    }

    const TagLibPath native(path);
    TagLib::FileRef ref(native.get(), false);
    if (ref.isNull())
        return {};
    const TagLib::PropertyMap props = ref.properties();
    for (const char *key : { "LYRICS", "UNSYNCEDLYRICS" }) {
        const auto it = props.find(key);
        if (it == props.end() || it->second.isEmpty())
            continue;
        const QString text = QString::fromStdWString(it->second.front().toWString());
        Lyrics l = Lyrics::fromLrc(text);   // niektóre programy zapisują w tagu format LRC
        if (!l.isEmpty()) {
            l.source = QStringLiteral("Tag w pliku");
            l.origin = QStringLiteral("local");
            return l;
        }
    }
    return {};
}

} // namespace

LyricsService *LyricsService::instance()
{
    static LyricsService *inst = new LyricsService;
    return inst;
}

LyricsService *LyricsService::create(QQmlEngine *, QJSEngine *)
{
    LyricsService *s = instance();
    QJSEngine::setObjectOwnership(s, QJSEngine::CppOwnership);
    return s;
}

LyricsService::LyricsService(QObject *parent)
    : QObject(parent)
    , m_hardware(SpeechEngine::detectHardware())
{
    auto settings = AppPaths::settings();
    m_model = settings->value("lyrics/model", m_hardware.recommendedModel).toString();
    if (!SpeechEngine::modelNames().contains(m_model))
        m_model = m_hardware.recommendedModel;
    m_autoAlign = m_hardware.sufficient && settings->value("lyrics/autoAlign", false).toBool();

    connect(&m_speech, &SpeechEngine::stateChanged, this, [this] {
        emit sttChanged();
        emit alignChanged();
    });
    connect(&m_speech, &SpeechEngine::notice, this, &LyricsService::notice);
    connect(&m_speech, &SpeechEngine::progress, this, [this](qreal value, const QString &stage) {
        m_alignProgress = value;
        m_alignStage = stage;
        emit alignChanged();
    });
    // Po pobraniu silnika lub modelu od razu dopasuj bieżący utwór (jeśli funkcja jest włączona).
    connect(&m_speech, &SpeechEngine::stateChanged, this, &LyricsService::maybeAutoAlign, Qt::QueuedConnection);

    StreamResolver *resolver = YouTubeService::instance()->resolver();
    connect(resolver, &StreamResolver::resolved, this, [this](const QString &videoId, const QUrl &url) {
        if (videoId != m_waitingStream)
            return;
        m_waitingStream.clear();
        startAlign(url);
    });
    connect(resolver, &StreamResolver::failed, this, [this](const QString &videoId, const QString &) {
        if (videoId != m_waitingStream)
            return;
        m_waitingStream.clear();
        emit alignChanged();
    });
}

// ---- ustawienia -----------------------------------------------------------------

void LyricsService::setAutoAlign(bool on)
{
    on = on && m_hardware.sufficient;
    if (on == m_autoAlign)
        return;
    m_autoAlign = on;
    AppPaths::settings()->setValue("lyrics/autoAlign", on);
    if (!on)
        cancelAlign();
    emit settingsChanged();
    emit alignChanged();
    maybeAutoAlign();
}

void LyricsService::setSttModel(const QString &model)
{
    if (model == m_model || !SpeechEngine::modelNames().contains(model))
        return;
    m_model = model;
    AppPaths::settings()->setValue("lyrics/model", model);
    m_alignTried.clear();
    emit settingsChanged();
    emit sttChanged();
    emit alignChanged();
}

bool LyricsService::canAlign() const
{
    return m_hardware.sufficient && sttReady() && m_state == QLatin1String("ready") && !m_lyrics.isEmpty()
           && !m_lyrics.synced && !aligning();
}

// ---- utwór i pobieranie ---------------------------------------------------------

void LyricsService::setTrack(const Track *track)
{
    if (!track) {
        if (!m_hasTrack)
            return;
        m_hasTrack = false;
        ++m_serial;
        cancelAlign();
        m_track = {};
        apply({}, false);
        setState(QStringLiteral("none"));
        return;
    }
    if (m_hasTrack && track->id == m_track.id)
        return;

    ++m_serial;
    cancelAlign();
    m_hasTrack = true;
    m_track = *track;
    fetch();
}

void LyricsService::retry()
{
    if (!m_hasTrack)
        return;
    ++m_serial;
    cancelAlign();
    QFile::remove(cachePath(m_track.id));
    m_alignTried.remove(m_track.id);
    fetch();
}

void LyricsService::fetch()
{
    apply({}, false);
    if (m_track.demo || m_track.source == QLatin1String("radio")) {
        setState(QStringLiteral("missing"));
        return;
    }

    Lyrics cached;
    if (loadCached(m_track.id, cached)) {
        if (cached.isEmpty()) {
            setState(QStringLiteral("missing"));
        } else {
            apply(cached, false);
            setState(QStringLiteral("ready"));
            maybeAutoAlign();
        }
        return;
    }

    setState(QStringLiteral("loading"));
    const quint64 serial = m_serial;
    if (m_track.source == QLatin1String("youtube")) {
        fetchYouTube(m_track.path, serial, [this, serial](const Lyrics &l, const QString &error) {
            if (serial != m_serial)
                return;
            if (!l.isEmpty()) {
                apply(l, true);
                setState(QStringLiteral("ready"));
                maybeAutoAlign();
            } else if (!error.isEmpty()) {
                setState(QStringLiteral("error"), error);
            } else {
                storeCached(m_track.id, {});
                setState(QStringLiteral("missing"));
            }
        });
        return;
    }

    // Plik lokalny: odszukaj utwór w YouTube Music po wykonawcy i tytule.
    const QString query = (m_track.artist + QChar(' ') + m_track.title).trimmed();
    if (m_track.title.isEmpty()) {
        fetchLocal(serial, {}, {});
        return;
    }
    YouTubeService::instance()->api()->search(query, [this, serial](const QList<Track> &results, const QString &error) {
        if (serial != m_serial)
            return;
        const int match = error.isEmpty() ? YtMusicApi::bestMatch(m_track, results) : -1;
        const QString videoId = match >= 0 ? results.at(match).path : QString();
        if (videoId.isEmpty()) {
            fetchLocal(serial, {}, error);
            return;
        }
        fetchYouTube(videoId, serial, [this, serial](const Lyrics &l, const QString &ytError) {
            if (serial != m_serial)
                return;
            fetchLocal(serial, l, ytError);
        });
    });
}

void LyricsService::fetchYouTube(const QString &videoId, quint64 serial,
                                 std::function<void(const Lyrics &, const QString &)> done)
{
    YouTubeService::instance()->api()->fetchLyrics(videoId, [this, serial, done](const Lyrics &l, const QString &error) {
        if (serial == m_serial)
            done(l, error);
    });
}

void LyricsService::fetchLocal(quint64 serial, const Lyrics &fromYouTube, const QString &ytError)
{
    // Tekst z YouTube z czasami wygrywa; inaczej sprawdzamy plik (może mieć .lrc z czasami).
    if (fromYouTube.synced) {
        apply(fromYouTube, true);
        setState(QStringLiteral("ready"));
        return;
    }
    auto *watcher = new QFutureWatcher<Lyrics>(this);
    connect(watcher, &QFutureWatcher<Lyrics>::finished, this, [this, watcher, serial, fromYouTube, ytError] {
        watcher->deleteLater();
        if (serial != m_serial)
            return;
        const Lyrics local = watcher->result();
        const Lyrics &chosen = local.synced || fromYouTube.isEmpty() ? local : fromYouTube;
        if (!chosen.isEmpty()) {
            apply(chosen, true);
            setState(QStringLiteral("ready"));
            maybeAutoAlign();
        } else if (!ytError.isEmpty()) {
            setState(QStringLiteral("error"), ytError);
        } else {
            storeCached(m_track.id, {});
            setState(QStringLiteral("missing"));
        }
    });
    watcher->setFuture(QtConcurrent::run(readLocalLyrics, m_track.path));
}

void LyricsService::apply(const Lyrics &lyrics, bool store)
{
    m_lyrics = lyrics;
    if (m_lyrics.synced)
        m_lyrics.fillEnds(m_track.duration > 0 ? qint64(m_track.duration) * 1000 : -1);
    if (store && m_hasTrack)
        storeCached(m_track.id, m_lyrics);

    m_lines.clear();
    m_starts.clear();
    for (int i = 0; i < m_lyrics.lines.size(); ++i) {
        const LyricLine &l = m_lyrics.lines.at(i);
        m_lines << QVariantMap {
            { "text", l.text },
            { "start", l.start >= 0 ? l.start / 1000.0 : -1.0 },
            { "end", l.end >= 0 ? l.end / 1000.0 : -1.0 },
        };
        if (m_lyrics.synced && l.start >= 0 && !l.text.isEmpty())
            m_starts.emplace_back(l.start, i);
    }
    emit lyricsChanged();
    emit alignChanged();
}

void LyricsService::setState(const QString &state, const QString &error)
{
    m_state = state;
    m_error = error;
    emit lyricsChanged();
    emit alignChanged();
}

int LyricsService::lineAt(double seconds) const
{
    if (m_starts.empty())
        return -1;
    const qint64 ms = qint64(seconds * 1000);
    auto it = std::upper_bound(m_starts.cbegin(), m_starts.cend(), ms,
                               [](qint64 v, const std::pair<qint64, int> &e) { return v < e.first; });
    return it == m_starts.cbegin() ? -1 : std::prev(it)->second;
}

// ---- dopasowanie czasów (alpha) -------------------------------------------------

void LyricsService::maybeAutoAlign()
{
    if (m_autoAlign && canAlign() && !m_alignTried.contains(m_track.id))
        alignNow();
}

void LyricsService::alignNow()
{
    if (!canAlign())
        return;
    m_alignTried.insert(m_track.id, true);
    m_alignProgress = 0;
    m_alignStage = QStringLiteral("Przygotowanie");
    if (m_track.source == QLatin1String("youtube")) {
        StreamResolver *resolver = YouTubeService::instance()->resolver();
        const QUrl cached = resolver->cachedUrl(m_track.path);
        if (cached.isValid()) {
            startAlign(cached);
        } else {
            m_waitingStream = m_track.path;
            emit alignChanged();
            resolver->resolve(m_track.path);
        }
        return;
    }
    startAlign(QUrl::fromLocalFile(m_track.path));
}

void LyricsService::startAlign(const QUrl &audio)
{
    // Początek tekstu jako podpowiedź dla modelu (słownictwo, pisownia, język).
    QString prompt;
    for (const LyricLine &l : std::as_const(m_lyrics.lines)) {
        if (prompt.size() > 200)
            break;
        if (!l.text.isEmpty())
            prompt += (prompt.isEmpty() ? QString() : QStringLiteral(" ")) + l.text;
    }

    const quint64 serial = m_serial;
    const Lyrics base = m_lyrics;
    const qint64 durationMs = m_track.duration > 0 ? qint64(m_track.duration) * 1000 : -1;
    m_speech.transcribe(audio, m_model, prompt.left(220),
                        [this, serial, base, durationMs](const QList<LyricsAligner::Word> &words, const QString &error) {
        emit alignChanged();
        if (serial != m_serial)
            return;
        if (!error.isEmpty()) {
            emit notice(QStringLiteral("Dopasowanie tekstu: ") + error, QStringLiteral("close"));
            return;
        }
        const LyricsAligner::Result r = LyricsAligner::align(base, words, durationMs);
        if (r.coverage < 0.25 || r.anchoredLines < 2) {
            emit notice(QStringLiteral("Nie udało się dopasować tekstu do nagrania (rozpoznano %1% słów)")
                            .arg(qRound(r.coverage * 100)),
                        QStringLiteral("close"));
            return;
        }
        Lyrics aligned = r.lyrics;
        aligned.source = base.source;
        apply(aligned, true);
        emit notice(QStringLiteral("Dopasowano czas tekstu (%1% słów)").arg(qRound(r.coverage * 100)),
                    QStringLiteral("check"));
    });
    emit alignChanged();
}

void LyricsService::cancelAlign()
{
    m_waitingStream.clear();
    m_speech.cancel();
    m_alignProgress = 0;
    m_alignStage.clear();
    emit alignChanged();
}

// ---- cache ----------------------------------------------------------------------

QString LyricsService::cachePath(const QString &trackId) const
{
    const QString dir = AppPaths::dataDir() + QStringLiteral("/lyrics");
    QDir().mkpath(dir);
    return dir + QChar('/') + QCryptographicHash::hash(trackId.toUtf8(), QCryptographicHash::Sha1).toHex()
           + QStringLiteral(".json");
}

bool LyricsService::loadCached(const QString &trackId, Lyrics &out) const
{
    QFile f(cachePath(trackId));
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    if (o.value("id").toString() != trackId)
        return false;
    out = Lyrics::fromJson(o);
    if (out.isEmpty()) {
        // Zapamiętany brak tekstu — po kilku dniach sprawdzamy ponownie.
        const qint64 at = qint64(o.value("fetched").toDouble());
        return QDateTime::currentMSecsSinceEpoch() - at < kMissingRetryMs;
    }
    return true;
}

void LyricsService::storeCached(const QString &trackId, const Lyrics &lyrics) const
{
    QJsonObject o = lyrics.toJson();
    o.insert("id", trackId);
    o.insert("fetched", double(QDateTime::currentMSecsSinceEpoch()));
    QSaveFile f(cachePath(trackId));
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
        f.commit();
    }
}
