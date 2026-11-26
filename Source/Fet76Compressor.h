// Copyright (c) 2026 FSMAudio. Todos los derechos reservados. Software propietario: ver LICENSE y EULA.txt.
#pragma once
/**
    Fet76Compressor.h
    -----------------
    Núcleo DSP del EFRT. Solo depende de la librería estándar, así
    que se puede compilar y testear por separado (ver Tests/DspTests.cpp).

    Compresor FET de carácter clásico, pensado para rock/post-hardcore (baterías,
    guitarras, voces).

      - Detección feed-forward (sobre la señal de entrada ya empujada por el Input),
        con umbral interno fijo, como el flujo de trabajo del hardware: el knob de
        Input es lo que empuja la señal contra el umbral.
      - La curva estática usa el ratio nominal tal cual: 4:1 comprime 4:1, 20:1
        comprime 20:1 (verificado en Tests/DspTests.cpp). Las versiones anteriores
        aplicaban la pendiente feed-forward dentro de un lazo feedback y los ratios
        efectivos terminaban entre 1.6:1 y 2.2:1.
      - Ballistics en el dominio dB: attack/release son constantes de tiempo reales
        y no dependen del sample rate. Release "program dependent": cuanta más
        reducción hay, más rápido se recupera (constante rápida = 15% de la lenta).
      - Detector de pico con un decaimiento corto (anti-rizado en graves) y HPF de
        sidechain opcional de 12 dB/oct.
      - Link estéreo variable (0-100%) calculado en dB.
      - "All Buttons In": modo agresivo de diseño propio (ratio altísimo, umbral más
        bajo, rodilla casi dura, tiempos más rápidos, más saturación). Es una
        interpretación artística, NO una medición de hardware.
      - Saturador FET/transformador (clase FetSaturator): asimétrico (2º + 3er
        armónico), ganancia unitaria en señal chica, 100% transparente con Color = 0.
      - Robusto: NaN/Inf en la entrada se descartan y nunca envenenan el estado.
*/
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace efrt
{

enum class RatioMode { Ratio4 = 0, Ratio8, Ratio12, Ratio20, AllButtonsIn };

constexpr float kPi = 3.14159265358979323846f;

/** NaN / Inf / valores absurdos -> 0. */
inline float sanitize (float x) noexcept
{
    return (std::abs (x) < 1.0e6f) ? x : 0.0f;
}

inline float linToDb (float lin) noexcept
{
    return 8.685889638f * std::log (std::max (lin, 1.0e-9f));   // 20 / ln(10)
}

inline float dbToLin (float db) noexcept
{
    return std::exp (0.115129255f * db);                        // ln(10) / 20
}

//==============================================================================
/** Forma de la curva estática de cada botón de ratio. */
struct CurveShape
{
    float thresholdDb;
    float ratio;
    float kneeDb;
    float attackScale;     // multiplica el attack del knob
    float releaseScale;    // multiplica el release del knob
    float releaseSpanDb;   // GR a partir de la cual el release es 100% "rápido"
    float extraDrive;      // drive extra del saturador (solo All Buttons In)
};

inline CurveShape curveFor (RatioMode m) noexcept
{
    switch (m)
    {
        case RatioMode::Ratio4:       return { -18.0f,  4.0f, 10.0f, 1.0f, 1.0f, 20.0f, 0.0f };
        case RatioMode::Ratio8:       return { -18.0f,  8.0f,  6.0f, 1.0f, 1.0f, 20.0f, 0.0f };
        case RatioMode::Ratio12:      return { -18.0f, 12.0f,  4.0f, 1.0f, 1.0f, 20.0f, 0.0f };
        case RatioMode::Ratio20:      return { -18.0f, 20.0f,  2.0f, 1.0f, 1.0f, 20.0f, 0.0f };
        case RatioMode::AllButtonsIn: return { -22.0f, 30.0f,  0.5f, 0.6f, 0.5f, 10.0f, 0.8f };
    }
    return { -18.0f, 4.0f, 10.0f, 1.0f, 1.0f, 20.0f, 0.0f };
}

/** Reducción de ganancia estática (dB, >= 0) para un nivel de entrada en dB.
    Rodilla suave centrada en el umbral (cuadrática). Por encima de la rodilla la
    pendiente es exactamente (1 - 1/ratio): el ratio de la curva es el nominal. */
inline float gainReductionDbFor (float levelDb, const CurveShape& c) noexcept
{
    const float d        = levelDb - c.thresholdDb;
    const float slope    = 1.0f - 1.0f / c.ratio;
    const float halfKnee = 0.5f * c.kneeDb;

    if (d <= -halfKnee) return 0.0f;
    if (d >=  halfKnee) return slope * d;

    const float t = d + halfKnee;
    return slope * t * t / (2.0f * c.kneeDb);
}

//==============================================================================
/** Pasa-altos de 12 dB/oct (Butterworth) con topología TPT/SVF. */
class HighPass12
{
public:
    void setCutoff (double fs, float hz) noexcept
    {
        const float g = std::tan (kPi * std::min (hz, (float) (0.45 * fs)) / (float) fs);
        const float k = 1.41421356f;                       // 1/Q con Q = 0.7071
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
        kk = k;
    }

    void reset() noexcept { ic1 = ic2 = 0.0f; }

    float process (float x) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return x - kk * v1 - v2;
    }

private:
    float a1 = 0, a2 = 0, a3 = 0, kk = 1.41421356f;
    float ic1 = 0, ic2 = 0;
};

//==============================================================================
/** Compresor (parte de control): calcula la ganancia lineal por canal, muestra a
    muestra, a partir de la señal de detección. No toca el audio. */
class Fet76Compressor
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr float kMinHpfHz = 20.0f;   // <= este valor el HPF queda apagado

    // Decaimiento del detector de pico = release * 0.5, acotado a [10, 40] ms. Medido:
    // baja el rizado de ganancia en 40 Hz de 1.84 dB p-p (versión anterior) a ~1.1 dB
    // a cambio de ~25 ms más en el release más corto. Para graves de verdad, usar el HPF.
    static constexpr float kDecayMult = 0.5f, kDecayMinMs = 10.0f, kDecayMaxMs = 40.0f;

    struct Settings
    {
        float attackMs  = 0.2f;
        float releaseMs = 300.0f;
        RatioMode ratio = RatioMode::Ratio4;
        float hpfHz     = kMinHpfHz;
        float link      = 1.0f;        // 0..1
    };

    void prepare (double sampleRate, int numChannels) noexcept
    {
        fs       = sampleRate;
        channels = std::max (1, std::min (numChannels, kMaxChannels));
        lastSettingsValid = false;
        setSettings (settings);
        reset();
    }

    void reset() noexcept
    {
        for (auto& c : ch) { c.level = 0.0f; c.gr = 0.0f; c.hpf.reset(); }
    }

    void setSettings (const Settings& s) noexcept
    {
        if (lastSettingsValid && same (s, settings))
            return;

        settings = s;
        lastSettingsValid = true;

        const float attackMs  = std::min (std::max (s.attackMs, 0.02f), 0.8f);
        const float releaseMs = std::min (std::max (s.releaseMs, 50.0f), 1100.0f);
        curve = curveFor (s.ratio);

        const double fsd = fs;
        auto coef = [fsd] (float ms) { return (float) std::exp (-1.0 / (0.001 * (double) ms * fsd)); };

        attackCoef       = coef (attackMs  * curve.attackScale);
        const float rel  = releaseMs * curve.releaseScale;
        releaseCoefSlow  = coef (rel);
        releaseCoefFast  = coef (rel * 0.15f);
        // Decaimiento del detector de pico: atado al release, para que un tono
        // grave no haga "temblar" la ganancia dentro de cada ciclo.
        decayCoef        = coef (std::min (std::max (rel * kDecayMult, kDecayMinMs), kDecayMaxMs));
        invReleaseSpan   = 1.0f / curve.releaseSpanDb;
        kneeFloorLin     = dbToLin (curve.thresholdDb - 0.5f * curve.kneeDb);
        link             = std::min (std::max (s.link, 0.0f), 1.0f);

        hpfActive = s.hpfHz > kMinHpfHz + 0.01f;
        if (hpfActive)
            for (int c = 0; c < channels; ++c)
                ch[(size_t) c].hpf.setCutoff (fs, s.hpfHz);
    }

    /** detIn[c]: señal de detección de cada canal (ya multiplicada por el Input).
        gainLinOut[c]: ganancia lineal a aplicar al audio de ese canal. */
    void processSample (const float* detIn, float* gainLinOut) noexcept
    {
        float lvlMax = 0.0f;

        for (int c = 0; c < channels; ++c)
        {
            auto& s = ch[(size_t) c];
            float v = sanitize (detIn[c]);
            if (hpfActive) v = s.hpf.process (v);

            s.level = std::max (std::abs (v), s.level * decayCoef);
            if (s.level < 1.0e-20f) s.level = 0.0f;           // anti-denormales
            lvlMax = std::max (lvlMax, s.level);
        }

        float target[kMaxChannels] = { 0.0f, 0.0f };

        if (lvlMax >= kneeFloorLin)                           // atajo: sin log si no hay GR posible
        {
            const float dbMax = linToDb (lvlMax);

            for (int c = 0; c < channels; ++c)
            {
                float db = dbMax;
                if (channels > 1 && link < 0.999f)
                {
                    const float own = linToDb (ch[(size_t) c].level);
                    db = own + link * (dbMax - own);
                }
                target[c] = gainReductionDbFor (db, curve);
            }
        }

        for (int c = 0; c < channels; ++c)
        {
            auto& s = ch[(size_t) c];
            const float t = target[c];

            if (t > s.gr)
            {
                s.gr = t + attackCoef * (s.gr - t);
            }
            else
            {
                // Release program dependent: más GR -> constante más rápida.
                const float m    = std::min (s.gr * invReleaseSpan, 1.0f);
                const float coef = releaseCoefSlow + m * (releaseCoefFast - releaseCoefSlow);
                s.gr = t + coef * (s.gr - t);
                if (s.gr < 1.0e-5f && t <= 0.0f) s.gr = 0.0f;
            }

            gainLinOut[c] = dbToLin (-s.gr);
        }
    }

    float getGainReductionDb (int c) const noexcept { return ch[(size_t) c].gr; }
    float getMaxGainReductionDb() const noexcept
    {
        float m = 0.0f;
        for (int c = 0; c < channels; ++c) m = std::max (m, ch[(size_t) c].gr);
        return m;
    }

    const CurveShape& getCurve() const noexcept { return curve; }

private:
    static bool eq (float a, float b) noexcept { return ! (a < b) && ! (b < a); }

    static bool same (const Settings& a, const Settings& b) noexcept
    {
        return eq (a.attackMs, b.attackMs) && eq (a.releaseMs, b.releaseMs) && a.ratio == b.ratio
            && eq (a.hpfHz, b.hpfHz) && eq (a.link, b.link);
    }

    struct ChannelState
    {
        HighPass12 hpf;
        float level = 0.0f;
        float gr    = 0.0f;
    };

    double fs = 44100.0;
    int channels = 2;
    ChannelState ch[kMaxChannels];

    Settings settings;
    bool lastSettingsValid = false;
    CurveShape curve = curveFor (RatioMode::Ratio4);

    float attackCoef = 0, releaseCoefSlow = 0, releaseCoefFast = 0, decayCoef = 0;
    float invReleaseSpan = 0.05f, kneeFloorLin = 0.0f, link = 1.0f;
    bool hpfActive = false;
};

//==============================================================================
/** Saturador tipo FET/transformador.

    y = ( tanh(k (x + b)) - tanh(k b) ) / ( k (1 - tanh^2(k b)) )

    - Ganancia unitaria en señal chica para cualquier k (no cambia el nivel al
      mover Color).
    - El sesgo b lo vuelve asimétrico: genera 2º armónico además del 3º.
    - Un DC blocker (4 Hz) saca el offset que produce la asimetría.
    - Color = 0 es bypass exacto (y = x). Pensado para correr sobre audio
      sobremuestreado. */
class FetSaturator
{
public:
    void prepare (double sampleRate) noexcept
    {
        dcR = 1.0f - 2.0f * kPi * 4.0f / (float) sampleRate;
        reset();
        setColor (0.0f, 0.0f);
    }

    void reset() noexcept { x1 = y1 = 0.0f; }

    /** color01: 0..1. extraDrive: se suma al drive (modo All Buttons In). */
    void setColor (float color01, float extraDrive) noexcept
    {
        enabled = color01 > 0.0005f;
        if (! enabled) return;

        k    = 0.4f + 2.2f * std::min (color01, 1.0f) + extraDrive;
        th   = std::tanh (k * kBias);
        norm = 1.0f / (k * (1.0f - th * th));
    }

    bool isEnabled() const noexcept { return enabled; }

    float process (float x) noexcept
    {
        if (! enabled)
        {
            x1 = x; y1 = x;                    // mantiene el DC blocker "en fase" para
            return x;                          // que reactivar Color no haga click
        }

        const float y   = (std::tanh (k * (x + kBias)) - th) * norm;
        const float out = y - x1 + dcR * y1;
        x1 = y;
        y1 = out;
        return out;
    }

private:
    static constexpr float kBias = 0.15f;

    bool  enabled = false;
    float k = 1.0f, th = 0.0f, norm = 1.0f, dcR = 0.999f;
    float x1 = 0.0f, y1 = 0.0f;
};

//==============================================================================
/** Auto gain: iguala la sonoridad (potencia media lenta) de la salida con la de
    la entrada. Va en bloques, con ventana creciente al arrancar para converger
    rápido, y se congela con silencio. */
class LoudnessMatcher
{
public:
    void prepare (double sampleRate) noexcept { fs = sampleRate; reset(); }

    void reset() noexcept { msDry = msWet = 0.0; seconds = 0.0; gain = 1.0f; }

    /** sumDry / sumWet: suma de cuadrados (todos los canales); count: nº de muestras
        (por canal) acumuladas. */
    void pushBlock (double sumDry, double sumWet, int count, int numChannels) noexcept
    {
        if (count <= 0) return;
        const double n = (double) count * (double) numChannels;
        const double d = sumDry / n, w = sumWet / n;

        if (d < 1.0e-9) return;                                   // silencio: congelar

        const double dt = (double) count / fs;
        const double window = std::min (seconds + dt, kTauSeconds);
        const double a = dt / (window + dt);                      // promedio acumulado al inicio
        seconds += dt;

        msDry += a * (d - msDry);
        msWet += a * (w - msWet);

        if (msWet > 1.0e-12 && msDry > 1.0e-12)
        {
            const float g = (float) std::sqrt (msDry / msWet);
            gain = std::min (std::max (g, kMinGain), kMaxGain);
        }
    }

    float getGain() const noexcept { return gain; }

private:
    static constexpr double kTauSeconds = 2.0;
    static constexpr float  kMinGain = 0.125f, kMaxGain = 8.0f;   // +-18 dB

    double fs = 44100.0, msDry = 0, msWet = 0, seconds = 0;
    float gain = 1.0f;
};

} // namespace efrt

// Alias por compatibilidad con el código anterior (RatioMode en el scope global).
using efrt::RatioMode;
