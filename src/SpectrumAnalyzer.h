#pragma once

#include <QList>

#include <complex>
#include <vector>

class QAudioBuffer;

// Analiza widma odtwarzanego dźwięku: zbiera próbki z QAudioBufferOutput,
// liczy FFT (okno Hanna) i zwraca poziomy w pasmach rozłożonych logarytmicznie.
class SpectrumAnalyzer
{
public:
    static constexpr int kFftSize = 2048;

    explicit SpectrumAnalyzer(int bands);

    void push(const QAudioBuffer &buffer);
    void pushSamples(const float *mono, size_t count, int sampleRate);
    void clear();

    // Wartości 0..1 dla każdego pasma, wyliczone z najnowszych kFftSize próbek.
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
    std::vector<int> m_edges;   // granice pasm w indeksach binów FFT
};
