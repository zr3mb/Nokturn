#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <array>

class QQmlEngine;
class QJSEngine;

// Parametric equalizer in the style of "Fruity Parametric EQ": 7 bands (RBJ biquad filters),
// a preamp and a limiter that protects against clipping.
// Parameters are changed from QML (GUI thread); audio processing also runs on the GUI thread,
// in AudioEngine::push(), so no locks are needed.
class Equalizer : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(bool limiter READ limiter WRITE setLimiter NOTIFY changed)
    Q_PROPERTY(double preamp READ preamp WRITE setPreamp NOTIFY changed)
    Q_PROPERTY(QVariantList bands READ bands NOTIFY changed)
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    // Responses for the graph: 180 points spaced logarithmically from 20 Hz to 20 kHz (dB).
    Q_PROPERTY(QVariantList curve READ curve NOTIFY changed)
    Q_PROPERTY(QVariantList bandCurves READ bandCurves NOTIFY changed)
    Q_PROPERTY(QString preset READ preset NOTIFY changed)
    Q_PROPERTY(QStringList presets READ presets CONSTANT)
    Q_PROPERTY(double maxGain READ maxGain CONSTANT)
    Q_PROPERTY(double maxPreamp READ maxPreamp CONSTANT)
    Q_PROPERTY(qreal peak READ peak NOTIFY metersChanged)            // dBFS after EQ, before the limiter
    Q_PROPERTY(qreal reduction READ reduction NOTIFY metersChanged)  // how many dB the limiter takes off
    Q_PROPERTY(bool clipping READ clipping NOTIFY metersChanged)

public:
    enum Type { LowCut, LowShelf, Bell, HighShelf, HighCut, Notch, BandPass };
    Q_ENUM(Type)

    static constexpr int kBands = 7;
    static constexpr int kMaxStages = 4;      // slope of cut filters: 12 / 24 / 48 dB/oct.
    static constexpr int kMaxChannels = 8;

    struct Band
    {
        bool enabled = true;
        int type = Bell;
        double freq = 1000.0;
        double gain = 0.0;
        double q = 0.9;
        int slope = 1;                         // number of biquad sections for Low/High Cut (1, 2, 4)
    };

    static Equalizer *instance();
    static Equalizer *create(QQmlEngine *, QJSEngine *);

    bool enabled() const { return m_enabled; }
    bool limiter() const { return m_limiter; }
    double preamp() const { return m_preamp; }
    QVariantList bands() const;
    int revision() const { return m_revision; }
    QVariantList curve() const { return m_curve; }
    QVariantList bandCurves() const { return m_bandCurves; }
    QString preset() const { return m_preset; }
    QStringList presets() const;
    Q_INVOKABLE QString presetLabel(const QString &id) const;   // translated preset name ("Custom" included)
    double maxGain() const { return 30.0; }
    double maxPreamp() const { return 24.0; }
    qreal peak() const { return m_peakDb; }
    qreal reduction() const { return m_reductionDb; }
    bool clipping() const { return m_clipHold > 0; }

    void setEnabled(bool on);
    void setLimiter(bool on);
    void setPreamp(double db);

    Q_INVOKABLE void setBandType(int band, int type);
    Q_INVOKABLE void setBandFreq(int band, double hz);
    Q_INVOKABLE void setBandGain(int band, double db);
    Q_INVOKABLE void setBandQ(int band, double q);
    Q_INVOKABLE void setBandEnabled(int band, bool on);
    Q_INVOKABLE void setBandSlope(int band, int slope);
    Q_INVOKABLE void resetBand(int band);
    Q_INVOKABLE void applyPreset(const QString &name);

    // Response (dB) at `points` points spaced logarithmically from 20 Hz to 20 kHz.
    Q_INVOKABLE QVariantList response(int points) const;
    Q_INVOKABLE QVariantList bandResponse(int band, int points) const;

    // --- audio path ---
    void setSampleRate(int rate);
    void process(float *interleaved, qsizetype frames, int channels);
    void resetState();
    void tickMeters(double dt);

signals:
    void changed();
    void metersChanged();

private:
    struct Coeffs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
    struct State { double x1 = 0, x2 = 0, y1 = 0, y2 = 0; };

    explicit Equalizer(QObject *parent = nullptr);

    static Coeffs design(const Band &band, double rate);
    static double magnitudeDb(const Coeffs &c, double hz, double rate);
    static int stages(const Band &band);
    bool active(const Band &band) const;
    void rebuild(int band);
    void touch(bool keepPreset = false);
    void updateCurves();
    void save() const;
    void load();
    void setBands(const std::array<Band, kBands> &bands);

    std::array<Band, kBands> m_bands;
    std::array<Coeffs, kBands> m_coeffs;
    std::array<std::array<std::array<State, kMaxChannels>, kMaxStages>, kBands> m_state {};
    double m_rate = 48000.0;
    bool m_enabled = true;
    bool m_limiter = true;
    double m_preamp = 0.0;
    QString m_preset;
    int m_revision = 0;
    QVariantList m_curve;
    QVariantList m_bandCurves;

    // limiter and meters
    double m_limGain = 1.0;
    float m_blockPeak = 0.0f;
    double m_blockReduction = 0.0;
    double m_peakDb = -90.0;
    double m_reductionDb = 0.0;
    double m_clipHold = 0.0;
};
