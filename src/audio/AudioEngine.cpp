#include "AudioEngine.h"

#include "Equalizer.h"

#include <QAudioBuffer>
#include <QAudioDevice>
#include <QAudioSink>
#include <QElapsedTimer>
#include <QIODevice>
#include <QMediaDevices>
#include <QMutex>

#include <algorithm>
#include <cstring>

// Bufor pierścieniowy czytany przez QAudioSink (w wątku audio) i zapisywany z wątku GUI.
class AudioEngine::Ring : public QIODevice
{
public:
    Ring(const QAudioFormat &format, QObject *parent)
        : QIODevice(parent)
        , m_channels(format.channelCount())
        , m_int16(format.sampleFormat() == QAudioFormat::Int16)
        , m_capacity(size_t(format.sampleRate()) * 2)            // 2 s zapasu
        , m_prebuffer(size_t(format.sampleRate()) / 5)           // 200 ms poduszki
        , m_data(m_capacity * size_t(m_channels), 0.0f)
    {
    }

    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return 1 << 20; }

    void append(const float *samples, size_t frames)
    {
        QMutexLocker lock(&m_mutex);
        for (size_t f = 0; f < frames; ++f) {
            if (m_count == m_capacity) {           // przepełnienie — porzuć najstarsze
                m_read = (m_read + 1) % m_capacity;
                --m_count;
            }
            const size_t pos = (m_read + m_count) % m_capacity;
            std::memcpy(&m_data[pos * m_channels], samples + f * m_channels, sizeof(float) * m_channels);
            ++m_count;
        }
    }

    void clear()
    {
        QMutexLocker lock(&m_mutex);
        m_read = 0;
        m_count = 0;
        m_primed = false;
    }

    void takePlayed(std::vector<float> &out)
    {
        QMutexLocker lock(&m_mutex);
        out.swap(m_played);
        m_played.clear();
    }

protected:
    qint64 writeData(const char *, qint64) override { return -1; }

    qint64 readData(char *data, qint64 maxSize) override
    {
        const int bytesPerSample = m_int16 ? 2 : 4;
        const size_t frames = size_t(maxSize) / size_t(bytesPerSample * m_channels);
        QMutexLocker lock(&m_mutex);

        // Najpierw zbierz poduszkę; po niedoborze zbieramy ją od nowa.
        if (!m_primed && m_count >= m_prebuffer)
            m_primed = true;
        const size_t take = m_primed ? std::min(frames, m_count) : 0;
        if (m_primed && take < frames)
            m_primed = false;

        for (size_t f = 0; f < frames; ++f) {
            const float *src = f < take ? &m_data[((m_read + f) % m_capacity) * m_channels] : nullptr;
            float mono = 0.0f;
            for (int c = 0; c < m_channels; ++c) {
                const float v = src ? src[c] : 0.0f;
                mono += v;
                if (m_int16)
                    reinterpret_cast<qint16 *>(data)[f * m_channels + c] = qint16(std::clamp(v, -1.0f, 1.0f) * 32767.0f);
                else
                    reinterpret_cast<float *>(data)[f * m_channels + c] = v;
            }
            if (src && m_played.size() < 16384)
                m_played.push_back(mono / m_channels);
        }
        m_read = (m_read + take) % m_capacity;
        m_count -= take;
        return qint64(frames) * bytesPerSample * m_channels;
    }

private:
    QMutex m_mutex;
    const int m_channels;
    const bool m_int16;
    const size_t m_capacity;
    const size_t m_prebuffer;
    std::vector<float> m_data;
    size_t m_read = 0;
    size_t m_count = 0;
    bool m_primed = false;
    std::vector<float> m_played;
};

AudioEngine::AudioEngine(QObject *parent)
    : QObject(parent)
{
    const QAudioDevice device = QMediaDevices::defaultAudioOutput();
    m_format = device.preferredFormat();
    if (m_format.sampleRate() <= 0)
        m_format.setSampleRate(48000);
    if (m_format.channelCount() <= 0)
        m_format.setChannelCount(2);
    m_format.setChannelCount(std::min(m_format.channelCount(), 2));
    m_format.setSampleFormat(QAudioFormat::Float);
    if (!device.isFormatSupported(m_format))
        m_format.setSampleFormat(QAudioFormat::Int16);

    Equalizer::instance()->setSampleRate(m_format.sampleRate());

    m_ring = new Ring(m_format, this);
    m_ring->open(QIODevice::ReadOnly);
    m_sink = new QAudioSink(device, m_format, this);
    m_sink->setBufferSize(m_format.bytesForDuration(80000));   // 80 ms bufora urządzenia
    m_sink->start(m_ring);
}

AudioEngine::~AudioEngine()
{
    if (m_sink)
        m_sink->stop();
}

QAudioFormat AudioEngine::bufferFormat() const
{
    QAudioFormat f;
    f.setSampleFormat(QAudioFormat::Float);
    f.setChannelCount(m_format.channelCount());
    f.setSampleRate(m_format.sampleRate());
    return f;
}

void AudioEngine::push(const QAudioBuffer &buffer)
{
    if (!buffer.isValid())
        return;
    const QAudioFormat in = buffer.format();
    const int outCh = m_format.channelCount();
    const qsizetype frames = buffer.frameCount();
    m_scratch.resize(size_t(frames) * outCh);

    if (in.sampleFormat() == QAudioFormat::Float && in.channelCount() == outCh) {
        std::memcpy(m_scratch.data(), buffer.constData<float>(), m_scratch.size() * sizeof(float));
    } else {
        // Inny format — konwersja próbka po próbce (z rozłożeniem mono na oba kanały).
        const char *data = buffer.constData<char>();
        const int inCh = std::max(1, in.channelCount());
        const int bpf = in.bytesPerFrame();
        const int bps = in.bytesPerSample();
        for (qsizetype f = 0; f < frames; ++f)
            for (int c = 0; c < outCh; ++c)
                m_scratch[f * outCh + c] = in.normalizedSampleValue(data + f * bpf + std::min(c, inCh - 1) * bps);
    }

    Equalizer::instance()->process(m_scratch.data(), frames, outCh);
    m_ring->append(m_scratch.data(), size_t(frames));

    // Diagnostyka (NOKTURN_AUDIO_DEBUG=1): czy próbki przychodzą w tempie odtwarzania.
    static const bool debug = qEnvironmentVariableIsSet("NOKTURN_AUDIO_DEBUG");
    if (debug) {
        static QElapsedTimer clock;
        static qint64 total = 0;
        if (!clock.isValid())
            clock.start();
        total += frames;
        if (clock.elapsed() >= 3000) {
            qInfo("audio: %.2f s dźwięku w %.2f s (format %d Hz / %d kan.)",
                  double(total) / m_format.sampleRate(), clock.elapsed() / 1000.0,
                  m_format.sampleRate(), outCh);
            total = 0;
            clock.restart();
        }
    }
}

void AudioEngine::flush()
{
    m_ring->clear();
    Equalizer::instance()->resetState();
}

void AudioEngine::setPaused(bool paused)
{
    if (paused && m_sink->state() == QAudio::ActiveState)
        m_sink->suspend();
    else if (!paused && m_sink->state() == QAudio::SuspendedState)
        m_sink->resume();
}

void AudioEngine::setVolume(float linear)
{
    m_sink->setVolume(linear);
}

void AudioEngine::takePlayed(std::vector<float> &mono)
{
    m_ring->takePlayed(mono);
}
