#pragma once

#include <QAudioFormat>
#include <QObject>

#include <vector>

class QAudioBuffer;
class QAudioSink;

// Własny tor wyjściowy: zdekodowane próbki (z QAudioBufferOutput) przechodzą przez korektor
// i trafiają do bufora pierścieniowego, z którego czyta QAudioSink. Bufor ma "poduszkę"
// ~200 ms, żeby chwilowe przycięcia interfejsu nie powodowały trzasków.
// Próbki faktycznie odtworzone są odkładane dla analizatora widma — wizualizer
// pokazuje więc to, co słychać (po korekcji), we właściwym momencie.
class AudioEngine : public QObject
{
    Q_OBJECT

public:
    explicit AudioEngine(QObject *parent = nullptr);
    ~AudioEngine() override;

    QAudioFormat bufferFormat() const;   // format, w jakim chcemy dostawać próbki z odtwarzacza
    int sampleRate() const { return m_format.sampleRate(); }

    void push(const QAudioBuffer &buffer);
    void flush();
    void setPaused(bool paused);
    void setVolume(float linear);

    // Odtworzone próbki (mono) od ostatniego wywołania — dla analizatora widma.
    void takePlayed(std::vector<float> &mono);

private:
    class Ring;

    QAudioFormat m_format;
    QAudioSink *m_sink = nullptr;
    Ring *m_ring = nullptr;
    std::vector<float> m_scratch;
};
