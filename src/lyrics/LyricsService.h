#pragma once

#include "LrcLib.h"
#include "Lyrics.h"
#include "SpeechEngine.h"
#include "TrackModel.h"

#include <QHash>
#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <vector>

class QQmlEngine;
class QJSEngine;

// Lyrics for QML.
// Source order: YouTube Music (the timed version, then plain text; for local files
// the track is looked up by artist and title), and second the lyrics from the file
// (.lrc next to the track or the LYRICS tag). When the lyrics have no timestamps and the user turned on
// "automatic timing" (alpha), line times come from speech recognition (SpeechEngine).
// Results go to a cache on disk — recognition runs once per track.
class LyricsService : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Lyrics)
    QML_SINGLETON

    // "none" (nothing playing) | "loading" | "ready" | "missing" | "error"
    Q_PROPERTY(QString state READ state NOTIFY lyricsChanged)
    Q_PROPERTY(QString error READ error NOTIFY lyricsChanged)
    Q_PROPERTY(QVariantList lines READ lines NOTIFY lyricsChanged)
    Q_PROPERTY(bool synced READ synced NOTIFY lyricsChanged)
    Q_PROPERTY(QString source READ source NOTIFY lyricsChanged)
    Q_PROPERTY(QString origin READ origin NOTIFY lyricsChanged)
    // Manual timing correction for this track (s, + = the lyrics come later) — the synced lyrics may
    // belong to a different version (e.g. a music video with an intro). Remembered per track.
    Q_PROPERTY(double offset READ offset NOTIFY offsetChanged)

    Q_PROPERTY(bool aligning READ aligning NOTIFY alignChanged)
    Q_PROPERTY(qreal alignProgress READ alignProgress NOTIFY alignChanged)
    Q_PROPERTY(QString alignStage READ alignStage NOTIFY alignChanged)
    Q_PROPERTY(bool canAlign READ canAlign NOTIFY alignChanged)
    // A track without lyrics: they can be recognized from the recording (test feature).
    Q_PROPERTY(bool canTranscribe READ canTranscribe NOTIFY alignChanged)
    Q_PROPERTY(bool recognizing READ recognizing NOTIFY alignChanged)

    Q_PROPERTY(bool autoAlign READ autoAlign WRITE setAutoAlign NOTIFY settingsChanged)
    Q_PROPERTY(QString sttModel READ sttModel WRITE setSttModel NOTIFY settingsChanged)
    Q_PROPERTY(QStringList sttModels READ sttModels CONSTANT)
    Q_PROPERTY(bool sttEngineReady READ sttEngineReady NOTIFY sttChanged)
    Q_PROPERTY(bool sttCanDownloadEngine READ sttCanDownloadEngine CONSTANT)
    Q_PROPERTY(bool sttModelReady READ sttModelReady NOTIFY sttChanged)
    Q_PROPERTY(bool sttReady READ sttReady NOTIFY sttChanged)
    Q_PROPERTY(bool sttDownloading READ sttDownloading NOTIFY sttChanged)
    Q_PROPERTY(qreal sttProgress READ sttProgress NOTIFY sttChanged)
    Q_PROPERTY(QString sttDownloadLabel READ sttDownloadLabel NOTIFY sttChanged)
    Q_PROPERTY(bool hardwareOk READ hardwareOk CONSTANT)
    Q_PROPERTY(QString hardwareSummary READ hardwareSummary CONSTANT)
    Q_PROPERTY(QString recommendedModel READ recommendedModel CONSTANT)

public:
    static LyricsService *instance();
    static LyricsService *create(QQmlEngine *, QJSEngine *);

    QString state() const { return m_state; }
    QString error() const { return m_error; }
    QVariantList lines() const { return m_lines; }
    bool synced() const { return m_lyrics.synced; }
    QString source() const { return m_lyrics.source; }
    QString origin() const { return m_lyrics.origin; }
    double offset() const { return m_offset; }

    bool aligning() const { return m_speech.busy() || !m_waitingStream.isEmpty(); }
    qreal alignProgress() const { return m_alignProgress; }
    QString alignStage() const { return m_alignStage; }
    bool canAlign() const;
    bool canTranscribe() const;
    bool recognizing() const { return aligning() && m_recognize; }

    bool autoAlign() const { return m_autoAlign; }
    void setAutoAlign(bool on);
    QString sttModel() const { return m_model; }
    void setSttModel(const QString &model);
    QStringList sttModels() const { return SpeechEngine::modelNames(); }
    bool sttEngineReady() const { return m_speech.engineReady(); }
    bool sttCanDownloadEngine() const { return SpeechEngine::canDownloadEngine(); }
    bool sttModelReady() const { return m_speech.modelReady(m_model); }
    bool sttReady() const { return sttEngineReady() && sttModelReady(); }
    bool sttDownloading() const { return m_speech.downloading(); }
    qreal sttProgress() const { return m_speech.downloadProgress(); }
    QString sttDownloadLabel() const { return m_speech.downloadLabel(); }
    bool hardwareOk() const { return m_hardware.sufficient; }
    QString hardwareSummary() const { return m_hardware.summary; }
    QString recommendedModel() const { return m_hardware.recommendedModel; }

    // Index of the line (in `lines`) being sung right now; -1 before the first line or without timestamps.
    Q_INVOKABLE int lineAt(double seconds) const;
    Q_INVOKABLE QString modelSize(const QString &model) const { return SpeechEngine::modelSize(model); }
    Q_INVOKABLE void retry();
    Q_INVOKABLE void nudge(double seconds);   // shifts the timing of this track's lyrics
    Q_INVOKABLE void resetOffset();
    Q_INVOKABLE void alignNow();
    Q_INVOKABLE void transcribeNow();
    Q_INVOKABLE void cancelAlign();
    Q_INVOKABLE void downloadSttEngine() { m_speech.downloadEngine(); }
    Q_INVOKABLE void redetectStt() { m_speech.detect(); }
    Q_INVOKABLE void downloadSttModel() { m_speech.downloadModel(m_model); }
    Q_INVOKABLE void cancelSttDownload() { m_speech.cancelDownload(); }
    Q_INVOKABLE void removeSttModel(const QString &model) { m_speech.removeModel(model); }

    // Called by the player when the track changes (nullptr = nothing playing).
    void setTrack(const Track *track);

signals:
    void lyricsChanged();
    void offsetChanged();
    void alignChanged();
    void settingsChanged();
    void sttChanged();
    void notice(const QString &text, const QString &icon);

private:
    explicit LyricsService(QObject *parent = nullptr);

    void fetch();
    void fetchYouTube(const QString &videoId, quint64 serial, std::function<void(const Lyrics &, const QString &)> done);
    void fetchLocal(quint64 serial, const Lyrics &fromYouTube, const QString &ytError);
    void withLrcLib(quint64 serial, const Lyrics &best, const QString &error);   // LRCLIB when no timing yet
    void finish(const Lyrics &lyrics, const QString &error);
    void apply(const Lyrics &lyrics, bool store);
    void setState(const QString &state, const QString &error = {});
    void maybeAutoAlign();
    void runSpeech(bool recognize);   // the track's audio (a YouTube stream is resolved first), then startAlign
    void startAlign(const QUrl &audio);
    void startRecognize(const QUrl &audio);

    QString cachePath(const QString &trackId) const;
    QString offsetKey() const;
    void loadOffset();
    bool loadCached(const QString &trackId, Lyrics &out) const;
    void storeCached(const QString &trackId, const Lyrics &lyrics) const;

    LrcLib m_lrclib;
    SpeechEngine m_speech;
    SpeechEngine::Hardware m_hardware;

    Track m_track;
    bool m_hasTrack = false;
    quint64 m_serial = 0;
    Lyrics m_lyrics;
    QVariantList m_lines;
    std::vector<std::pair<qint64, int>> m_starts;   // (start ms, line index) — for binary search
    double m_offset = 0;
    QString m_state = QStringLiteral("none");
    QString m_error;

    QString m_waitingStream;   // videoId whose URL we're waiting for to start recognition
    qreal m_alignProgress = 0;
    QString m_alignStage;
    QHash<QString, bool> m_alignTried;   // tracks for which recognition has already failed this session
    bool m_recognize = false;            // the job writes the lyrics themselves, not just their timing

    bool m_autoAlign = false;
    QString m_model;
};
