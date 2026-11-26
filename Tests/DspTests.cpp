// Copyright (c) 2026 FSMAudio. Todos los derechos reservados. Software propietario: ver LICENSE y EULA.txt.
// Tests de regresión del núcleo DSP (Source/Fet76Compressor.h). Sin JUCE.
// Cada test imprime lo que mide, para poder mirar los números y no solo el OK/FAIL.
#include "Fet76Compressor.h"

#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

using namespace efrt;

static int g_failures = 0, g_checks = 0;
#define CHECK(cond, ...)                                                      \
    do {                                                                      \
        ++g_checks;                                                           \
        if (! (cond)) { ++g_failures; std::printf ("   FAIL  %s:%d  %s  -> ", __FILE__, __LINE__, #cond); std::printf (__VA_ARGS__); std::printf ("\n"); } \
    } while (0)

//------------------------------------------------------------------------------
static std::vector<float> sine (double f, double dbfs, double seconds, double fs, double startSec = 0.0)
{
    const size_t n = (size_t) (seconds * fs);
    std::vector<float> x (n);
    const double a = std::pow (10.0, dbfs / 20.0);
    for (size_t i = 0; i < n; ++i)
        x[i] = (float) (a * std::sin (2.0 * 3.14159265358979323846 * f * ((double) i / fs + startSec)));
    return x;
}

static double peakDb (const std::vector<float>& x, size_t from, size_t to)
{
    float p = 0;
    for (size_t i = from; i < to && i < x.size(); ++i) p = std::max (p, std::abs (x[i]));
    return 20.0 * std::log10 ((double) p + 1e-20);
}

/** Pasa un mono por el compresor (sin saturador). Devuelve audio y la GR por muestra. */
static void renderMono (const std::vector<float>& x, double fs, float inputDb, const Fet76Compressor::Settings& s,
                        std::vector<float>& y, std::vector<float>& gr)
{
    Fet76Compressor c;
    c.prepare (fs, 1);
    c.setSettings (s);
    const float ig = dbToLin (inputDb);
    y.resize (x.size()); gr.resize (x.size());
    for (size_t i = 0; i < x.size(); ++i)
    {
        const float v = x[i] * ig;
        float g;
        c.processSample (&v, &g);
        y[i] = v * g;
        gr[i] = c.getGainReductionDb (0);
    }
}

static size_t firstIndexAbove (const std::vector<float>& v, size_t from, float thr)
{
    for (size_t i = from; i < v.size(); ++i) if (v[i] >= thr) return i;
    return v.size();
}

//------------------------------------------------------------------------------
static void testRatios()
{
    std::printf ("[1] Ratio efectivo = ratio nominal (curva estática, seno 1 kHz, Input 0)\n");
    const double fs = 48000.0;
    const char* names[] = { "4:1", "8:1", "12:1", "20:1", "All" };
    const double nominal[] = { 4, 8, 12, 20, 30 };

    for (int m = 0; m < 5; ++m)
    {
        std::vector<double> xs, ys;
        for (double L = -12.0; L <= -2.0; L += 2.0)
        {
            Fet76Compressor::Settings s; s.ratio = (RatioMode) m; s.attackMs = 0.2f; s.releaseMs = 300.0f;
            std::vector<float> y, gr;
            renderMono (sine (1000.0, L, 1.0, fs), fs, 0.0f, s, y, gr);
            xs.push_back (L); ys.push_back (peakDb (y, (size_t) (0.8 * fs), y.size()));
        }
        // regresión lineal
        double mx = 0, my = 0; for (size_t i = 0; i < xs.size(); ++i) { mx += xs[i]; my += ys[i]; } mx /= xs.size(); my /= ys.size();
        double num = 0, den = 0; for (size_t i = 0; i < xs.size(); ++i) { num += (xs[i] - mx) * (ys[i] - my); den += (xs[i] - mx) * (xs[i] - mx); }
        const double slope = num / den, eff = 1.0 / slope;
        std::printf ("    %-4s pendiente %.3f  -> ratio efectivo %.1f:1  (nominal %.0f:1)\n", names[m], slope, eff, nominal[m]);
        CHECK (std::abs (slope - 1.0 / nominal[m]) < 0.02, "pendiente %.3f vs %.3f esperada", slope, 1.0 / nominal[m]);
    }
    // los 4 botones tienen que ser distinguibles entre sí
    std::printf ("    (los botones 4/8/12/20 ya no son casi idénticos; antes medían 1.6 / 1.9 / 1.94 / 1.92)\n");
}

static void testUnityAndSaturator()
{
    std::printf ("[2] Ganancia unitaria bajo el umbral y saturador\n");
    const double fs = 48000.0;
    {
        Fet76Compressor::Settings s; std::vector<float> y, gr;
        auto x = sine (1000.0, -40.0, 0.5, fs);
        renderMono (x, fs, 0.0f, s, y, gr);
        double maxd = 0; for (size_t i = 0; i < x.size(); ++i) maxd = std::max (maxd, (double) std::abs (x[i] - y[i]));
        std::printf ("    -40 dBFS, compresor solo: max|y-x| = %.2e\n", maxd);
        CHECK (maxd < 1e-6, "no es transparente bajo el umbral");
    }
    {
        FetSaturator sat; sat.prepare (48000.0);
        sat.setColor (0.0f, 0.0f);
        auto x = sine (1000.0, -6.0, 0.2, fs); double maxd = 0;
        for (float v : x) maxd = std::max (maxd, (double) std::abs (sat.process (v) - v));
        std::printf ("    Color = 0: max|y-x| = %.2e (bypass exacto)\n", maxd);
        CHECK (maxd == 0.0, "Color 0 no es bypass exacto");
    }
    for (float color : { 0.25f, 0.5f, 1.0f })
    {
        FetSaturator sat; sat.prepare (fs); sat.setColor (color, 0.0f);
        // ganancia en señal chica
        auto xs = sine (1000.0, -60.0, 0.5, fs); double eIn = 0, eOut = 0;
        for (size_t i = 0; i < xs.size(); ++i) { const float y = sat.process (xs[i]); if (i > xs.size() / 2) { eIn += xs[i] * xs[i]; eOut += y * y; } }
        const double gdb = 10.0 * std::log10 (eOut / eIn);
        // armónicos a -18 dBFS (coherente: 1 kHz = 48 ciclos en 2^... usamos DFT directa de 1 s)
        FetSaturator sat2; sat2.prepare (fs); sat2.setColor (color, 0.0f);
        auto x = sine (1000.0, -18.0, 1.0, fs); std::vector<float> y (x.size());
        for (size_t i = 0; i < x.size(); ++i) y[i] = sat2.process (x[i]);
        auto harm = [&] (int h) {
            double re = 0, im = 0; const size_t a = x.size() / 2;   // 2da mitad: sin transitorio del DC blocker
            for (size_t i = a; i < x.size(); ++i) { const double ph = 2 * 3.14159265358979323846 * 1000.0 * h * (double) i / fs; re += y[i] * std::cos (ph); im += y[i] * std::sin (ph); }
            return 20 * std::log10 (std::sqrt (re * re + im * im) + 1e-20);
        };
        const double h1 = harm (1), h2 = harm (2) - h1, h3 = harm (3) - h1;
        double mean = 0; for (size_t i = x.size() / 2; i < x.size(); ++i) mean += y[i]; mean /= (x.size() / 2.0);
        std::printf ("    Color %.2f: ganancia pequeña señal %+.3f dB | a -18 dBFS: 2º %.1f dBc, 3º %.1f dBc | DC %.1e\n", color, gdb, h2, h3, mean);
        CHECK (std::abs (gdb) < 0.15, "ganancia en señal chica = %.3f dB", gdb);
        CHECK (h2 > -75.0, "casi sin 2º armónico (%.1f dBc): el saturador sigue siendo simétrico", h2);
        CHECK (std::abs (mean) < 2e-3, "DC residual %.2e", mean);
    }
}

static void testNaN()
{
    std::printf ("[3] NaN / Inf no envenenan el estado\n");
    const double fs = 48000.0;
    Fet76Compressor c; c.prepare (fs, 1);
    Fet76Compressor::Settings s; s.ratio = RatioMode::Ratio8; c.setSettings (s);
    const float ig = dbToLin (10.0f);
    float maxGr = 0; int nonFinite = 0;
    for (int i = 0; i < 96000; ++i)
    {
        float v = ig * 0.5f * (float) std::sin (2.0 * 3.14159265358979323846 * 1000.0 * i / fs);
        if (i == 1000) v = std::numeric_limits<float>::quiet_NaN();
        if (i == 1500) v = std::numeric_limits<float>::infinity();
        float g; c.processSample (&v, &g);
        const float out = sanitize (v) * g;
        if (! std::isfinite (out)) ++nonFinite;
        if (i > 48000) maxGr = std::max (maxGr, c.getGainReductionDb (0));
    }
    std::printf ("    GR máx en el 2º segundo = %.2f dB (antes del fix: 0.00 dB), muestras no finitas = %d\n", maxGr, nonFinite);
    CHECK (maxGr > 5.0f, "el compresor quedó muerto tras un NaN");
    CHECK (nonFinite == 0, "salida no finita");
}

static void testTiming()
{
    std::printf ("[4] Attack / release independientes del sample rate\n");
    for (float att : { 0.2f, 0.8f })
    {
        double t63[4]; int k = 0;
        for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
        {
            Fet76Compressor::Settings s; s.ratio = RatioMode::Ratio8; s.attackMs = att; s.releaseMs = 300;
            const double pre = 0.05;
            auto lo = sine (1000.0, -40.0, pre, fs), hi = sine (1000.0, -6.0, 0.4, fs, pre);
            lo.insert (lo.end(), hi.begin(), hi.end());
            std::vector<float> y, gr; renderMono (lo, fs, 0.0f, s, y, gr);
            const float fin = gr.back();
            const size_t i0 = (size_t) (pre * fs), i1 = firstIndexAbove (gr, i0, 0.632f * fin);
            t63[k++] = 1e3 * (double) (i1 - i0) / fs;
        }
        std::printf ("    attack %.1f ms: t63 = %.3f / %.3f / %.3f / %.3f ms  (44.1 / 48 / 96 / 192 kHz)\n", att, t63[0], t63[1], t63[2], t63[3]);
        for (int i = 0; i < 4; ++i)
        {
            CHECK (t63[i] > 0.5 * att && t63[i] < 1.6 * att + 0.05, "t63 = %.3f ms fuera de rango para attack %.1f ms", t63[i], att);
            CHECK (std::abs (t63[i] - t63[3]) < 0.25 * t63[3] + 0.03, "t63 depende del sample rate");
        }
    }
    // release
    double tr[3]; int k = 0; const double fs = 48000.0;
    for (float rel : { 100.0f, 300.0f, 1000.0f })
    {
        Fet76Compressor::Settings s; s.ratio = RatioMode::Ratio8; s.attackMs = 0.2f; s.releaseMs = rel;
        auto hi = sine (1000.0, -6.0, 0.6, fs), lo = sine (1000.0, -60.0, 3.0, fs);
        hi.insert (hi.end(), lo.begin(), lo.end());
        std::vector<float> y, gr; renderMono (hi, fs, 0.0f, s, y, gr);
        const size_t i0 = (size_t) (0.6 * fs); const float g0 = gr[i0 - 1];
        size_t i1 = i0; while (i1 < gr.size() && gr[i1] > 0.1f * g0) ++i1;
        tr[k++] = 1e3 * (double) (i1 - i0) / fs;
        std::printf ("    release %5.0f ms: GR %.1f dB -> 10%% en %.0f ms\n", rel, g0, tr[k - 1]);
    }
    CHECK (tr[0] < tr[1] && tr[1] < tr[2], "el release no es monótono con el knob");
    CHECK (tr[2] > 3.0 * tr[0], "el rango del knob de release es demasiado chico");
}

static void testLink()
{
    std::printf ("[5] Link estéreo (L fuerte, R suave)\n");
    const double fs = 48000.0;
    for (float link : { 1.0f, 0.5f, 0.0f })
    {
        Fet76Compressor c; c.prepare (fs, 2);
        Fet76Compressor::Settings s; s.ratio = RatioMode::Ratio4; s.link = link; c.setSettings (s);
        for (int i = 0; i < 48000; ++i)
        {
            const float l = 0.8f * (float) std::sin (2.0 * 3.14159265358979323846 * 1000.0 * i / fs);
            const float r = 0.02f * (float) std::sin (2.0 * 3.14159265358979323846 * 1000.0 * i / fs);
            const float in[2] = { l, r }; float g[2]; c.processSample (in, g);
        }
        std::printf ("    link %3.0f%%: GR L = %.2f dB, GR R = %.2f dB\n", link * 100, c.getGainReductionDb (0), c.getGainReductionDb (1));
        if (link == 1.0f) CHECK (std::abs (c.getGainReductionDb (0) - c.getGainReductionDb (1)) < 0.01f, "link 100%% no iguala la GR");
        if (link == 0.0f) CHECK (c.getGainReductionDb (1) < 0.01f && c.getGainReductionDb (0) > 3.0f, "link 0%% no es independiente");
        if (link == 0.5f) CHECK (c.getGainReductionDb (1) > 0.5f && c.getGainReductionDb (1) < c.getGainReductionDb (0) - 0.5f, "link 50%% no es intermedio");
    }
}

static void testBassRippleAndHpf()
{
    std::printf ("[6] Rizado de ganancia en graves y HPF de sidechain (8:1, Input +12, release 50 ms)\n");
    const double fs = 48000.0; double rip40 = 0, rip40hpf = 0, rip1k = 0;
    for (int variant = 0; variant < 3; ++variant)
    {
        const double f = variant == 2 ? 1000.0 : 40.0;
        Fet76Compressor::Settings s; s.ratio = RatioMode::Ratio8; s.releaseMs = 50.0f; s.hpfHz = variant == 1 ? 150.0f : 20.0f;
        std::vector<float> y, gr; renderMono (sine (f, -12.0, 2.0, fs), fs, 12.0f, s, y, gr);
        float mn = 1e9f, mx = -1e9f; for (size_t i = (size_t) (1.5 * fs); i < gr.size(); ++i) { mn = std::min (mn, gr[i]); mx = std::max (mx, gr[i]); }
        (variant == 0 ? rip40 : variant == 1 ? rip40hpf : rip1k) = mx - mn;
    }
    std::printf ("    40 Hz: %.2f dB p-p | 40 Hz con HPF 150 Hz: %.2f dB p-p | 1 kHz: %.2f dB p-p   (antes: 1.84 / - / 0.10)\n", rip40, rip40hpf, rip1k);
    CHECK (rip40 < 1.2, "rizado en 40 Hz = %.2f dB", rip40);
    CHECK (rip40hpf < 0.5 * rip40 + 0.05, "el HPF no reduce el rizado");
    CHECK (rip1k < 0.2, "rizado en 1 kHz = %.2f dB", rip1k);

    HighPass12 hp; hp.setCutoff (fs, 150.0f);
    auto resp = [&] (double f) { auto x = sine (f, -20.0, 1.0, fs); double e = 0, e0 = 0; for (size_t i = 0; i < x.size(); ++i) { const float y = hp.process (x[i]); if (i > x.size() / 2) { e += y * y; e0 += x[i] * x[i]; } } return 10 * std::log10 (e / e0); };
    const double a40 = resp (40.0), a1k = resp (1000.0), a150 = resp (150.0);
    std::printf ("    HPF 150 Hz: 40 Hz %.1f dB | 150 Hz %.1f dB | 1 kHz %.2f dB\n", a40, a150, a1k);
    CHECK (a40 < -20.0 && a40 > -32.0, "pendiente del HPF rara (%.1f dB @40 Hz)", a40);
    CHECK (std::abs (a150 + 3.0) < 0.6, "corte no es -3 dB (%.2f)", a150);
    CHECK (std::abs (a1k) < 0.15, "el HPF toca 1 kHz (%.2f dB)", a1k);
}

static void testMatcher()
{
    std::printf ("[7] Auto gain (LoudnessMatcher)\n");
    LoudnessMatcher m; m.prepare (48000.0);
    for (int b = 0; b < 400; ++b) { const double d = 512.0 * 2 * 0.01, w = d * 0.0625; m.pushBlock (d, w, 512, 2); }   // wet = -12 dB (potencia)
    std::printf ("    wet 12 dB abajo -> ganancia %.3f (esperado 4.000 = +12 dB)\n", m.getGain());
    CHECK (std::abs (m.getGain() - 4.0f) < 0.05f, "no converge");
    const float g = m.getGain();
    for (int b = 0; b < 50; ++b) m.pushBlock (0.0, 0.0, 512, 2);                    // silencio: se congela
    CHECK (m.getGain() == g, "no se congela con silencio");
    LoudnessMatcher m2; m2.prepare (48000.0);
    m2.pushBlock (512 * 2 * 0.01, 512 * 2 * 1e-14, 512, 2);                          // wet ~ nada: tope +18 dB
    CHECK (m2.getGain() <= 8.0f, "sin tope");
}

static void testReset()
{
    std::printf ("[8] reset()\n");
    const double fs = 48000.0; Fet76Compressor c; c.prepare (fs, 2);
    Fet76Compressor::Settings s; s.ratio = RatioMode::Ratio20; c.setSettings (s);
    for (int i = 0; i < 20000; ++i) { const float in[2] = { 0.9f * (float) std::sin (0.1 * i), 0.9f * (float) std::sin (0.1 * i) }; float g[2]; c.processSample (in, g); }
    CHECK (c.getMaxGainReductionDb() > 10.0f, "no comprimió");
    c.reset(); CHECK (c.getMaxGainReductionDb() == 0.0f, "reset no limpia la GR");
}

int main()
{
    testRatios(); testUnityAndSaturator(); testNaN(); testTiming(); testLink(); testBassRippleAndHpf(); testMatcher(); testReset();
    std::printf ("\n%d checks, %d fallos\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
