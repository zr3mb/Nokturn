#include "SpectrumAnalyzer.h"

#include <QAudioBuffer>
#include <QtMath>

#include <algorithm>
#include <cmath>

SpectrumAnalyzer::SpectrumAnalyzer(int bands)
    : m_bands(bands)
    , m_ring(kFftSize, 0.0f)
    , m_window(kFftSize)
    , m_fft(kFftSize)
{
    for (int i = 0; i < kFftSize; ++i)
        m_window[i] = 0.5f * (1.0f - std::cos(2.0f * float(M_PI) * i / (kFftSize - 1)));
    buildBands();
}

void SpectrumAnalyzer::buildBands()
{
    // Pasma od 40 Hz do 16 kHz (albo Nyquista), rozłożone logarytmicznie.
    const double lo = 40.0;
    const double hi = std::min(16000.0, m_sampleRate / 2.0);
    const double binHz = double(m_sampleRate) / kFftSize;
    m_edges.assign(m_bands + 1, 0);
    for (int b = 0; b <= m_bands; ++b) {
        const double f = lo * std::pow(hi / lo, double(b) / m_bands);
        m_edges[b] = std::clamp(int(std::round(f / binHz)), 1, kFftSize / 2 - 1);
    }
    // Każde pasmo musi mieć co najmniej jeden bin.
    for (int b = 1; b <= m_bands; ++b)
        m_edges[b] = std::max(m_edges[b], m_edges[b - 1] + 1);
}

void SpectrumAnalyzer::clear()
{
    std::fill(m_ring.begin(), m_ring.end(), 0.0f);
    m_filled = 0;
}

void SpectrumAnalyzer::push(const QAudioBuffer &buffer)
{
    if (!buffer.isValid())
        return;
    const QAudioFormat format = buffer.format();
    if (format.sampleRate() > 0 && format.sampleRate() != m_sampleRate) {
        m_sampleRate = format.sampleRate();
        buildBands();
    }

    const int channels = std::max(1, format.channelCount());
    const int bytesPerSample = format.bytesPerSample();
    const int bytesPerFrame = format.bytesPerFrame();
    const char *data = buffer.constData<char>();
    const qsizetype frames = buffer.frameCount();

    for (qsizetype i = 0; i < frames; ++i) {
        float mono = 0.0f;
        const char *frame = data + i * bytesPerFrame;
        for (int c = 0; c < channels; ++c)
            mono += format.normalizedSampleValue(frame + c * bytesPerSample);
        m_ring[m_write] = mono / channels;
        m_write = (m_write + 1) % kFftSize;
    }
    m_filled = int(std::min<qsizetype>(kFftSize, m_filled + frames));
}

void SpectrumAnalyzer::pushSamples(const float *mono, size_t count, int sampleRate)
{
    if (sampleRate > 0 && sampleRate != m_sampleRate) {
        m_sampleRate = sampleRate;
        buildBands();
    }
    for (size_t i = 0; i < count; ++i) {
        m_ring[m_write] = mono[i];
        m_write = (m_write + 1) % kFftSize;
    }
    m_filled = int(std::min<size_t>(kFftSize, size_t(m_filled) + count));
}

void SpectrumAnalyzer::compute(QList<qreal> &out)
{
    out.resize(m_bands);
    if (m_filled < kFftSize / 4) {
        std::fill(out.begin(), out.end(), 0.0);
        return;
    }

    // Najnowsze próbki z bufora pierścieniowego, w kolejności bit-reversal.
    const int bits = int(std::log2(kFftSize));
    for (int i = 0; i < kFftSize; ++i) {
        int rev = 0;
        for (int b = 0; b < bits; ++b)
            rev |= ((i >> b) & 1) << (bits - 1 - b);
        const float s = m_ring[(m_write + i) % kFftSize] * m_window[i];
        m_fft[rev] = { s, 0.0f };
    }

    // Iteracyjne FFT radix-2
    for (int len = 2; len <= kFftSize; len <<= 1) {
        const float angle = -2.0f * float(M_PI) / len;
        const std::complex<float> wlen(std::cos(angle), std::sin(angle));
        for (int i = 0; i < kFftSize; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (int j = 0; j < len / 2; ++j) {
                const std::complex<float> u = m_fft[i + j];
                const std::complex<float> v = m_fft[i + j + len / 2] * w;
                m_fft[i + j] = u + v;
                m_fft[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }

    // Pełnoskalowa sinusoida z oknem Hanna daje amplitudę ≈ N/4.
    const float reference = kFftSize / 4.0f;
    for (int b = 0; b < m_bands; ++b) {
        float peak = 0.0f;
        for (int k = m_edges[b]; k < m_edges[b + 1]; ++k)
            peak = std::max(peak, std::abs(m_fft[k]));
        const float x = float(b) / (m_bands - 1);
        // dBFS + lekkie podbicie góry (muzyka naturalnie traci energię w wysokich pasmach)
        const float db = 20.0f * std::log10(std::max(peak / reference, 1e-6f)) + x * 9.0f;
        out[b] = std::clamp((db + 62.0f) / 56.0f, 0.0f, 1.0f);
    }
}
