#include "SpeechEngine.h"

#include "AppPaths.h"

#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <memory>

#ifdef Q_OS_WIN
#  include <windows.h>
#elif defined(Q_OS_UNIX)
#  include <unistd.h>
#endif

#if (defined(Q_OS_WIN) || (defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID))) && defined(Q_PROCESSOR_X86_64)
#  define EVERPLAY_ENGINE_PACKAGE 1
#endif

namespace {

struct Package
{
    const char *url;
    const char *sha256;
};

// CrispASR in a pinned release: a newer one may change the command line. The checksums are the release's
// "digest" fields. The Vulkan build runs on any graphics card with a current driver and on the CPU; the
// CPU-only build is the fallback for a system without the Vulkan loader, where the other doesn't start.
#ifdef EVERPLAY_ENGINE_PACKAGE
#  ifdef Q_OS_WIN
const Package kEngine { "https://github.com/CrispStrobe/CrispASR/releases/download/v0.8.50/crispasr-windows-x86_64-vulkan.zip",
                        "ac6fbfd527d782d7b999a135721dd131d76b18016283256c842b8f12411b1978" };
const Package kEngineCpu { "https://github.com/CrispStrobe/CrispASR/releases/download/v0.8.50/crispasr-windows-x86_64-cpu.zip",
                           "03565a3c60249ead81b6934f719af3ef09f48d28121d4231490db1c43522c53e" };
const char *kEngineSize = "37 MB";
#  else
const Package kEngine { "https://github.com/CrispStrobe/CrispASR/releases/download/v0.8.50/crispasr-linux-x86_64-vulkan.tar.gz",
                        "32f7fdcc1b0ccecb7ca9e85831ce1e3f91aa6a235316eeb15b0ee8641cd60af5" };
const Package kEngineCpu { "https://github.com/CrispStrobe/CrispASR/releases/download/v0.8.50/crispasr-linux-x86_64.tar.gz",
                           "bd034b6b45088fb55a39900f2a6661322dc4f1ef89e55e93cb540fe25f307b26" };
const char *kEngineSize = "72 MB";
#  endif
#endif

// Models from Hugging Face at a fixed revision of their repositories.
// The aligner: NVIDIA's Canary CTC model (25 European languages), for word timing of a known text.
const Package kAligner { "https://huggingface.co/cstr/canary-ctc-aligner-GGUF/resolve/2b22dc9aff585bc8368a5228173c8afe22c155b7/canary-ctc-aligner-q5_0.gguf",
                         "154502cfa0c53f11b2207f4f3c5626b4ca4ff8a5bc51ed185d42f87e6f5864c3" };
const char *kAlignerFile = "canary-ctc-aligner-q5_0.gguf";
// Vocal separation: Mel-Band RoFormer, the vocals stem.
const Package kSeparator { "https://huggingface.co/cstr/mel-band-roformer-vocals-GGUF/resolve/d6909215e1d7a3bc4ea1a569b930e38470d43000/mel-band-roformer-vocals-f16.gguf",
                           "fe94b114bce12653dae4e94968d28cbab36dcdd0c783fc052c3b656218233f24" };
const char *kSeparatorFile = "mel-band-roformer-vocals-f16.gguf";

// whisper models for recognizing lyrics nobody has: the repository's file list carries their SHA-256.
const char *kModelTreeApi = "https://huggingface.co/api/models/ggerganov/whisper.cpp/tree/main";
const char *kModelBase = "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/";

constexpr int kSpeechRate = 16000;      // whisper and the aligner work on 16 kHz mono
constexpr int kSeparationRate = 44100;  // the separator expects CD-quality stereo
// The aligner puts a word where its sound is clearest, on average a quarter of a second after the singer
// starts it (measured against hand-timed lyrics); the highlight should come with the start.
constexpr qint64 kAlignLeadMs = 250;
// A graphics card with less memory than this doesn't fit the separator.
constexpr int kMinGpuMemoryMb = 2000;

QString engineFileName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("crispasr.exe");
#else
    return QStringLiteral("crispasr");
#endif
}

void hideConsole(QProcess *process)
{
#ifdef Q_OS_WIN
    process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= CREATE_NO_WINDOW;
    });
#else
    Q_UNUSED(process);
#endif
}

int cpuThreads()
{
    return std::clamp(QThread::idealThreadCount() - 1, 2, 12);
}

// Decoder buffer -> int16 at `rate` with `channels` (1 or 2). The FFmpeg backend usually converts to the
// requested format itself, but in case it doesn't, we convert here (channel mixing + linear interpolation).
void appendConverted(const QAudioBuffer &buffer, QByteArray &out, double &resamplePos, int rate, int channels)
{
    const QAudioFormat f = buffer.format();
    if (f.sampleRate() == rate && f.channelCount() == channels && f.sampleFormat() == QAudioFormat::Int16) {
        out.append(buffer.constData<char>(), buffer.byteCount());
        return;
    }
    const int inChannels = std::max(1, f.channelCount());
    const qsizetype frames = buffer.frameCount();
    auto sample = [&](qsizetype idx) -> float {
        switch (f.sampleFormat()) {
        case QAudioFormat::Float: return buffer.constData<float>()[idx];
        case QAudioFormat::Int16: return buffer.constData<qint16>()[idx] / 32768.0f;
        case QAudioFormat::Int32: return buffer.constData<qint32>()[idx] / 2147483648.0f;
        case QAudioFormat::UInt8: return (buffer.constData<quint8>()[idx] - 128) / 128.0f;
        default: return 0.0f;
        }
    };
    // Mono: the average of every channel. Stereo: the first two (a mono source goes to both).
    std::vector<std::vector<float>> planes(channels, std::vector<float>(frames, 0.0f));
    for (qsizetype i = 0; i < frames; ++i) {
        if (channels == 1) {
            float sum = 0;
            for (int c = 0; c < inChannels; ++c)
                sum += sample(i * inChannels + c);
            planes[0][i] = sum / inChannels;
        } else {
            for (int c = 0; c < channels; ++c)
                planes[c][i] = sample(i * inChannels + std::min(c, inChannels - 1));
        }
    }
    const double step = double(std::max(1, f.sampleRate())) / rate;
    for (; resamplePos < double(frames) - 1; resamplePos += step) {
        const qsizetype i = qsizetype(resamplePos);
        const float frac = float(resamplePos - i);
        for (int c = 0; c < channels; ++c) {
            const std::vector<float> &p = planes[c];
            const float v = std::clamp(p[i] + (p[i + 1] - p[i]) * frac, -1.0f, 1.0f);
            const qint16 s = qint16(v * 32767.0f);
            out.append(reinterpret_cast<const char *>(&s), sizeof s);
        }
    }
    resamplePos -= double(std::max<qsizetype>(0, frames - 1));
}

bool writeWav(const QString &path, const QByteArray &pcm, int rate, int channels)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    auto u32 = [&f](quint32 v) { v = qToLittleEndian(v); f.write(reinterpret_cast<const char *>(&v), 4); };
    auto u16 = [&f](quint16 v) { v = qToLittleEndian(v); f.write(reinterpret_cast<const char *>(&v), 2); };
    f.write("RIFF");
    u32(36 + quint32(pcm.size()));
    f.write("WAVEfmt ");
    u32(16);
    u16(1);                  // PCM
    u16(quint16(channels));
    u32(quint32(rate));
    u32(quint32(rate * channels * 2));
    u16(quint16(channels * 2));
    u16(16);
    f.write("data");
    u32(quint32(pcm.size()));
    return f.write(pcm) == pcm.size();
}

} // namespace

struct SpeechEngine::Job
{
    enum Kind { Transcribe, Align };
    Kind kind = Transcribe;
    QUrl audio;
    QString model;
    QString language;          // whisper's code; empty = detected from the recording first (Transcribe)
    QStringList probes;        // short pieces of the recording for the language detection
    QHash<QString, double> votes;
    WordsCallback callback;
    QStringList lines;         // the lyrics to align
    LinesCallback linesCallback;
    bool separate = false;     // the vocals are separated from the music before aligning
    QAudioDecoder *decoder = nullptr;
    QProcess *process = nullptr;
    QTimer *ticker = nullptr;  // estimated progress of a step that doesn't report its own
    QElapsedTimer clock;
    std::unique_ptr<QTemporaryDir> dir;
    QByteArray pcm;
    int sampleRate = kSpeechRate;
    int channels = 1;
    double resamplePos = 0;
    int decodedPercent = 0;    // the last progress reported while decoding
    qint64 durationMs = 0;
    QString wav, outBase;
    bool cpuOnly = false;      // the graphics card failed once: the rest runs on the CPU

    ~Job() { delete ticker; }
};

SpeechEngine::SpeechEngine(QObject *parent)
    : QObject(parent)
{
    // whisper.cpp, which this replaced: the CPU and CUDA builds of the program (the models are still used).
    for (const char *former : { "/tools/whisper", "/tools/whisper-cuda" }) {
        QDir dir(AppPaths::dataDir() + QLatin1String(former));
        if (dir.exists())
            dir.removeRecursively();
    }
    detect();
}

SpeechEngine::~SpeechEngine()
{
    cancel();
}

// ---- hardware -------------------------------------------------------------------

SpeechEngine::Hardware SpeechEngine::detectHardware()
{
    Hardware hw;
    hw.threads = std::max(1, QThread::idealThreadCount());
#ifdef Q_OS_WIN
    MEMORYSTATUSEX status;
    status.dwLength = sizeof status;
    if (GlobalMemoryStatusEx(&status))
        hw.memoryGb = double(status.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0);
#elif defined(Q_OS_UNIX) && defined(_SC_PHYS_PAGES)
    const long pages = sysconf(_SC_PHYS_PAGES);
    const long pageSize = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && pageSize > 0)
        hw.memoryGb = double(pages) * pageSize / (1024.0 * 1024.0 * 1024.0);
#endif
    const bool memKnown = hw.memoryGb > 0;
    // Whisper on the CPU: "base" needs ~0.5 GB of RAM, "small" ~1 GB, "medium" ~2.5 GB, and the time
    // to recognize a 4-minute track grows with the model size — hence the 4 threads and 7.5 GB threshold.
    hw.sufficient = hw.threads >= 4 && (!memKnown || hw.memoryGb >= 7.5);
    if (hw.threads >= 16 && (!memKnown || hw.memoryGb >= 15.5))
        hw.recommendedModel = QStringLiteral("medium");
    else if (hw.threads >= 8)
        hw.recommendedModel = QStringLiteral("small");
    else
        hw.recommendedModel = QStringLiteral("base");
    hw.summary = tr("%n CPU threads", "", hw.threads);
    if (memKnown)
        hw.summary += QStringLiteral(" · ") + tr("%1 GB RAM").arg(qRound(hw.memoryGb));
    return hw;
}

QString SpeechEngine::modelSize(const QString &model)
{
    if (model == QLatin1String("base"))
        return QStringLiteral("142 MB");
    if (model == QLatin1String("small"))
        return QStringLiteral("466 MB");
    if (model == QLatin1String("medium"))
        return tr("1.5 GB");
    return {};
}

QString SpeechEngine::engineSize()
{
#ifdef EVERPLAY_ENGINE_PACKAGE
    return QString::fromLatin1(kEngineSize);
#else
    return {};
#endif
}

bool SpeechEngine::canDownloadEngine()
{
#ifdef EVERPLAY_ENGINE_PACKAGE
    return true;
#else
    return false;
#endif
}

// ---- files ----------------------------------------------------------------------

QString SpeechEngine::toolsDir() const
{
    return AppPaths::dataDir() + QStringLiteral("/tools/crispasr");
}

QString SpeechEngine::modelsDir() const
{
    const QString dir = AppPaths::dataDir() + QStringLiteral("/models");
    QDir().mkpath(dir);
    return dir;
}

QString SpeechEngine::modelPath(const QString &model) const
{
    return modelsDir() + QStringLiteral("/ggml-%1.bin").arg(model);
}

QString SpeechEngine::alignerPath() const
{
    return modelsDir() + QChar('/') + QLatin1String(kAlignerFile);
}

QString SpeechEngine::separatorPath() const
{
    return modelsDir() + QChar('/') + QLatin1String(kSeparatorFile);
}

bool SpeechEngine::modelReady(const QString &model) const
{
    return modelNames().contains(model) && QFileInfo(modelPath(model)).size() > 1024 * 1024;
}

bool SpeechEngine::alignerReady() const
{
    return QFileInfo(alignerPath()).size() > 1024 * 1024;
}

bool SpeechEngine::separatorReady() const
{
    return QFileInfo(separatorPath()).size() > 1024 * 1024;
}

void SpeechEngine::detect()
{
    m_enginePath.clear();
    if (QDir(toolsDir()).exists()) {
        QDirIterator it(toolsDir(), { engineFileName() }, QDir::Files, QDirIterator::Subdirectories);
        if (it.hasNext())
            m_enginePath = it.next();
    }
    m_engineCpuBuild = !m_enginePath.isEmpty() && !m_enginePath.contains(QLatin1String("vulkan"));
    if (m_enginePath.isEmpty()) {
        const QString local = QCoreApplication::applicationDirPath() + QChar('/') + engineFileName();
        if (QFile::exists(local))
            m_enginePath = local;
        else
            m_enginePath = QStandardPaths::findExecutable(QStringLiteral("crispasr"));
    }
    if (m_enginePath.isEmpty())
        m_gpu = {};
    emit stateChanged();
    if (!m_enginePath.isEmpty())
        probeEngine({});
}

void SpeechEngine::probeEngine(std::function<void(bool)> done)
{
    // "--diagnostics" lists the devices the engine can compute on: "[0] gpu name=Vulkan0 desc=<card> mem=<free>/<total> MiB".
    auto *process = new QProcess(this);
    hideConsole(process);
    process->setProcessChannelMode(QProcess::MergedChannels);
    connect(process, &QProcess::finished, this, [this, process, done](int code, QProcess::ExitStatus status) {
        process->deleteLater();
        const bool ok = status == QProcess::NormalExit && code == 0;
        static const QRegularExpression re(QStringLiteral("\\[\\d+\\]\\s+gpu\\s+name=\\S+\\s+desc=(.+?)\\s+mem=\\d+/(\\d+)\\s*MiB"));
        Gpu best;
        auto it = re.globalMatch(QString::fromUtf8(process->readAll()));
        while (it.hasNext()) {
            const auto m = it.next();
            const int memory = m.captured(2).toInt();
            if (memory >= kMinGpuMemoryMb && memory > best.memoryMb)
                best = Gpu { m.captured(1).trimmed(), memory };
        }
        m_gpu = ok ? best : Gpu {};
        emit stateChanged();
        if (done)
            done(ok);
    });
    connect(process, &QProcess::errorOccurred, this, [this, process, done](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;   // a crash also ends in finished()
        process->deleteLater();
        m_gpu = {};
        emit stateChanged();
        if (done)
            done(false);
    });
    // A broken graphics driver can make the device enumeration hang.
    QTimer::singleShot(20000, process, [process] { process->kill(); });
    process->start(m_enginePath, { QStringLiteral("--diagnostics") });
}

void SpeechEngine::removeComponents()
{
    cancel();
    QDir(toolsDir()).removeRecursively();
    QFile::remove(alignerPath());
    QFile::remove(separatorPath());
    detect();
}

void SpeechEngine::removeModel(const QString &model)
{
    if (modelNames().contains(model))
        QFile::remove(modelPath(model));
    emit stateChanged();
}

// ---- downloading ----------------------------------------------------------------

void SpeechEngine::finishDownloadError(const QString &message)
{
    m_downloading = false;
    m_componentsQueued = false;
    m_reply.clear();
    emit stateChanged();
    if (!message.isEmpty())
        emit notice(message, QStringLiteral("close"));
}

void SpeechEngine::cancelDownload()
{
    if (m_reply)
        m_reply->abort();
}

void SpeechEngine::fetchVerified(const QUrl &url, const QString &target, const QByteArray &sha256Hex,
                                 std::function<void(bool)> done)
{
    auto file = std::make_shared<QSaveFile>(target);
    if (!file->open(QIODevice::WriteOnly)) {
        finishDownloadError(tr("Can't write the file: %1").arg(target));
        return;
    }
    auto hash = std::make_shared<QCryptographicHash>(QCryptographicHash::Sha256);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QByteArrayLiteral("Everplay"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = m_net.get(request);
    m_reply = reply;
    connect(reply, &QNetworkReply::readyRead, this, [reply, file, hash] {
        const QByteArray chunk = reply->readAll();
        hash->addData(chunk);
        file->write(chunk);
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        m_progress = total > 0 ? qreal(received) / total : 0;
        emit stateChanged();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, file, hash, sha256Hex, done] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            file->cancelWriting();
            finishDownloadError(reply->error() == QNetworkReply::OperationCanceledError
                                    ? tr("Download cancelled")
                                    : tr("Download error: %1").arg(reply->errorString()));
            return;
        }
        const QByteArray rest = reply->readAll();
        hash->addData(rest);
        file->write(rest);
        if (hash->result().toHex() != sha256Hex.toLower()) {
            file->cancelWriting();
            finishDownloadError(tr("The downloaded file doesn't match the checksum — aborted"));
            return;
        }
        if (!file->commit()) {
            finishDownloadError(tr("Couldn't save the downloaded file"));
            return;
        }
        m_reply.clear();
        done(true);
    });
}

void SpeechEngine::downloadComponents()
{
    if (m_downloading)
        return;
    m_componentsQueued = true;
    m_downloading = true;
    m_progress = 0;
    emit stateChanged();
    nextComponent();
}

void SpeechEngine::nextComponent()
{
    if (!m_componentsQueued)
        return;
    m_progress = 0;
    if (!engineReady()) {
        if (!canDownloadEngine()) {
            finishDownloadError(tr("Install CrispASR (the crispasr command)"));
            return;
        }
        fetchEngine(false);
        return;
    }
    auto fetchModel = [this](const Package &pkg, const QString &path, const QString &label) {
        m_downloadLabel = label;
        emit stateChanged();
        fetchVerified(QUrl(QString::fromLatin1(pkg.url)), path, pkg.sha256, [this](bool) { nextComponent(); });
    };
    if (!alignerReady()) {
        fetchModel(kAligner, alignerPath(), tr("alignment model"));
        return;
    }
    if (m_gpu.present() && !separatorReady()) {
        fetchModel(kSeparator, separatorPath(), tr("vocal separation model"));
        return;
    }
    m_componentsQueued = false;
    m_downloading = false;
    emit stateChanged();
    emit notice(tr("Lyric timing is ready"), QStringLiteral("check"));
}

void SpeechEngine::fetchEngine(bool cpuOnly)
{
#ifdef EVERPLAY_ENGINE_PACKAGE
    const Package &pkg = cpuOnly ? kEngineCpu : kEngine;
    m_downloadLabel = QStringLiteral("CrispASR");
    emit stateChanged();
    const QString url = QString::fromLatin1(pkg.url);
    const QString dir = toolsDir();
    const QString archive = dir + (url.endsWith(QLatin1String(".zip")) ? QStringLiteral(".zip") : QStringLiteral(".tar.gz"));
    fetchVerified(QUrl(url), archive, pkg.sha256, [this, archive, dir, cpuOnly](bool) {
        QDir(dir).removeRecursively();
        QDir().mkpath(dir);
        auto *tar = new QProcess(this);
        hideConsole(tar);
        connect(tar, &QProcess::finished, this, [this, tar, archive, cpuOnly](int code) {
            tar->deleteLater();
            QFile::remove(archive);
            m_enginePath.clear();
            QDirIterator it(toolsDir(), { engineFileName() }, QDir::Files, QDirIterator::Subdirectories);
            if (code == 0 && it.hasNext())
                m_enginePath = it.next();
            if (m_enginePath.isEmpty()) {
                finishDownloadError(tr("Couldn't unpack CrispASR"));
                return;
            }
            m_engineCpuBuild = cpuOnly;
            probeEngine([this, cpuOnly](bool ok) {
                if (ok) {
                    nextComponent();
                } else if (!cpuOnly) {
                    // No Vulkan on this system: the build for the CPU alone.
                    QDir(toolsDir()).removeRecursively();
                    m_enginePath.clear();
                    fetchEngine(true);
                } else {
                    QDir(toolsDir()).removeRecursively();
                    m_enginePath.clear();
                    finishDownloadError(tr("CrispASR doesn't start on this system"));
                }
            });
        });
#  ifdef Q_OS_WIN
        // The tar program built into Windows 10/11 (bsdtar) unpacks ZIP too.
        QString tarExe = qEnvironmentVariable("SystemRoot") + QStringLiteral("/System32/tar.exe");
        if (!QFile::exists(tarExe))
            tarExe = QStringLiteral("tar");
#  else
        const QString tarExe = QStringLiteral("tar");
#  endif
        tar->start(tarExe, { QStringLiteral("-xf"), QDir::toNativeSeparators(archive),
                             QStringLiteral("-C"), QDir::toNativeSeparators(dir) });
    });
#else
    Q_UNUSED(cpuOnly);
#endif
}

void SpeechEngine::downloadModel(const QString &model)
{
    if (m_downloading || !modelNames().contains(model))
        return;
    m_downloading = true;
    m_progress = 0;
    m_downloadLabel = tr("model %1").arg(model);
    emit stateChanged();

    // The repository file list contains the SHA-256 of every model (the lfs.oid field).
    QNetworkRequest request{QUrl(QString::fromLatin1(kModelTreeApi))};
    request.setHeader(QNetworkRequest::UserAgentHeader, QByteArrayLiteral("Everplay"));
    QNetworkReply *reply = m_net.get(request);
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, model] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            finishDownloadError(tr("Couldn't download the model list: %1").arg(reply->errorString()));
            return;
        }
        const QString file = QStringLiteral("ggml-%1.bin").arg(model);
        QByteArray sha;
        for (const QJsonValue &v : QJsonDocument::fromJson(reply->readAll()).array()) {
            const QJsonObject o = v.toObject();
            if (o.value("path").toString() == file)
                sha = o.value("lfs").toObject().value("oid").toString().toLatin1();
        }
        if (sha.size() != 64) {
            finishDownloadError(tr("Couldn't find the checksum of model %1").arg(model));
            return;
        }
        fetchVerified(QUrl(QString::fromLatin1(kModelBase) + file), modelPath(model), sha, [this, model](bool) {
            m_downloading = false;
            emit stateChanged();
            emit notice(tr("The speech recognition model (%1) is ready").arg(model), QStringLiteral("check"));
        });
    });
}

// ---- jobs -----------------------------------------------------------------------

void SpeechEngine::transcribe(const QUrl &audio, const QString &model, const QString &language, WordsCallback callback)
{
    cancel();
    if (!engineReady()) {
        callback({}, tr("The CrispASR program is missing"));
        return;
    }
    if (!modelReady(model)) {
        callback({}, tr("The speech recognition model is missing"));
        return;
    }
    m_job = new Job;
    m_job->kind = Job::Transcribe;
    m_job->model = model;
    m_job->language = language;
    m_job->callback = std::move(callback);
    startJob(audio, kSpeechRate, 1);
}

void SpeechEngine::align(const QUrl &audio, const QStringList &lines, const QString &language, LinesCallback callback)
{
    cancel();
    if (!alignReady()) {
        callback({}, tr("The lyric timing components are missing"));
        return;
    }
    m_job = new Job;
    m_job->kind = Job::Align;
    m_job->lines = lines;
    m_job->language = language;
    m_job->linesCallback = std::move(callback);
    m_job->separate = separates();
    if (m_job->separate)
        startJob(audio, kSeparationRate, 2);
    else
        startJob(audio, kSpeechRate, 1);
}

void SpeechEngine::startJob(const QUrl &audio, int sampleRate, int channels)
{
    m_job->audio = audio;
    m_job->sampleRate = sampleRate;
    m_job->channels = channels;
    const QString tmpRoot = AppPaths::dataDir() + QStringLiteral("/tmp");
    QDir().mkpath(tmpRoot);
    m_job->dir = std::make_unique<QTemporaryDir>(tmpRoot + QStringLiteral("/stt-XXXXXX"));
    emit stateChanged();

    QAudioFormat format;
    format.setSampleRate(sampleRate);
    format.setChannelCount(channels);
    format.setSampleFormat(QAudioFormat::Int16);

    auto *decoder = new QAudioDecoder(this);
    m_job->decoder = decoder;
    decoder->setAudioFormat(format);
    decoder->setSource(audio);
    connect(decoder, &QAudioDecoder::bufferReady, this, [this, decoder] {
        if (!m_job || m_job->decoder != decoder)
            return;
        while (decoder->bufferAvailable())
            appendConverted(decoder->read(), m_job->pcm, m_job->resamplePos, m_job->sampleRate, m_job->channels);
        // A song comes in over ten thousand small buffers. Reporting each one re-evaluated the whole
        // lyrics interface every time and made decoding take minutes instead of seconds, so only
        // whole percents go out.
        const qint64 duration = decoder->duration();
        const int percent = duration > 0 ? int(100 * std::min(1.0, double(decoder->position()) / duration)) : 0;
        if (percent > m_job->decodedPercent) {
            m_job->decodedPercent = percent;
            emit progress(0.15 * percent / 100.0, tr("Decoding audio"));
        }
    });
    connect(decoder, &QAudioDecoder::finished, this, [this, decoder] {
        if (!m_job || m_job->decoder != decoder)
            return;
        m_job->decoder = nullptr;
        decoder->deleteLater();
        decoded();
    });
    connect(decoder, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error), this, [this, decoder](QAudioDecoder::Error) {
        if (!m_job || m_job->decoder != decoder)
            return;
        const QString message = decoder->errorString();
        m_job->decoder = nullptr;
        decoder->deleteLater();
        if (m_job->kind == Job::Align)
            finishAlign({}, tr("Couldn't decode the audio: %1").arg(message));
        else
            finishJob({}, tr("Couldn't decode the audio: %1").arg(message));
    });
    emit progress(0, tr("Decoding audio"));
    decoder->start();
}

void SpeechEngine::decoded()
{
    const qint64 bytesPerSecond = qint64(m_job->sampleRate) * m_job->channels * 2;
    const bool align = m_job->kind == Job::Align;
    if (m_job->pcm.size() < bytesPerSecond) {
        if (align)
            finishAlign({}, tr("Not enough audio to recognize"));
        else
            finishJob({}, tr("Not enough audio to recognize"));
        return;
    }
    m_job->durationMs = m_job->pcm.size() * 1000 / bytesPerSecond;
    m_job->wav = m_job->dir->filePath(QStringLiteral("audio.wav"));
    m_job->outBase = m_job->dir->filePath(QStringLiteral("words"));
    if (!writeWav(m_job->wav, m_job->pcm, m_job->sampleRate, m_job->channels)) {
        if (align)
            finishAlign({}, tr("Couldn't save the temporary file"));
        else
            finishJob({}, tr("Couldn't save the temporary file"));
        return;
    }
    if (!align && m_job->language.isEmpty()) {
        // whisper guesses the language from the first 30 seconds, which in a song is often an intro or a
        // shout-out: a Polish song then comes out in Cyrillic. Pieces from the middle, where the singing is,
        // tell it much better.
        constexpr qsizetype kProbe = qsizetype(kSpeechRate) * 2 * 30;
        const qsizetype size = m_job->pcm.size();
        const QList<double> at = size > kProbe * 3 ? QList<double> { 0.3, 0.62 } : QList<double> { 0.5 };
        for (int i = 0; i < at.size(); ++i) {
            const qsizetype from = std::clamp<qsizetype>(qsizetype(size * at[i]) & ~qsizetype(1), 0, std::max<qsizetype>(0, size - kProbe));
            const QString probe = m_job->dir->filePath(QStringLiteral("probe%1.wav").arg(i));
            if (writeWav(probe, m_job->pcm.mid(from, kProbe), kSpeechRate, 1))
                m_job->probes << probe;
        }
    }
    m_job->pcm.clear();
    m_job->pcm.squeeze();
    if (!align)
        detectLanguage();
    else if (m_job->separate)
        runSeparation();
    else
        runAligner(m_job->wav);
}

QProcess *SpeechEngine::startProcess(const QStringList &args, bool gpu)
{
    auto *process = new QProcess(this);
    hideConsole(process);
    m_job->process = process;
    QStringList all = args;
    if (!gpu)
        all << QStringLiteral("-ng");
    process->setProgram(m_enginePath);
    process->setArguments(all);
    return process;
}

void SpeechEngine::estimateProgress(qreal from, qreal to, qint64 expectedMs, const QString &stage)
{
    if (!m_job->ticker) {
        m_job->ticker = new QTimer(this);
        m_job->ticker->setInterval(300);
    }
    m_job->ticker->disconnect(this);
    m_job->clock.restart();
    connect(m_job->ticker, &QTimer::timeout, this, [this, from, to, expectedMs, stage] {
        if (!m_job)
            return;
        const double part = std::min(0.95, double(m_job->clock.elapsed()) / std::max<qint64>(1, expectedMs));
        emit progress(from + (to - from) * part, stage);
    });
    m_job->ticker->start();
    emit progress(from, stage);
}

void SpeechEngine::detectLanguage()
{
    if (m_job->probes.isEmpty()) {
        if (m_job->language.isEmpty()) {
            QString best = QStringLiteral("auto");
            double score = 0;
            for (auto it = m_job->votes.cbegin(); it != m_job->votes.cend(); ++it)
                if (it.value() > score) {
                    best = it.key();
                    score = it.value();
                }
            m_job->language = best;
        }
        runWhisper();
        return;
    }
    const QString probe = m_job->probes.takeFirst();
    const bool gpu = m_gpu.present() && !m_job->cpuOnly;
    QProcess *process = startProcess({
        QStringLiteral("-m"), modelPath(m_job->model),
        QStringLiteral("-f"), probe,
        QStringLiteral("-t"), QString::number(gpu ? 4 : cpuThreads()),
        QStringLiteral("-l"), QStringLiteral("auto"),
        QStringLiteral("-dl"),
        // The whisper model itself tells the language (the default is a separate small one, downloaded).
        QStringLiteral("--lid-model"), modelPath(m_job->model),
    }, gpu);
    process->setProcessChannelMode(QProcess::MergedChannels);
    emit progress(0.15, tr("Detecting the language"));
    connect(process, &QProcess::finished, this, [this, process] {
        if (!m_job || m_job->process != process)
            return;
        m_job->process = nullptr;
        process->deleteLater();
        static const QRegularExpression re(QStringLiteral("detected '([a-z]{2,3})' \\(p=([0-9.]+)\\)"));
        const auto m = re.match(QString::fromUtf8(process->readAll()));
        if (m.hasMatch())
            m_job->votes[m.captured(1)] += m.captured(2).toDouble();
        detectLanguage();
    });
    process->start();
}

void SpeechEngine::runWhisper()
{
    // On the graphics card when there is one; a failure there (driver, memory) runs the same track again
    // on the CPU.
    const bool gpu = m_gpu.present() && !m_job->cpuOnly;
    QProcess *process = startProcess({
        QStringLiteral("-m"), modelPath(m_job->model),
        QStringLiteral("-f"), m_job->wav,
        QStringLiteral("-l"), m_job->language.isEmpty() ? QStringLiteral("auto") : m_job->language,
        QStringLiteral("-t"), QString::number(gpu ? 4 : cpuThreads()),
        QStringLiteral("-ml"), QStringLiteral("1"),      // one word per segment = the time of every word
        QStringLiteral("-sow"),
        // No text carried from one 30-second window to the next: with it, a chorus sung several times
        // sends the model into a loop that repeats one phrase to the end of a long song. (That also
        // turns off an initial prompt, so none is passed.)
        QStringLiteral("-mc"), QStringLiteral("0"),
        // A narrower beam: about a third faster and, over music, no less accurate.
        QStringLiteral("-bs"), QStringLiteral("3"),
        QStringLiteral("-bo"), QStringLiteral("3"),
        QStringLiteral("-oj"), QStringLiteral("-of"), m_job->outBase,
        QStringLiteral("-pp"),
    }, gpu);
    connect(process, &QProcess::readyReadStandardError, this, [this, process, gpu] {
        static const QRegularExpression re(QStringLiteral("progress\\s*=\\s*(\\d+)%"));
        auto it = re.globalMatch(QString::fromUtf8(process->readAllStandardError()));
        int last = -1;
        while (it.hasNext())
            last = it.next().captured(1).toInt();
        if (last >= 0)
            emit progress(0.15 + 0.85 * last / 100.0, gpu ? tr("Recognizing the singing (graphics card)")
                                                          : tr("Recognizing the singing"));
    });
    connect(process, &QProcess::finished, this, [this, process, gpu](int code, QProcess::ExitStatus status) {
        if (!m_job || m_job->process != process)
            return;
        m_job->process = nullptr;
        process->deleteLater();
        if (gpu && (status != QProcess::NormalExit || code != 0)) {
            m_job->cpuOnly = true;
            runWhisper();
            return;
        }
        if (status != QProcess::NormalExit || code != 0) {
            finishJob({}, tr("CrispASR ended with an error (code %1)").arg(code));
            return;
        }
        QFile f(m_job->outBase + QStringLiteral(".json"));
        if (!f.open(QIODevice::ReadOnly)) {
            finishJob({}, tr("No recognition result"));
            return;
        }
        QList<LyricsAligner::Word> words;
        // A word cut in the middle of a character is invalid UTF-8, which would make the whole file unreadable:
        // through QString it becomes a replacement character instead.
        const QJsonDocument doc = QJsonDocument::fromJson(QString::fromUtf8(f.readAll()).toUtf8());
        for (const QJsonValue &v : doc.object().value("transcription").toArray()) {
            const QJsonObject seg = v.toObject();
            const QString text = seg.value("text").toString().trimmed();
            const QJsonObject offsets = seg.value("offsets").toObject();
            if (!text.isEmpty())
                words << LyricsAligner::Word { text, offsets.value("from").toVariant().toLongLong(),
                                               offsets.value("to").toVariant().toLongLong() };
        }
        finishJob(words, words.isEmpty() ? tr("No words were recognized") : QString());
    });
    process->start();
}

void SpeechEngine::runSeparation()
{
    // Always on the graphics card (separates() needs one). It writes "<input name>_vocals.wav".
    QProcess *process = startProcess({
        QStringLiteral("--separate"),
        QStringLiteral("-m"), separatorPath(),
        QStringLiteral("-f"), m_job->wav,
        QStringLiteral("--stems"), QStringLiteral("vocals"),
        QStringLiteral("--sep-output-dir"), m_job->dir->path(),
    }, true);
    // About 0.15 s per second of a track on a mid-range card, plus loading the model.
    estimateProgress(0.15, 0.85, 8000 + m_job->durationMs / 5, tr("Separating the vocals (graphics card)"));
    connect(process, &QProcess::finished, this, [this, process](int code, QProcess::ExitStatus status) {
        if (!m_job || m_job->process != process)
            return;
        m_job->process = nullptr;
        process->deleteLater();
        const QString vocals = m_job->dir->filePath(QStringLiteral("audio_vocals.wav"));
        // When the separation fails, the lyrics are aligned to the whole mix: less accurate, but still a result.
        const bool ok = status == QProcess::NormalExit && code == 0 && QFileInfo(vocals).size() > 1024;
        runAligner(ok ? vocals : m_job->wav);
    });
    process->start();
}

void SpeechEngine::runAligner(const QString &wav)
{
    const QString text = m_job->dir->filePath(QStringLiteral("lyrics.txt"));
    const QString out = m_job->dir->filePath(QStringLiteral("aligned.json"));
    {
        QFile f(text);
        if (!f.open(QIODevice::WriteOnly) || f.write(m_job->lines.join(QChar('\n')).toUtf8() + '\n') < 0) {
            finishAlign({}, tr("Couldn't save the temporary file"));
            return;
        }
    }
    const bool gpu = m_gpu.present() && !m_job->cpuOnly;
    QStringList args = {
        QStringLiteral("--align-only"),
        QStringLiteral("-am"), alignerPath(),
        QStringLiteral("-f"), wav,
        QStringLiteral("--text-file"), text,
        QStringLiteral("--align-granularity"), QStringLiteral("segment"),   // one segment per line of text
        QStringLiteral("--align-format"), QStringLiteral("json"),
        QStringLiteral("--align-output"), out,
        QStringLiteral("-t"), QString::number(gpu ? 4 : cpuThreads()),
    };
    if (!m_job->language.isEmpty())
        args << QStringLiteral("-l") << m_job->language;
    QProcess *process = startProcess(args, gpu);
    estimateProgress(m_job->separate ? 0.85 : 0.15, 1.0, 4000 + m_job->durationMs / 40, tr("Aligning the lyrics"));
    connect(process, &QProcess::finished, this, [this, process, wav, out, gpu](int code, QProcess::ExitStatus status) {
        if (!m_job || m_job->process != process)
            return;
        m_job->process = nullptr;
        process->deleteLater();
        if (status != QProcess::NormalExit || code != 0) {
            if (gpu) {
                m_job->cpuOnly = true;
                runAligner(wav);
            } else {
                finishAlign({}, tr("CrispASR ended with an error (code %1)").arg(code));
            }
            return;
        }
        QFile f(out);
        if (!f.open(QIODevice::ReadOnly)) {
            finishAlign({}, tr("No recognition result"));
            return;
        }
        // [{"text": <line>, "start": s, "end": s, "words": [{"word", "start", "end"}]}], one entry per line.
        const QJsonArray segments = QJsonDocument::fromJson(QString::fromUtf8(f.readAll()).toUtf8()).array();
        auto wordsOf = [](const QJsonObject &segment) {
            QList<LyricsAligner::Word> words;
            for (const QJsonValue &v : segment.value("words").toArray()) {
                const QJsonObject w = v.toObject();
                const qint64 start = std::max<qint64>(0, std::llround(w.value("start").toDouble() * 1000) - kAlignLeadMs);
                const qint64 end = std::max<qint64>(start, std::llround(w.value("end").toDouble() * 1000) - kAlignLeadMs);
                words << LyricsAligner::Word { w.value("word").toString(), start, end };
            }
            return words;
        };
        const QStringList &lines = m_job->lines;
        QList<QList<LyricsAligner::Word>> result(lines.size());
        if (segments.size() == lines.size()) {
            for (int i = 0; i < lines.size(); ++i)
                result[i] = wordsOf(segments.at(i).toObject());
        } else {
            // A line the aligner skipped (nothing to say in it): the others are found by their text.
            int next = 0;
            for (const QJsonValue &v : segments) {
                const QJsonObject segment = v.toObject();
                const QString said = segment.value("text").toString().simplified();
                for (int i = next; i < lines.size(); ++i) {
                    if (lines.at(i).simplified() == said) {
                        result[i] = wordsOf(segment);
                        next = i + 1;
                        break;
                    }
                }
            }
        }
        const bool any = std::any_of(result.cbegin(), result.cend(), [](const auto &l) { return !l.isEmpty(); });
        finishAlign(result, any ? QString() : tr("No words were found in the recording"));
    });
    process->start();
}

void SpeechEngine::finishJob(const QList<LyricsAligner::Word> &words, const QString &error)
{
    if (!m_job)
        return;
    Job *job = m_job;
    m_job = nullptr;
    emit stateChanged();
    if (job->callback)
        job->callback(words, error);
    delete job;
}

void SpeechEngine::finishAlign(const QList<QList<LyricsAligner::Word>> &lines, const QString &error)
{
    if (!m_job)
        return;
    Job *job = m_job;
    m_job = nullptr;
    emit stateChanged();
    if (job->linesCallback)
        job->linesCallback(lines, error);
    delete job;
}

void SpeechEngine::cancel()
{
    if (!m_job)
        return;
    Job *job = m_job;
    m_job = nullptr;
    if (job->decoder) {
        job->decoder->disconnect(this);
        job->decoder->stop();
        job->decoder->deleteLater();
    }
    if (job->process) {
        job->process->disconnect(this);
        job->process->kill();
        job->process->waitForFinished(2000);
        job->process->deleteLater();
    }
    delete job;
    emit stateChanged();
}
