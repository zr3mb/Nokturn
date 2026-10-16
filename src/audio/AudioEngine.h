#pragma once

#include <QAudioFormat>
#include <QObject>

#include <array>
#include <vector>

class QAudioBuffer;
class QAudioSink;

// Własny tor wyjściowy: zdekodowane próbki (z QAudioBufferOutput) przechodzą przez korektor
// i trafiają do bufora pierścieniowego, z którego czyta QAudioSink. Bufor ma "poduszkę"
// ~200 ms, żeby chwilowe przycięcia interfejsu nie powodowały trzasków.
// Próbki faktycznie odtworzone są odkładane dla analizatora widma — wizualizer
// pokazuje więc to, co słychać (po korekcji), we właściwym momencie.
//
// Miksowanie: odtwarzacz ma dwa "decki" (dwa QMediaPlayer). Próbki każdego trafiają do osobnej
// kolejki; poza przejściem gra tylko aktywny deck, a w trakcie przejścia oba są sumowane
// z obwiedniami zależnymi od stylu (przenikanie, filtr, zamiana basu, echo). Miks powstaje
// przed korektorem, więc korektor, limiter i wizualizer działają na tym, co słychać.
class AudioEngine : public QObject
{
    Q_OBJECT

public:
    static constexpr int kDecks = 2;

    enum class Style { Fade, Rise, BassSwap, Echo };

    struct Transition
    {
        Style style = Style::Fade;
        double seconds = 6.0;
        bool trimIntro = true;     // pomiń ciszę na początku nowego utworu
    };

    explicit AudioEngine(QObject *parent = nullptr);
    ~AudioEngine() override;

    QAudioFormat bufferFormat() const;   // format, w jakim chcemy dostawać próbki z odtwarzacza
    int sampleRate() const { return m_format.sampleRate(); }

    void push(int deck, const QAudioBuffer &buffer);
    void flush();                        // czyści wszystko (zmiana utworu przez użytkownika, przewinięcie)
    void dropDeck(int deck);             // porzuca próbki decku (np. zatrzymanego)
    void setActiveDeck(int deck);
    int activeDeck() const { return m_active; }
    void setPaused(bool paused);
    void setVolume(float linear);

    // Przejście z `from` do `to`. Obwiednia rusza, gdy nadejdą pierwsze próbki nowego utworu
    // (do tego czasu stary gra normalnie), więc opóźnienie startu nie powoduje dziury.
    void beginTransition(int from, int to, const Transition &t);
    void finishTransitionNow();          // np. przewinięcie w trakcie — stary deck milknie od razu
    void setDeckEnded(int deck);         // deck doszedł do końca pliku: brak próbek = cisza, nie czekamy
    bool inTransition() const { return m_tr.active; }
    double transitionProgress() const;

    // Głośność decku (dBFS): krótkookresowa (~0,1 s) i średnia od początku utworu — do wykrywania
    // ciszy i naturalnych wyciszeń na końcu nagrania.
    double shortLevelDb(int deck) const;
    double averageLevelDb(int deck) const;
    void resetLevels(int deck);

    // Odtworzone próbki (mono) od ostatniego wywołania — dla analizatora widma.
    void takePlayed(std::vector<float> &mono);

signals:
    void transitionFinished(int fromDeck);

private:
    class Ring;

    struct Stage
    {
        std::vector<float> data;    // przeplatane próbki
        size_t read = 0;            // indeks pierwszej nieodczytanej ramki
        bool ended = false;
        size_t frames(int ch) const { return (data.size() / size_t(ch)) - read; }
        const float *at(size_t frame, int ch) const { return data.data() + (read + frame) * size_t(ch); }
        void consume(size_t n, int ch);
        void clear() { data.clear(); read = 0; ended = false; }
    };

    struct Levels
    {
        double shortEnergy = 0;     // wygładzona energia (eksponencjalnie)
        double sumEnergy = 0;
        double frames = 0;
    };

    struct TransitionState
    {
        bool active = false;
        bool started = false;       // pierwsze (nieciche) próbki nowego utworu już są
        int from = 0;
        int to = 1;
        Transition t;
        size_t length = 0;          // ramki
        size_t pos = 0;
        size_t trimmed = 0;
        // stan efektów (na kanał)
        std::array<double, 2> hpX1 {}, hpX2 {}, hpY1 {}, hpY2 {};
        std::array<double, 2> lpA {}, lpA2 {}, lpB {}, lpB2 {};
        std::vector<float> echo;
        size_t echoPos = 0;
    };

    void pump();
    void mix(size_t frames);
    void emit_(const float *frames, size_t count);
    void finishTransition();

    QAudioFormat m_format;
    QAudioSink *m_sink = nullptr;
    Ring *m_ring = nullptr;
    std::vector<float> m_scratch;
    std::vector<float> m_out;
    std::array<Stage, kDecks> m_stage;
    std::array<Levels, kDecks> m_levels;
    int m_active = 0;
    TransitionState m_tr;
};
