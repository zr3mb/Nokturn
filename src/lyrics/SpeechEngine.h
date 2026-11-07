#pragma once

#include "LyricsAligner.h"

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QUrl>

#include <functional>

class QNetworkReply;
class QProcess;

// Speech processing for lyric timing (CrispASR) — an experimental feature.
// The "crispasr" program and its models are downloaded from official sources (GitHub, Hugging Face) in pinned
// versions and verified with SHA-256. Qt Multimedia (FFmpeg) decodes the audio to WAV.
//
// - align(): forced alignment. The lyrics are known, so the CTC aligner only finds where each of their words
//   is sung. With a graphics card the vocals are first separated from the music, which is what makes the
//   timing accurate (measured on real songs: 97% of lines within half a second, against 80% for recognizing
//   the singing with whisper and matching the lyrics to it).
// - transcribe(): speech recognition with a whisper model, for tracks nobody has lyrics for.
class SpeechEngine : public QObject
{
    Q_OBJECT

public:
    struct Hardware
    {
        int threads = 1;
        double memoryGb = 0;     // 0 = unknown
        bool sufficient = false; // enough for recognizing the singing on the CPU
        QString recommendedModel;
        QString summary;
    };

    // A graphics card the engine can use (Vulkan: NVIDIA, AMD and Intel alike). Empty name = none.
    struct Gpu
    {
        QString name;
        int memoryMb = 0;
        bool present() const { return !name.isEmpty(); }
    };

    using WordsCallback = std::function<void(const QList<LyricsAligner::Word> &words, const QString &error)>;
    // One entry for every line given to align(): the words of that line with their times (empty = not found).
    using LinesCallback = std::function<void(const QList<QList<LyricsAligner::Word>> &lines, const QString &error)>;

    explicit SpeechEngine(QObject *parent = nullptr);
    ~SpeechEngine() override;

    static Hardware detectHardware();
    static QStringList modelNames() { return { QStringLiteral("base"), QStringLiteral("small"), QStringLiteral("medium") }; }
    static QString modelSize(const QString &model);
    static QString engineSize();
    static QString alignerSize() { return QStringLiteral("461 MB"); }
    static QString separatorSize() { return QStringLiteral("457 MB"); }

    // A ready-made CrispASR package exists for 64-bit Windows and Linux; elsewhere it's looked up in PATH.
    static bool canDownloadEngine();

    const Gpu &gpu() const { return m_gpu; }
    bool engineReady() const { return !m_enginePath.isEmpty(); }
    bool alignerReady() const;
    bool separatorReady() const;
    bool alignReady() const { return engineReady() && alignerReady(); }
    // The vocals are separated before alignment: only on a graphics card (on the CPU it takes the better part
    // of an hour per track).
    bool separates() const { return m_gpu.present() && separatorReady(); }
    bool modelReady(const QString &model) const;
    bool downloading() const { return m_downloading; }
    qreal downloadProgress() const { return m_progress; }
    QString downloadLabel() const { return m_downloadLabel; }
    bool busy() const { return m_job != nullptr; }

    void detect();
    // The engine, the aligner and (with a graphics card) the vocal separator — whatever is missing, one after
    // another.
    void downloadComponents();
    void downloadModel(const QString &model);
    void cancelDownload();
    void removeComponents();
    void removeModel(const QString &model);

    // Transcription with word timestamps. `language` is whisper's code ("pl", "en"…); empty = detected
    // from pieces of the recording.
    void transcribe(const QUrl &audio, const QString &model, const QString &language, WordsCallback callback);
    // Where every word of `lines` (non-empty lyric lines, in order) is sung. `language` may be empty.
    void align(const QUrl &audio, const QStringList &lines, const QString &language, LinesCallback callback);
    void cancel();

signals:
    void stateChanged();
    void progress(qreal value, const QString &stage);
    void notice(const QString &text, const QString &icon);

private:
    struct Job;

    QString toolsDir() const;
    QString modelsDir() const;
    QString modelPath(const QString &model) const;
    QString alignerPath() const;
    QString separatorPath() const;
    void finishDownloadError(const QString &message);
    // Downloads the file as a stream (models are hundreds of MB) and checks SHA-256 before saving.
    void fetchVerified(const QUrl &url, const QString &target, const QByteArray &sha256Hex,
                       std::function<void(bool ok)> done);
    void nextComponent();
    void fetchEngine(bool cpuOnly);
    // Runs the engine once to see that it starts and which graphics card it finds.
    void probeEngine(std::function<void(bool ok)> done);

    void startJob(const QUrl &audio, int sampleRate, int channels);
    void decoded();
    void detectLanguage();
    void runWhisper();
    void runSeparation();
    void runAligner(const QString &wav);
    QProcess *startProcess(const QStringList &args, bool gpu);
    // Progress of a step that doesn't report its own: estimated from the time it usually takes.
    void estimateProgress(qreal from, qreal to, qint64 expectedMs, const QString &stage);
    void finishJob(const QList<LyricsAligner::Word> &words, const QString &error);
    void finishAlign(const QList<QList<LyricsAligner::Word>> &lines, const QString &error);

    QString m_enginePath;
    bool m_engineCpuBuild = false;
    Gpu m_gpu;
    QNetworkAccessManager m_net;
    QPointer<QNetworkReply> m_reply;
    bool m_downloading = false;
    bool m_componentsQueued = false;
    qreal m_progress = 0;
    QString m_downloadLabel;
    Job *m_job = nullptr;
};
