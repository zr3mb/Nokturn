#include "LyricsService.h"

#include "AppPaths.h"
#include "Language.h"
#include "LyricsAligner.h"
#include "TagLibPath.h"
#include "youtube/YouTubeService.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
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
#include <cmath>

namespace {

constexpr qint64 kMissingRetryMs = 3LL * 24 * 3600 * 1000;   // missing lyrics are checked again after 3 days

// Lyrics from the file: first an .lrc next to the track (may have timestamps), then the LYRICS / UNSYNCEDLYRICS tag.
// Runs on a worker thread.
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
                l.source = QCoreApplication::translate("LyricsService", "File %1").arg(QFileInfo(lrc).fileName());
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
        Lyrics l = Lyrics::fromLrc(text);   // some programs store the LRC format in the tag
        if (!l.isEmpty()) {
            l.source = QCoreApplication::translate("LyricsService", "Tag in the file");
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
    // After the engine or a model is downloaded, align the current track right away (if the feature is on).
    connect(&m_speech, &SpeechEngine::stateChanged, this, &LyricsService::maybeAutoAlign, Qt::QueuedConnection);
    // The hardware summary is a translated label — rebuild it in the interface language.
    connect(Language::instance(), &Language::changed, this, [this] {
        m_hardware.summary = SpeechEngine::detectHardware().summary;
    });

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

// ---- settings -------------------------------------------------------------------

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

bool LyricsService::canTranscribe() const
{
    return m_hasTrack && m_hardware.sufficient && sttReady() && m_state == QLatin1String("missing") && !aligning()
           && m_track.source != QLatin1String("radio") && !m_track.demo;
}

// ---- track and fetching ---------------------------------------------------------

void LyricsService::setTrack(const Track *track)
{
    if (!track) {
        if (!m_hasTrack)
            return;
        m_hasTrack = false;
        ++m_serial;
        cancelAlign();
        m_track = {};
        loadOffset();
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
    loadOffset();
    fetch();
}

QString LyricsService::offsetKey() const
{
    // Track ids can be file paths — hashed into a plain settings key.
    return QStringLiteral("lyrics/offset/")
         + QString::fromLatin1(QCryptographicHash::hash(m_track.id.toUtf8(), QCryptographicHash::Md5).toHex());
}

void LyricsService::loadOffset()
{
    const double offset = m_hasTrack ? AppPaths::settings()->value(offsetKey(), 0.0).toDouble() : 0.0;
    if (offset != m_offset) {
        m_offset = offset;
        emit offsetChanged();
    }
}

void LyricsService::nudge(double seconds)
{
    if (!m_hasTrack)
        return;
    m_offset = std::clamp(std::round((m_offset + seconds) * 10.0) / 10.0, -30.0, 30.0);
    if (m_offset == 0.0)
        AppPaths::settings()->remove(offsetKey());
    else
        AppPaths::settings()->setValue(offsetKey(), m_offset);
    emit offsetChanged();
}

void LyricsService::resetOffset()
{
    nudge(-m_offset);
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
            withLrcLib(serial, l, error);
        });
        return;
    }

    // Local file: look the track up on YouTube Music by artist and title.
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
    // Lyrics from YouTube with timestamps win; otherwise we check the file (it may have an .lrc with timestamps).
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
        withLrcLib(serial, local.synced || fromYouTube.isEmpty() ? local : fromYouTube, ytError);
    });
    watcher->setFuture(QtConcurrent::run(readLocalLyrics, m_track.path));
}

void LyricsService::withLrcLib(quint64 serial, const Lyrics &best, const QString &error)
{
    // Timed lyrics are what we're after: without them (none, or plain text only) LRCLIB gets a try.
    if (best.synced) {
        finish(best, error);
        return;
    }
    m_lrclib.fetch(m_track.artist, m_track.title, m_track.album, m_track.duration,
                   [this, serial, best, error](const Lyrics &lrc) {
        if (serial != m_serial)
            return;
        if (lrc.synced || best.isEmpty())
            finish(lrc, lrc.isEmpty() ? error : QString());
        else
            finish(best, error);
    });
}

void LyricsService::finish(const Lyrics &lyrics, const QString &error)
{
    if (!lyrics.isEmpty()) {
        apply(lyrics, true);
        setState(QStringLiteral("ready"));
        maybeAutoAlign();
    } else if (!error.isEmpty()) {
        setState(QStringLiteral("error"), error);
    } else {
        storeCached(m_track.id, {});
        setState(QStringLiteral("missing"));
    }
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

// ---- timing alignment (alpha) ---------------------------------------------------

void LyricsService::maybeAutoAlign()
{
    if (m_autoAlign && canAlign() && !m_alignTried.contains(m_track.id))
        alignNow();
}

void LyricsService::alignNow()
{
    if (canAlign())
        runSpeech(false);
}

void LyricsService::transcribeNow()
{
    if (canTranscribe())
        runSpeech(true);
}

void LyricsService::runSpeech(bool recognize)
{
    m_recognize = recognize;
    m_alignTried.insert(m_track.id, true);
    m_alignProgress = 0;
    m_alignStage = tr("Preparing");
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
    if (m_recognize) {
        startRecognize(audio);
        return;
    }
    // The start of the lyrics as a hint for the model (vocabulary, spelling, language).
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
            emit notice(tr("Lyrics alignment: %1").arg(error), QStringLiteral("close"));
            return;
        }
        const LyricsAligner::Result r = LyricsAligner::align(base, words, durationMs);
        if (r.coverage < 0.25 || r.anchoredLines < 2) {
            emit notice(tr("Couldn't align the lyrics to the recording (%1% of words recognized)")
                            .arg(qRound(r.coverage * 100)),
                        QStringLiteral("close"));
            return;
        }
        Lyrics aligned = r.lyrics;
        aligned.source = base.source;
        apply(aligned, true);
        emit notice(tr("Aligned the lyrics timing (%1% of words)").arg(qRound(r.coverage * 100)),
                    QStringLiteral("check"));
    });
    emit alignChanged();
}

void LyricsService::startRecognize(const QUrl &audio)
{
    // No lyrics to go by: the words and their times come from the recording alone (test feature).
    const quint64 serial = m_serial;
    const qint64 durationMs = m_track.duration > 0 ? qint64(m_track.duration) * 1000 : -1;
    m_speech.transcribe(audio, m_model, QString(),
                        [this, serial, durationMs](const QList<LyricsAligner::Word> &words, const QString &error) {
        emit alignChanged();
        if (serial != m_serial)
            return;
        if (!error.isEmpty()) {
            emit notice(tr("Lyrics recognition: %1").arg(error), QStringLiteral("close"));
            return;
        }
        Lyrics recognized = LyricsAligner::fromTranscription(words, durationMs);
        if (recognized.wordCount() < 6) {
            emit notice(tr("No singing was recognized in this track"), QStringLiteral("close"));
            return;
        }
        recognized.source = tr("Recognized automatically (test feature), may contain mistakes");
        apply(recognized, true);
        setState(QStringLiteral("ready"));
        emit notice(tr("Recognized the lyrics (test feature)"), QStringLiteral("check"));
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
    // Before cache version 2, signed-in users got YouTube lyrics without timing (the Android request carried
    // the website's cookies) — those are fetched again once.
    if (o.value("v").toInt() < 2 && !out.synced && out.origin == QLatin1String("youtube"))
        return false;
    // Version 3 added LRCLIB: "missing" from before is checked again.
    if (o.value("v").toInt() < 3 && out.isEmpty())
        return false;
    if (out.isEmpty()) {
        // Remembered missing lyrics — we check again after a few days.
        const qint64 at = qint64(o.value("fetched").toDouble());
        return QDateTime::currentMSecsSinceEpoch() - at < kMissingRetryMs;
    }
    return true;
}

void LyricsService::storeCached(const QString &trackId, const Lyrics &lyrics) const
{
    QJsonObject o = lyrics.toJson();
    o.insert("id", trackId);
    o.insert("v", 3);
    o.insert("fetched", double(QDateTime::currentMSecsSinceEpoch()));
    QSaveFile f(cachePath(trackId));
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
        f.commit();
    }
}
