#include "SpeechEngine.h"

#include "AppPaths.h"

#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
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
#include <QtEndian>

#include <algorithm>
#include <memory>

#ifdef Q_OS_WIN
#  include <windows.h>
#elif defined(Q_OS_UNIX)
#  include <unistd.h>
#endif

namespace {

const char *kReleaseApi = "https://api.github.com/repos/ggml-org/whisper.cpp/releases/latest";
const char *kReleaseAsset = "whisper-bin-x64.zip";
const char *kModelTreeApi = "https://huggingface.co/api/models/ggerganov/whisper.cpp/tree/main";
const char *kModelBase = "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/";
constexpr int kSampleRate = 16000;

QString engineFileName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("whisper-cli.exe");
#else
    return QStringLiteral("whisper-cli");
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

// Decoder buffer -> 16 kHz mono int16. The FFmpeg backend usually converts to the requested format itself,
// but in case it doesn't, we convert here (channel averaging + linear interpolation).
void appendConverted(const QAudioBuffer &buffer, QByteArray &out, double &resamplePos)
{
    const QAudioFormat f = buffer.format();
    if (f.sampleRate() == kSampleRate && f.channelCount() == 1 && f.sampleFormat() == QAudioFormat::Int16) {
        out.append(buffer.constData<char>(), buffer.byteCount());
        return;
    }
    const int channels = std::max(1, f.channelCount());
    const qsizetype frames = buffer.frameCount();
    std::vector<float> mono(frames, 0.0f);
    for (qsizetype i = 0; i < frames; ++i) {
        float sum = 0;
        for (int c = 0; c < channels; ++c) {
            const qsizetype idx = i * channels + c;
            switch (f.sampleFormat()) {
            case QAudioFormat::Float: sum += buffer.constData<float>()[idx]; break;
            case QAudioFormat::Int16: sum += buffer.constData<qint16>()[idx] / 32768.0f; break;
            case QAudioFormat::Int32: sum += buffer.constData<qint32>()[idx] / 2147483648.0f; break;
            case QAudioFormat::UInt8: sum += (buffer.constData<quint8>()[idx] - 128) / 128.0f; break;
            default: break;
            }
        }
        mono[i] = sum / channels;
    }
    const double step = double(std::max(1, f.sampleRate())) / kSampleRate;
    for (; resamplePos < double(frames) - 1; resamplePos += step) {
        const qsizetype i = qsizetype(resamplePos);
        const float frac = float(resamplePos - i);
        const float v = std::clamp(mono[i] + (mono[i + 1] - mono[i]) * frac, -1.0f, 1.0f);
        const qint16 s = qint16(v * 32767.0f);
        out.append(reinterpret_cast<const char *>(&s), sizeof s);
    }
    resamplePos -= double(std::max<qsizetype>(0, frames - 1));
}

bool writeWav(const QString &path, const QByteArray &pcm)
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
    u16(1);                  // mono
    u32(kSampleRate);
    u32(kSampleRate * 2);
    u16(2);
    u16(16);
    f.write("data");
    u32(quint32(pcm.size()));
    return f.write(pcm) == pcm.size();
}

} // namespace

struct SpeechEngine::Job
{
    QUrl audio;
    QString model;
    QString prompt;
    WordsCallback callback;
    QAudioDecoder *decoder = nullptr;
    QProcess *process = nullptr;
    std::unique_ptr<QTemporaryDir> dir;
    QByteArray pcm;
    double resamplePos = 0;
};

SpeechEngine::SpeechEngine(QObject *parent)
    : QObject(parent)
{
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

// ---- files ----------------------------------------------------------------------

QString SpeechEngine::toolsDir() const
{
    const QString dir = AppPaths::dataDir() + QStringLiteral("/tools/whisper");
    QDir().mkpath(dir);
    return dir;
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

bool SpeechEngine::modelReady(const QString &model) const
{
    return modelNames().contains(model) && QFileInfo(modelPath(model)).size() > 1024 * 1024;
}

void SpeechEngine::detect()
{
    m_enginePath.clear();
    QDirIterator it(toolsDir(), { engineFileName() }, QDir::Files, QDirIterator::Subdirectories);
    if (it.hasNext())
        m_enginePath = it.next();
    if (m_enginePath.isEmpty()) {
        const QString local = QCoreApplication::applicationDirPath() + QChar('/') + engineFileName();
        if (QFile::exists(local))
            m_enginePath = local;
        // Distribution packages: "whisper-cli" (Arch, Fedora, Homebrew), "whisper-cpp" (Nix, some others).
        for (const char *name : { "whisper-cli", "whisper-cpp" }) {
            if (!m_enginePath.isEmpty())
                break;
            m_enginePath = QStandardPaths::findExecutable(QString::fromLatin1(name));
        }
    }
    emit stateChanged();
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
    request.setHeader(QNetworkRequest::UserAgentHeader, QByteArrayLiteral("Nokturn"));
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
        done(true);
    });
}

void SpeechEngine::downloadEngine()
{
#ifndef Q_OS_WIN
    // Ready-made whisper.cpp packages on GitHub are for Windows only.
    emit notice(tr("Install whisper.cpp (the whisper-cli command) from your package manager"), QStringLiteral("close"));
#else
    if (m_downloading)
        return;
    m_downloading = true;
    m_progress = 0;
    m_downloadLabel = QStringLiteral("whisper.cpp");
    emit stateChanged();

    // Description of the latest release: the package URL and its SHA-256 (the "digest" field, computed by GitHub).
    QNetworkRequest request(QUrl(QString::fromLatin1(kReleaseApi)));
    request.setHeader(QNetworkRequest::UserAgentHeader, QByteArrayLiteral("Nokturn"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    QNetworkReply *reply = m_net.get(request);
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            finishDownloadError(tr("Couldn't get information about whisper.cpp: %1").arg(reply->errorString()));
            return;
        }
        const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
        QString url;
        QByteArray sha;
        for (const QJsonValue &v : release.value("assets").toArray()) {
            const QJsonObject a = v.toObject();
            if (a.value("name").toString() != QLatin1String(kReleaseAsset))
                continue;
            url = a.value("browser_download_url").toString();
            const QString digest = a.value("digest").toString();
            if (digest.startsWith(QLatin1String("sha256:")))
                sha = digest.mid(7).toLatin1();
        }
        if (url.isEmpty() || sha.size() != 64) {
            finishDownloadError(tr("The whisper.cpp release has no package with a checksum"));
            return;
        }
        const QString zip = AppPaths::dataDir() + QStringLiteral("/tools/whisper.zip");
        fetchVerified(QUrl(url), zip, sha, [this, zip](bool) {
            // Unpacked with the tar program built into Windows 10/11 (bsdtar handles ZIP).
            QDir(toolsDir()).removeRecursively();
            const QString dir = toolsDir();
            auto *tar = new QProcess(this);
            hideConsole(tar);
            connect(tar, &QProcess::finished, this, [this, tar, zip](int code) {
                tar->deleteLater();
                QFile::remove(zip);
                m_downloading = false;
                m_reply.clear();
                detect();
                if (code == 0 && engineReady())
                    emit notice(tr("Speech recognition is ready"), QStringLiteral("check"));
                else
                    emit notice(tr("Couldn't unpack whisper.cpp"), QStringLiteral("close"));
            });
            QString tarExe = qEnvironmentVariable("SystemRoot") + QStringLiteral("/System32/tar.exe");
            if (!QFile::exists(tarExe))
                tarExe = QStringLiteral("tar");
            tar->start(tarExe, { QStringLiteral("-xf"), QDir::toNativeSeparators(zip),
                                 QStringLiteral("-C"), QDir::toNativeSeparators(dir) });
        });
    });
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
    request.setHeader(QNetworkRequest::UserAgentHeader, QByteArrayLiteral("Nokturn"));
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
            m_reply.clear();
            emit stateChanged();
            emit notice(tr("The speech recognition model (%1) is ready").arg(model), QStringLiteral("check"));
        });
    });
}

// ---- transcription --------------------------------------------------------------

void SpeechEngine::transcribe(const QUrl &audio, const QString &model, const QString &prompt, WordsCallback callback)
{
    cancel();
    if (!engineReady()) {
        callback({}, tr("The whisper.cpp program is missing"));
        return;
    }
    if (!modelReady(model)) {
        callback({}, tr("The speech recognition model is missing"));
        return;
    }

    m_job = new Job;
    m_job->audio = audio;
    m_job->model = model;
    m_job->prompt = prompt;
    m_job->callback = std::move(callback);
    const QString tmpRoot = AppPaths::dataDir() + QStringLiteral("/tmp");
    QDir().mkpath(tmpRoot);
    m_job->dir = std::make_unique<QTemporaryDir>(tmpRoot + QStringLiteral("/stt-XXXXXX"));
    emit stateChanged();

    QAudioFormat format;
    format.setSampleRate(kSampleRate);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    auto *decoder = new QAudioDecoder(this);
    m_job->decoder = decoder;
    decoder->setAudioFormat(format);
    decoder->setSource(audio);
    connect(decoder, &QAudioDecoder::bufferReady, this, [this, decoder] {
        if (!m_job || m_job->decoder != decoder)
            return;
        while (decoder->bufferAvailable())
            appendConverted(decoder->read(), m_job->pcm, m_job->resamplePos);
        const qint64 duration = decoder->duration();
        if (duration > 0)
            emit progress(0.15 * std::min(1.0, double(decoder->position()) / duration), tr("Decoding audio"));
    });
    connect(decoder, &QAudioDecoder::finished, this, [this, decoder] {
        if (!m_job || m_job->decoder != decoder)
            return;
        m_job->decoder = nullptr;
        decoder->deleteLater();
        startWhisper();
    });
    connect(decoder, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error), this, [this, decoder](QAudioDecoder::Error) {
        if (!m_job || m_job->decoder != decoder)
            return;
        const QString message = decoder->errorString();
        m_job->decoder = nullptr;
        decoder->deleteLater();
        finishJob({}, tr("Couldn't decode the audio: %1").arg(message));
    });
    emit progress(0, tr("Decoding audio"));
    decoder->start();
}

void SpeechEngine::startWhisper()
{
    if (m_job->pcm.size() < kSampleRate * 2) {
        finishJob({}, tr("Not enough audio to recognize"));
        return;
    }
    const QString wav = m_job->dir->filePath(QStringLiteral("audio.wav"));
    const QString outBase = m_job->dir->filePath(QStringLiteral("words"));
    if (!writeWav(wav, m_job->pcm)) {
        finishJob({}, tr("Couldn't save the temporary file"));
        return;
    }
    m_job->pcm.clear();
    m_job->pcm.squeeze();

    const int threads = std::clamp(QThread::idealThreadCount() - 1, 2, 12);
    QStringList args = {
        QStringLiteral("-m"), modelPath(m_job->model),
        QStringLiteral("-f"), wav,
        QStringLiteral("-l"), QStringLiteral("auto"),
        QStringLiteral("-t"), QString::number(threads),
        QStringLiteral("-ml"), QStringLiteral("1"),      // one word per segment = the time of every word
        QStringLiteral("-sow"),
        QStringLiteral("-oj"), QStringLiteral("-of"), outBase,
        QStringLiteral("-pp"),
    };
    if (!m_job->prompt.isEmpty())
        args << QStringLiteral("--prompt") << m_job->prompt;

    auto *process = new QProcess(this);
    hideConsole(process);
    m_job->process = process;
    connect(process, &QProcess::readyReadStandardError, this, [this, process] {
        static const QRegularExpression re(QStringLiteral("progress\\s*=\\s*(\\d+)%"));
        auto it = re.globalMatch(QString::fromUtf8(process->readAllStandardError()));
        int last = -1;
        while (it.hasNext())
            last = it.next().captured(1).toInt();
        if (last >= 0)
            emit progress(0.15 + 0.85 * last / 100.0, tr("Recognizing the singing"));
    });
    connect(process, &QProcess::finished, this, [this, process, outBase](int code, QProcess::ExitStatus status) {
        if (!m_job || m_job->process != process)
            return;
        m_job->process = nullptr;
        process->deleteLater();
        if (status != QProcess::NormalExit || code != 0) {
            finishJob({}, tr("whisper.cpp ended with an error (code %1)").arg(code));
            return;
        }
        QFile f(outBase + QStringLiteral(".json"));
        if (!f.open(QIODevice::ReadOnly)) {
            finishJob({}, tr("No recognition result"));
            return;
        }
        QList<LyricsAligner::Word> words;
        for (const QJsonValue &v : QJsonDocument::fromJson(f.readAll()).object().value("transcription").toArray()) {
            const QJsonObject seg = v.toObject();
            const QString text = seg.value("text").toString().trimmed();
            const QJsonObject offsets = seg.value("offsets").toObject();
            if (!text.isEmpty())
                words << LyricsAligner::Word { text, offsets.value("from").toVariant().toLongLong(),
                                               offsets.value("to").toVariant().toLongLong() };
        }
        finishJob(words, words.isEmpty() ? tr("No words were recognized") : QString());
    });
    process->start(m_enginePath, args);
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
