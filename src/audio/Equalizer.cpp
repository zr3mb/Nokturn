#include "Equalizer.h"

#include "AppPaths.h"

#include <QHash>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <complex>

namespace {

using Band = Equalizer::Band;

constexpr double kPi = 3.14159265358979323846;

std::array<Band, Equalizer::kBands> flatBands()
{
    return { {
        { false, Equalizer::LowCut,    30.0,    0.0, 0.71, 2 },
        { true,  Equalizer::LowShelf,  90.0,    0.0, 0.71, 1 },
        { true,  Equalizer::Bell,      250.0,   0.0, 0.9,  1 },
        { true,  Equalizer::Bell,      1000.0,  0.0, 0.9,  1 },
        { true,  Equalizer::Bell,      3500.0,  0.0, 0.9,  1 },
        { true,  Equalizer::HighShelf, 9000.0,  0.0, 0.71, 1 },
        { false, Equalizer::HighCut,   18000.0, 0.0, 0.71, 2 },
    } };
}

// Preset ids are English; the displayed name goes through tr() (Equalizer::presetLabel).
struct Preset { const char *name; double preamp; void (*apply)(std::array<Band, Equalizer::kBands> &); };

const Preset kPresets[] = {
    { QT_TR_NOOP("Flat"), 0.0, [](auto &) {} },
    { QT_TR_NOOP("Bass boost"), 0.0, [](auto &b) {
        b[1].gain = 8.0; b[2].gain = -1.5;
    } },
    { QT_TR_NOOP("Extreme bass"), 0.0, [](auto &b) {
        b[1].freq = 70.0; b[1].gain = 18.0;
        b[2].freq = 50.0; b[2].gain = 6.0; b[2].q = 1.2;
        b[3].freq = 300.0; b[3].gain = -4.0; b[3].q = 0.8;
    } },
    { QT_TR_NOOP("V-shape"), 0.0, [](auto &b) {
        b[1].gain = 6.0; b[3].gain = -4.0; b[3].q = 0.6; b[5].gain = 6.0;
    } },
    { QT_TR_NOOP("Vocal"), 0.0, [](auto &b) {
        b[0].enabled = true; b[0].freq = 80.0;
        b[2].gain = -2.0; b[4].freq = 3000.0; b[4].gain = 4.0; b[4].q = 1.0; b[5].gain = 2.0;
    } },
    { QT_TR_NOOP("Treble"), 0.0, [](auto &b) {
        b[4].gain = 3.0; b[5].freq = 6000.0; b[5].gain = 8.0;
    } },
    { QT_TR_NOOP("Loudness+"), 12.0, [](auto &b) {
        b[1].gain = 3.0; b[5].gain = 2.0;
    } },
    { QT_TR_NOOP("Lo-fi radio"), 4.0, [](auto &b) {
        b[0].enabled = true; b[0].freq = 300.0; b[0].slope = 4;
        b[3].freq = 1200.0; b[3].gain = 4.0;
        b[6].enabled = true; b[6].freq = 3500.0; b[6].slope = 4;
    } },
};


// Presets were saved under their Polish names before 0.3 — map them to the current ids.
QString migratePresetName(const QString &name)
{
    static const QHash<QString, QString> old {
        { QStringLiteral("P\u0142asko"), QStringLiteral("Flat") },   // i18n-ignore
        { QStringLiteral("Ekstremalny bas"), QStringLiteral("Extreme bass") },   // i18n-ignore
        { QStringLiteral("Wokal"), QStringLiteral("Vocal") },   // i18n-ignore
        { QStringLiteral("Soprany"), QStringLiteral("Treble") },   // i18n-ignore
        { QStringLiteral("G\u0142o\u015bno\u015b\u0107+"), QStringLiteral("Loudness+") },   // i18n-ignore
        { QStringLiteral("Radio lo-fi"), QStringLiteral("Lo-fi radio") },
        { QStringLiteral("W\u0142asne"), QStringLiteral("Custom") },   // i18n-ignore
    };
    return old.value(name, name);
}
} // namespace

Equalizer *Equalizer::instance()
{
    static Equalizer *inst = new Equalizer;
    return inst;
}

Equalizer *Equalizer::create(QQmlEngine *, QJSEngine *)
{
    Equalizer *eq = instance();
    QJSEngine::setObjectOwnership(eq, QJSEngine::CppOwnership);
    return eq;
}

Equalizer::Equalizer(QObject *parent)
    : QObject(parent)
    , m_bands(flatBands())
{
    m_preset = QStringLiteral("Flat");
    load();
    for (int b = 0; b < kBands; ++b)
        rebuild(b);
    updateCurves();
}

// ---- parameters ---------------------------------------------------------------

QVariantList Equalizer::bands() const
{
    QVariantList out;
    for (int i = 0; i < kBands; ++i) {
        const Band &b = m_bands[i];
        out << QVariantMap {
            { "index", i }, { "enabled", b.enabled }, { "type", b.type }, { "freq", b.freq },
            { "gain", b.gain }, { "q", b.q }, { "slope", b.slope },
            { "usesGain", b.type == LowShelf || b.type == Bell || b.type == HighShelf },
        };
    }
    return out;
}

QStringList Equalizer::presets() const
{
    QStringList out;
    for (const Preset &p : kPresets)
        out << QString::fromUtf8(p.name);
    return out;
}

QString Equalizer::presetLabel(const QString &id) const
{
    if (id == QLatin1String("Custom"))
        return tr("Custom");
    return tr(id.toUtf8().constData());
}

void Equalizer::setEnabled(bool on)
{
    if (on == m_enabled)
        return;
    m_enabled = on;
    resetState();
    touch(true);
}

void Equalizer::setLimiter(bool on)
{
    if (on == m_limiter)
        return;
    m_limiter = on;
    m_limGain = 1.0;
    touch(true);
}

void Equalizer::setPreamp(double db)
{
    db = std::clamp(db, -maxPreamp(), maxPreamp());
    if (std::abs(db - m_preamp) < 1e-6)
        return;
    m_preamp = db;
    touch();
}

void Equalizer::setBandType(int band, int type)
{
    if (band < 0 || band >= kBands || type < LowCut || type > BandPass || m_bands[band].type == type)
        return;
    m_bands[band].type = type;
    rebuild(band);
    touch();
}

void Equalizer::setBandFreq(int band, double hz)
{
    if (band < 0 || band >= kBands)
        return;
    m_bands[band].freq = std::clamp(hz, 20.0, 20000.0);
    rebuild(band);
    touch();
}

void Equalizer::setBandGain(int band, double db)
{
    if (band < 0 || band >= kBands)
        return;
    m_bands[band].gain = std::clamp(db, -maxGain(), maxGain());
    rebuild(band);
    touch();
}

void Equalizer::setBandQ(int band, double q)
{
    if (band < 0 || band >= kBands)
        return;
    m_bands[band].q = std::clamp(q, 0.1, 18.0);
    rebuild(band);
    touch();
}

void Equalizer::setBandEnabled(int band, bool on)
{
    if (band < 0 || band >= kBands || m_bands[band].enabled == on)
        return;
    m_bands[band].enabled = on;
    rebuild(band);
    touch();
}

void Equalizer::setBandSlope(int band, int slope)
{
    if (band < 0 || band >= kBands)
        return;
    m_bands[band].slope = slope >= 4 ? 4 : slope >= 2 ? 2 : 1;
    rebuild(band);
    touch();
}

void Equalizer::resetBand(int band)
{
    if (band < 0 || band >= kBands)
        return;
    m_bands[band] = flatBands()[band];
    rebuild(band);
    touch();
}

void Equalizer::applyPreset(const QString &name)
{
    for (const Preset &p : kPresets) {
        if (name == QString::fromUtf8(p.name)) {
            auto bands = flatBands();
            p.apply(bands);
            m_preamp = p.preamp;
            setBands(bands);
            m_preset = name;
            touch(true);
            return;
        }
    }
}

void Equalizer::setBands(const std::array<Band, kBands> &bands)
{
    m_bands = bands;
    for (int b = 0; b < kBands; ++b)
        rebuild(b);
}

void Equalizer::touch(bool keepPreset)
{
    if (!keepPreset)
        m_preset = QStringLiteral("Custom");
    ++m_revision;
    updateCurves();
    save();
    emit changed();
}

void Equalizer::updateCurves()
{
    constexpr int points = 180;
    m_curve = response(points);
    m_bandCurves.clear();
    for (int b = 0; b < kBands; ++b)
        m_bandCurves << QVariant(bandResponse(b, points));
}

// ---- filter design (RBJ Audio EQ Cookbook) ------------------------------------

int Equalizer::stages(const Band &band)
{
    return band.type == LowCut || band.type == HighCut ? std::clamp(band.slope, 1, kMaxStages) : 1;
}

bool Equalizer::active(const Band &band) const
{
    if (!band.enabled)
        return false;
    const bool gainType = band.type == LowShelf || band.type == Bell || band.type == HighShelf;
    return !gainType || std::abs(band.gain) > 0.01;
}

Equalizer::Coeffs Equalizer::design(const Band &band, double rate)
{
    const double f = std::clamp(band.freq, 10.0, rate * 0.49);
    const double w0 = 2.0 * kPi * f / rate;
    const double cs = std::cos(w0);
    const double sn = std::sin(w0);
    const double q = std::clamp(band.q, 0.1, 30.0);
    const double alpha = sn / (2.0 * q);
    const double A = std::pow(10.0, band.gain / 40.0);
    const double sqA = std::sqrt(A);

    double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;
    switch (band.type) {
    case LowCut:
        b0 = (1 + cs) / 2; b1 = -(1 + cs); b2 = (1 + cs) / 2;
        a0 = 1 + alpha; a1 = -2 * cs; a2 = 1 - alpha;
        break;
    case HighCut:
        b0 = (1 - cs) / 2; b1 = 1 - cs; b2 = (1 - cs) / 2;
        a0 = 1 + alpha; a1 = -2 * cs; a2 = 1 - alpha;
        break;
    case Bell:
        b0 = 1 + alpha * A; b1 = -2 * cs; b2 = 1 - alpha * A;
        a0 = 1 + alpha / A; a1 = -2 * cs; a2 = 1 - alpha / A;
        break;
    case LowShelf:
        b0 = A * ((A + 1) - (A - 1) * cs + 2 * sqA * alpha);
        b1 = 2 * A * ((A - 1) - (A + 1) * cs);
        b2 = A * ((A + 1) - (A - 1) * cs - 2 * sqA * alpha);
        a0 = (A + 1) + (A - 1) * cs + 2 * sqA * alpha;
        a1 = -2 * ((A - 1) + (A + 1) * cs);
        a2 = (A + 1) + (A - 1) * cs - 2 * sqA * alpha;
        break;
    case HighShelf:
        b0 = A * ((A + 1) + (A - 1) * cs + 2 * sqA * alpha);
        b1 = -2 * A * ((A - 1) + (A + 1) * cs);
        b2 = A * ((A + 1) + (A - 1) * cs - 2 * sqA * alpha);
        a0 = (A + 1) - (A - 1) * cs + 2 * sqA * alpha;
        a1 = 2 * ((A - 1) - (A + 1) * cs);
        a2 = (A + 1) - (A - 1) * cs - 2 * sqA * alpha;
        break;
    case Notch:
        b0 = 1; b1 = -2 * cs; b2 = 1;
        a0 = 1 + alpha; a1 = -2 * cs; a2 = 1 - alpha;
        break;
    case BandPass:
        b0 = alpha; b1 = 0; b2 = -alpha;
        a0 = 1 + alpha; a1 = -2 * cs; a2 = 1 - alpha;
        break;
    }
    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}

double Equalizer::magnitudeDb(const Coeffs &c, double hz, double rate)
{
    const double w = 2.0 * kPi * hz / rate;
    const std::complex<double> z1 = std::polar(1.0, -w);
    const std::complex<double> z2 = z1 * z1;
    const std::complex<double> num = c.b0 + c.b1 * z1 + c.b2 * z2;
    const std::complex<double> den = 1.0 + c.a1 * z1 + c.a2 * z2;
    return 20.0 * std::log10(std::max(std::abs(num) / std::abs(den), 1e-9));
}

void Equalizer::rebuild(int band)
{
    m_coeffs[band] = design(m_bands[band], m_rate);
}

QVariantList Equalizer::response(int points) const
{
    QVariantList out;
    points = std::max(points, 2);
    for (int i = 0; i < points; ++i) {
        const double f = 20.0 * std::pow(1000.0, double(i) / (points - 1));
        double db = 0.0;
        if (m_enabled) {
            for (int b = 0; b < kBands; ++b)
                if (active(m_bands[b]))
                    db += stages(m_bands[b]) * magnitudeDb(m_coeffs[b], f, m_rate);
        }
        out << db;
    }
    return out;
}

QVariantList Equalizer::bandResponse(int band, int points) const
{
    QVariantList out;
    if (band < 0 || band >= kBands)
        return out;
    points = std::max(points, 2);
    const Band &b = m_bands[band];
    for (int i = 0; i < points; ++i) {
        const double f = 20.0 * std::pow(1000.0, double(i) / (points - 1));
        out << (b.enabled ? stages(b) * magnitudeDb(m_coeffs[band], f, m_rate) : 0.0);
    }
    return out;
}

// ---- processing ----------------------------------------------------------------

void Equalizer::setSampleRate(int rate)
{
    if (rate <= 0 || std::abs(rate - m_rate) < 0.5)
        return;
    m_rate = rate;
    for (int b = 0; b < kBands; ++b)
        rebuild(b);
    resetState();
    updateCurves();
    emit changed();
}

void Equalizer::resetState()
{
    for (auto &band : m_state)
        for (auto &stage : band)
            stage.fill(State {});
    m_limGain = 1.0;
}

void Equalizer::process(float *interleaved, qsizetype frames, int channels)
{
    if (!m_enabled || channels <= 0)
        return;   // bypass — the signal is unchanged

    const int ch = std::min(channels, kMaxChannels);
    const double pre = std::pow(10.0, m_preamp / 20.0);
    const double threshold = 0.966;                                   // -0.3 dBFS
    const double release = 1.0 - std::exp(-1.0 / (0.15 * m_rate));   // ~150 ms

    std::array<bool, kBands> on {};
    std::array<int, kBands> nStages {};
    for (int b = 0; b < kBands; ++b) {
        on[b] = active(m_bands[b]);
        nStages[b] = stages(m_bands[b]);
    }

    float blockPeak = 0.0f;
    double minGain = 1.0;
    double values[kMaxChannels];

    for (qsizetype f = 0; f < frames; ++f) {
        float *frame = interleaved + f * channels;
        double framePeak = 0.0;
        for (int c = 0; c < ch; ++c) {
            double x = frame[c];
            for (int b = 0; b < kBands; ++b) {
                if (!on[b])
                    continue;
                const Coeffs &k = m_coeffs[b];
                for (int s = 0; s < nStages[b]; ++s) {
                    State &st = m_state[b][s][c];
                    const double y = k.b0 * x + k.b1 * st.x1 + k.b2 * st.x2 - k.a1 * st.y1 - k.a2 * st.y2;
                    st.x2 = st.x1; st.x1 = x;
                    st.y2 = st.y1; st.y1 = y;
                    x = y;
                }
            }
            x *= pre;
            values[c] = x;
            framePeak = std::max(framePeak, std::abs(x));
        }
        blockPeak = std::max(blockPeak, float(framePeak));

        // Peak limiter (instant attack, gentle release, linked channels).
        double g = 1.0;
        if (m_limiter) {
            const double target = framePeak > threshold ? threshold / framePeak : 1.0;
            if (target < m_limGain)
                m_limGain = target;
            else
                m_limGain += (1.0 - m_limGain) * release;
            g = m_limGain;
            minGain = std::min(minGain, g);
        }
        // Without the limiter the excess is simply clipped — that's audible distortion.
        for (int c = 0; c < ch; ++c)
            frame[c] = float(std::clamp(values[c] * g, -1.0, 1.0));
    }

    m_blockPeak = std::max(m_blockPeak, blockPeak);
    m_blockReduction = std::max(m_blockReduction, -20.0 * std::log10(minGain));
    if (!m_limiter && blockPeak > 1.0f)
        m_clipHold = 1.2;
}

void Equalizer::tickMeters(double dt)
{
    const double oldPeak = m_peakDb, oldRed = m_reductionDb;
    const bool oldClip = clipping();

    const double peakNow = 20.0 * std::log10(std::max<double>(m_blockPeak, 1e-5));
    m_peakDb = peakNow > m_peakDb ? peakNow : std::max(-60.0, m_peakDb - 30.0 * dt);
    m_reductionDb = m_blockReduction > m_reductionDb ? m_blockReduction : std::max(0.0, m_reductionDb - 20.0 * dt);
    m_clipHold = std::max(0.0, m_clipHold - dt);
    m_blockPeak = 0.0f;
    m_blockReduction = 0.0;

    if (std::abs(oldPeak - m_peakDb) > 0.05 || std::abs(oldRed - m_reductionDb) > 0.05 || oldClip != clipping())
        emit metersChanged();
}

// ---- saving ---------------------------------------------------------------------

void Equalizer::save() const
{
    QJsonArray bands;
    for (const Band &b : m_bands) {
        bands.append(QJsonObject {
            { "enabled", b.enabled }, { "type", b.type }, { "freq", b.freq },
            { "gain", b.gain }, { "q", b.q }, { "slope", b.slope } });
    }
    const QJsonObject root {
        { "enabled", m_enabled }, { "limiter", m_limiter }, { "preamp", m_preamp },
        { "preset", m_preset }, { "bands", bands } };
    AppPaths::settings()->setValue("eq/state", QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));
}

void Equalizer::load()
{
    const QString json = AppPaths::settings()->value("eq/state").toString();
    const QJsonObject root = QJsonDocument::fromJson(json.toUtf8()).object();
    if (root.isEmpty())
        return;
    m_enabled = root.value("enabled").toBool(true);
    m_limiter = root.value("limiter").toBool(true);
    m_preamp = std::clamp(root.value("preamp").toDouble(), -maxPreamp(), maxPreamp());
    m_preset = migratePresetName(root.value("preset").toString(QStringLiteral("Custom")));
    const QJsonArray bands = root.value("bands").toArray();
    for (int i = 0; i < kBands && i < bands.size(); ++i) {
        const QJsonObject o = bands.at(i).toObject();
        Band &b = m_bands[i];
        b.enabled = o.value("enabled").toBool(b.enabled);
        b.type = std::clamp(o.value("type").toInt(b.type), int(LowCut), int(BandPass));
        b.freq = std::clamp(o.value("freq").toDouble(b.freq), 20.0, 20000.0);
        b.gain = std::clamp(o.value("gain").toDouble(b.gain), -maxGain(), maxGain());
        b.q = std::clamp(o.value("q").toDouble(b.q), 0.1, 18.0);
        b.slope = o.value("slope").toInt(b.slope);
    }
}
