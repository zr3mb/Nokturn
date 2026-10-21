#pragma once

#include "LyricsAligner.h"

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QUrl>

#include <functional>

class QNetworkReply;

// Speech recognition (whisper.cpp) for aligning lyric timing — an experimental feature.
// The "whisper-cli" program and the ggml model are downloaded from official sources (GitHub, Hugging Face)
// and verified with the SHA-256 published by the service. Qt Multimedia (FFmpeg) decodes the audio
// to 16 kHz mono, and whisper returns words with timestamps.
class SpeechEngine : public QObject
{
    Q_OBJECT

public:
    struct Hardware
    {
        int threads = 1;
        double memoryGb = 0;     // 0 = unknown
        bool sufficient = false;
        QString recommendedModel;
        QString summary;
    };

    using WordsCallback = std::function<void(const QList<LyricsAligner::Word> &words, const QString &error)>;

    explicit SpeechEngine(QObject *parent = nullptr);
    ~SpeechEngine() override;

    static Hardware detectHardware();
    static QStringList modelNames() { return { QStringLiteral("base"), QStringLiteral("small"), QStringLiteral("medium") }; }
    static QString modelSize(const QString &model);

    // An NVIDIA graphics card (from nvidia-smi): with the CUDA build of whisper.cpp recognition runs on it,
    // several times faster. supported = a driver new enough for that build (CUDA 12.4: 551 or newer).
    struct Gpu
    {
        QString name;
        QString driver;
        int memoryMb = 0;
        bool supported = false;
    };
    const Gpu &gpu() const { return m_gpu; }
    bool gpuEngineReady() const { return !m_gpuEnginePath.isEmpty(); }
    bool usesGpu() const { return m_gpu.supported && gpuEngineReady(); }
    static QString gpuEngineSize() { return QStringLiteral("653 MB"); }
    void downloadGpuEngine();
    void removeGpuEngine();

    bool engineReady() const { return !m_enginePath.isEmpty() || usesGpu(); }
    // A ready-made whisper.cpp package exists only for Windows; on Linux it's installed from the repositories.
    static bool canDownloadEngine()
    {
#ifdef Q_OS_WIN
        return true;
#else
        return false;
#endif
    }
    bool modelReady(const QString &model) const;
    bool downloading() const { return m_downloading; }
    qreal downloadProgress() const { return m_progress; }
    QString downloadLabel() const { return m_downloadLabel; }
    bool busy() const { return m_job != nullptr; }

    void detect();
    void downloadEngine();
    void downloadModel(const QString &model);
    void cancelDownload();
    void removeModel(const QString &model);

    // Transcription with word timestamps. `prompt` (e.g. the start of the lyrics) hints the vocabulary to the model.
    void transcribe(const QUrl &audio, const QString &model, const QString &prompt, WordsCallback callback);
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
    void finishDownloadError(const QString &message);
    // Downloads the file as a stream (models are hundreds of MB) and checks SHA-256 before saving.
    void fetchVerified(const QUrl &url, const QString &target, const QByteArray &sha256Hex,
                       std::function<void(bool ok)> done);
    void startWhisper();
    void runWhisper(const QString &wav, const QString &outBase);
    void finishJob(const QList<LyricsAligner::Word> &words, const QString &error);
    void detectGpu();
    QString gpuToolsDir() const;
    // The newest release's package (the first of `assets` it has), verified and unpacked into `dir`.
    void fetchEngine(const QStringList &assets, const QString &dir, const QString &label);

    QString m_enginePath;
    QString m_gpuEnginePath;
    Gpu m_gpu;
    QNetworkAccessManager m_net;
    QPointer<QNetworkReply> m_reply;
    bool m_downloading = false;
    qreal m_progress = 0;
    QString m_downloadLabel;
    Job *m_job = nullptr;
};
