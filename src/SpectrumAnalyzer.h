#pragma once

#include <QList>

#include <complex>
#include <vector>

class QAudioBuffer;

// Spectrum analysis of the playing audio: collects samples from QAudioBufferOutput,
// computes an FFT (Hann window) and returns levels in logarithmically spaced bands.
class SpectrumAnalyzer
{
public:
    static constexpr int kFftSize = 2048;

    explicit SpectrumAnalyzer(int bands);

    void push(const QAudioBuffer &buffer);
    void pushSamples(const float *mono, size_t count, int sampleRate);
    void clear();

    // Values 0..1 for each band, computed from the newest kFftSize samples.
    void compute(QList<qreal> &out);

private:
    void buildBands();

    int m_bands;
    int m_sampleRate = 44100;
    std::vector<float> m_ring;
    int m_write = 0;
    int m_filled = 0;
    std::vector<float> m_window;
    std::vector<std::complex<float>> m_fft;
    std::vector<int> m_edges;   // band edges in FFT bin indices
};
