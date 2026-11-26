// Copyright (c) 2026 FSMAudio. Todos los derechos reservados. Software propietario: ver LICENSE y EULA.txt.
// Tests del procesador completo (PluginProcessor.cpp) sin host ni UI.
// Imprime lo que mide; sale con código != 0 si algún chequeo falla.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"

#include <cmath>
#include <complex>
#include <cstdio>
#include <limits>
#include <vector>

using Proc  = ElFratachoAudioProcessor;
using Audio = std::vector<std::vector<float>>;

static int g_failures = 0, g_checks = 0;
#define CHECK(cond, ...)                                                      \
    do {                                                                      \
        ++g_checks;                                                           \
        if (! (cond)) { ++g_failures; std::printf ("   FAIL  line %d  %s  -> ", __LINE__, #cond); std::printf (__VA_ARGS__); std::printf ("\n"); } \
    } while (0)

//------------------------------------------------------------------------------
static juce::AudioChannelSet setFor (int n)
{
    return n == 0 ? juce::AudioChannelSet::disabled() : (n == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
}

static void setParam (Proc& p, const char* id, float plainValue)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plainValue));
}

using ParamList = std::initializer_list<std::pair<const char*, float>>;

static std::unique_ptr<Proc> makeProc (int mainCh, int scCh, double fs, int block, ParamList params)
{
    auto p = std::make_unique<Proc>();
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (setFor (mainCh));
    layout.inputBuses.add (setFor (scCh));
    layout.outputBuses.add (setFor (mainCh));
    const bool ok = p->setBusesLayout (layout);
    if (! ok) std::printf ("   (!) setBusesLayout rechazó mainCh=%d scCh=%d\n", mainCh, scCh);

    for (auto& kv : params) setParam (*p, kv.first, kv.second);

    p->setRateAndBufferSizeDetails (fs, block);
    p->prepareToPlay (fs, block);
    return p;
}

static Audio sineAudio (int ch, double f, double dbfs, int n, double fs, double phase = 0.0)
{
    Audio a ((size_t) ch, std::vector<float> ((size_t) n));
    const double amp = std::pow (10.0, dbfs / 20.0);
    for (int c = 0; c < ch; ++c)
        for (int i = 0; i < n; ++i)
            a[(size_t) c][(size_t) i] = (float) (amp * std::sin (2.0 * 3.14159265358979323846 * f * i / fs + phase));
    return a;
}

/** Procesa 'x' en bloques de 'block'; 'sc' (opcional) va en los canales después de los principales. */
static Audio run (Proc& p, const Audio& x, const Audio* sc, int block)
{
    const int nMain = (int) x.size(), nSc = sc != nullptr ? (int) sc->size() : 0, n = (int) x[0].size();
    Audio y ((size_t) nMain, std::vector<float> ((size_t) n));
    juce::AudioBuffer<float> storage (nMain + nSc, block);
    juce::MidiBuffer midi;

    for (int start = 0; start < n; start += block)
    {
        const int len = std::min (block, n - start);
        juce::AudioBuffer<float> blk (storage.getArrayOfWritePointers(), nMain + nSc, len);
        for (int c = 0; c < nMain; ++c) std::copy (x[(size_t) c].begin() + start, x[(size_t) c].begin() + start + len, blk.getWritePointer (c));
        for (int c = 0; c < nSc; ++c)   std::copy ((*sc)[(size_t) c].begin() + start, (*sc)[(size_t) c].begin() + start + len, blk.getWritePointer (nMain + c));

        p.processBlock (blk, midi);

        for (int c = 0; c < nMain; ++c) std::copy (blk.getReadPointer (c), blk.getReadPointer (c) + len, y[(size_t) c].begin() + start);
    }
    return y;
}

static double rmsDb (const std::vector<float>& v, size_t from, size_t to)
{
    double e = 0; for (size_t i = from; i < to; ++i) e += (double) v[i] * v[i];
    return 10.0 * std::log10 (e / (double) (to - from) + 1e-30);
}

static double maxAbsDiff (const std::vector<float>& a, const std::vector<float>& b, size_t from, size_t to, int shiftB = 0)
{
    double m = 0;
    for (size_t i = from; i < to; ++i) m = std::max (m, (double) std::abs (a[i] - b[(size_t) ((long) i + shiftB)]));
    return m;
}

static bool approxEq (float a, float b) { return std::abs (a - b) < 1.0e-4f; }

static bool allFinite (const Audio& a) { for (auto& c : a) for (float v : c) if (! std::isfinite (v)) return false; return true; }

//------------------------------------------------------------------------------
static void testLayouts()
{
    std::printf ("[1] Layouts de buses\n");
    Proc p;
    auto layout = [] (int m, int sc, int out) { juce::AudioProcessor::BusesLayout l; l.inputBuses.add (setFor (m)); l.inputBuses.add (setFor (sc)); l.outputBuses.add (setFor (out)); return l; };
    CHECK (p.checkBusesLayoutSupported (layout (2, 0, 2)), "estéreo/estéreo");
    CHECK (p.checkBusesLayoutSupported (layout (1, 0, 1)), "mono/mono (antes el plugin no cargaba en pistas mono)");
    CHECK (p.checkBusesLayoutSupported (layout (2, 2, 2)), "estéreo + sidechain estéreo");
    CHECK (p.checkBusesLayoutSupported (layout (1, 1, 1)), "mono + sidechain mono");
    CHECK (p.checkBusesLayoutSupported (layout (2, 1, 2)), "estéreo + sidechain mono");
    CHECK (! p.checkBusesLayoutSupported (layout (2, 0, 1)), "estéreo->mono no debería soportarse");
    {
        juce::AudioProcessor::BusesLayout l; l.inputBuses.add (juce::AudioChannelSet::create5point1()); l.inputBuses.add (juce::AudioChannelSet::disabled()); l.outputBuses.add (juce::AudioChannelSet::create5point1());
        CHECK (! p.checkBusesLayoutSupported (l), "5.1 no debería soportarse");
    }
    std::printf ("    ok (mono, estéreo, con y sin sidechain)\n");
}

static void testLatencyAndDry()
{
    std::printf ("[2] Latencia reportada y Mix 0%% (dry)\n");
    const double fs = 48000.0; const int n = 48000;

    for (int os : { 0, 1, 2 })
    {
        auto p = makeProc (2, 0, fs, 512, { { ParamID::oversampling, (float) os }, { ParamID::mix, 0.0f }, { ParamID::color, 0.0f } });
        const int L = p->getLatencySamples();
        auto x = sineAudio (2, 440.0, -20.0, n, fs);
        auto y = run (*p, x, nullptr, 512);
        const double d = maxAbsDiff (y[0], x[0], (size_t) (L + 4000), (size_t) n, -L);
        std::printf ("    OS %s: latencia %d muestras | Mix 0%%: max|out - in retrasado| = %.2e\n", os == 0 ? "Off" : os == 1 ? "2x " : "4x ", L, d);
        CHECK (os == 0 ? L == 0 : L > 0, "latencia inesperada %d", L);
        CHECK (os == 0 ? d < 1e-9 : d < 1e-3, "Mix 0%% no es el dry retrasado (dif %.2e)", d);
    }
}

/** Lo que importa para compresión paralela: con la señal bajo el umbral y Color 0, el camino wet y el dry
    tienen que ser el MISMO a todas las frecuencias (misma ganancia y misma fase). Se compara Mix 100% vs Mix 0%. */
static void testWetDryAlignment()
{
    std::printf ("[2b] Alineación wet vs dry a distintas frecuencias (diferencia Mix 100%% vs Mix 0%%)\n");
    const double fs = 48000.0; const int n = 48000;
    for (int os : { 1, 2 })
    {
        std::printf ("    OS %dx:", os == 1 ? 2 : 4);
        double worst = -300.0;
        for (double f : { 200.0, 1000.0, 3000.0, 8000.0, 15000.0 })
        {
            Audio out[2];
            for (int m = 0; m < 2; ++m)
            {
                auto p = makeProc (2, 0, fs, 512, { { ParamID::oversampling, (float) os }, { ParamID::mix, m == 0 ? 0.0f : 100.0f }, { ParamID::color, 0.0f } });
                out[m] = run (*p, sineAudio (2, f, -40.0, n, fs), nullptr, 512);
            }
            double e = 0, s = 0;
            for (size_t i = 8000; i < (size_t) n; ++i) { const double r = out[1][0][i] - out[0][0][i]; e += r * r; s += (double) out[0][0][i] * out[0][0][i]; }
            const double db = 10.0 * std::log10 (e / s + 1e-30);
            std::printf ("  %5.0f Hz: %6.1f dB |", f, db);
            worst = std::max (worst, db);
        }
        std::printf ("\n");
        CHECK (worst < -70.0, "wet y dry no están alineados (peor caso %.1f dB)", worst);
    }
}

static void testAliasing()
{
    std::printf ("[3] Aliasing del saturador (9 kHz, Input +12 dB, 4:1, Color 100%%)\n");
    const double fs = 48000.0; const int N = 1 << 15;
    juce::dsp::FFT fft (15);
    double peakDb[3] = { 0, 0, 0 };

    for (int os = 0; os < 3; ++os)
    {
        auto p = makeProc (2, 0, fs, 512, { { ParamID::oversampling, (float) os }, { ParamID::color, 100.0f }, { ParamID::inputGain, 12.0f }, { ParamID::mix, 100.0f } });
        auto x = sineAudio (2, fs * 6144.0 / N, -12.0, 2 * N, fs);   // 9 kHz exactos (coherente con la FFT)
        auto y = run (*p, x, nullptr, 512);

        std::vector<float> buf (2 * (size_t) N, 0.0f);
        for (int i = 0; i < N; ++i) buf[(size_t) i] = y[0][(size_t) (N + i)] * (0.5f - 0.5f * std::cos (2.0f * 3.14159265f * (float) i / (float) N));
        fft.performFrequencyOnlyForwardTransform (buf.data());

        const int fb = 6144; double fund = 0;
        for (int k = fb - 3; k <= fb + 3; ++k) fund = std::max (fund, (double) buf[(size_t) k]);
        double worst = 0;
        for (int k = 4; k < N / 2; ++k)
        {
            bool harmonic = false;
            for (int h = 1; h * fb < N / 2; ++h) if (std::abs (k - h * fb) <= 3) harmonic = true;
            if (! harmonic) worst = std::max (worst, (double) buf[(size_t) k]);
        }
        peakDb[os] = 20.0 * std::log10 (worst / fund + 1e-30);
        std::printf ("    OS %s: pico de alias %.1f dBc\n", os == 0 ? "Off" : os == 1 ? "2x " : "4x ", peakDb[os]);
    }
    CHECK (peakDb[1] < peakDb[0] - 15.0, "2x no mejora al menos 15 dB (%.1f vs %.1f)", peakDb[1], peakDb[0]);
    CHECK (peakDb[2] < peakDb[0] - 25.0, "4x no mejora al menos 25 dB (%.1f vs %.1f)", peakDb[2], peakDb[0]);
}

static void testRobustness()
{
    std::printf ("[4] NaN / Inf en la entrada y bloques raros\n");
    const double fs = 48000.0;
    auto p = makeProc (2, 0, fs, 512, { { ParamID::ratio, 1.0f }, { ParamID::inputGain, 10.0f } });
    auto x = sineAudio (2, 1000.0, -6.0, 48000, fs);
    x[0][1000] = std::numeric_limits<float>::quiet_NaN();
    x[1][1200] = std::numeric_limits<float>::infinity();
    x[0][5000] = -std::numeric_limits<float>::infinity();
    auto y = run (*p, x, nullptr, 512);
    CHECK (allFinite (y), "la salida tiene valores no finitos");
    const float gr = p->readAndResetGainReductionDb();
    std::printf ("    salida finita: %s | GR tras los NaN: %.1f dB (el compresor sigue vivo)\n", allFinite (y) ? "sí" : "NO", gr);
    CHECK (gr > 5.0f, "el compresor murió tras los NaN (GR %.2f)", gr);

    // bloque más grande que el preparado + bloques de 1 muestra
    auto p2 = makeProc (2, 0, fs, 256, {});
    auto big = sineAudio (2, 500.0, -10.0, 1024, fs);
    auto yb = run (*p2, big, nullptr, 1024);
    CHECK (allFinite (yb), "bloque de 1024 con prepare(256)");
    auto p3 = makeProc (2, 0, fs, 512, {});
    auto tiny = sineAudio (2, 500.0, -10.0, 2000, fs);
    auto yt = run (*p3, tiny, nullptr, 1);
    CHECK (allFinite (yt), "bloques de 1 muestra");

    // el resultado no depende del tamaño de bloque
    auto pa = makeProc (2, 0, fs, 512, { { ParamID::inputGain, 8.0f } });
    auto pb = makeProc (2, 0, fs, 512, { { ParamID::inputGain, 8.0f } });
    auto sig = sineAudio (2, 700.0, -9.0, 24000, fs);
    auto ya = run (*pa, sig, nullptr, 64), yb2 = run (*pb, sig, nullptr, 480);
    const double d = maxAbsDiff (ya[0], yb2[0], 0, 24000);
    std::printf ("    max|salida(bloques de 64) - salida(bloques de 480)| = %.2e\n", d);
    CHECK (d < 1e-4, "depende del tamaño de bloque (%.2e)", d);
}

static void testSidechain()
{
    std::printf ("[5] Sidechain externo\n");
    const double fs = 48000.0; const int n = 48000;
    auto quiet = sineAudio (2, 200.0, -30.0, n, fs);
    auto loud  = sineAudio (2, 3000.0, -2.0, n, fs);
    Audio silent (2, std::vector<float> ((size_t) n, 0.0f));

    auto level = [&] (const Audio& sc, bool ext, int mainCh)
    {
        auto p = makeProc (mainCh, 2, fs, 512, { { ParamID::scExternal, ext ? 1.0f : 0.0f }, { ParamID::inputGain, 6.0f }, { ParamID::ratio, 1.0f }, { ParamID::color, 0.0f }, { ParamID::oversampling, 0.0f } });
        Audio x (quiet.begin(), quiet.begin() + mainCh);
        auto y = run (*p, x, &sc, 512);
        return rmsDb (y[0], (size_t) n / 2, (size_t) n);
    };
    const double lSilent = level (silent, true, 2), lLoud = level (loud, true, 2), lIntLoud = level (loud, false, 2), lMono = level (loud, true, 1);
    std::printf ("    principal -30 dBFS, SC externo: SC silencio %.1f dB | SC fuerte %.1f dB | (SC fuerte pero 'SC INT') %.1f dB | mono+SC %.1f dB\n", lSilent, lLoud, lIntLoud, lMono);
    CHECK (lSilent - lLoud > 6.0, "el SC fuerte no comprime (%.1f vs %.1f)", lSilent, lLoud);
    CHECK (std::abs (lIntLoud - lSilent) < 0.3, "con SC INT el bus de sidechain debería ignorarse");
    CHECK (lSilent - lMono > 6.0, "mono + sidechain estéreo no comprime");
}

static void testLinkAndMs()
{
    std::printf ("[6] Link estéreo y M/S\n");
    const double fs = 48000.0; const int n = 48000;
    Audio x (2); x[0] = sineAudio (1, 1000.0, -4.0, n, fs)[0]; x[1] = sineAudio (1, 1000.0, -34.0, n, fs)[0];

    for (float link : { 100.0f, 0.0f })
    {
        auto p = makeProc (2, 0, fs, 512, { { ParamID::link, link }, { ParamID::inputGain, 0.0f }, { ParamID::color, 0.0f }, { ParamID::oversampling, 0.0f } });
        auto y = run (*p, x, nullptr, 512);
        const double dL = rmsDb (y[0], 24000, (size_t) n) - rmsDb (x[0], 24000, (size_t) n);
        const double dR = rmsDb (y[1], 24000, (size_t) n) - rmsDb (x[1], 24000, (size_t) n);
        std::printf ("    link %3.0f%%: L %+.2f dB | R %+.2f dB\n", link, dL, dR);
        if (link > 50.0f) CHECK (std::abs (dL - dR) < 0.2 && dL < -5.0, "con link 100%% L y R deberían bajar igual");
        else                CHECK (std::abs (dR) < 0.1 && dL < -5.0, "con link 0%% R no debería tocarse");
    }
    {
        // M/S bajo el umbral: debe ser transparente (la codificación/decodificación es ortonormal)
        auto p = makeProc (2, 0, fs, 512, { { ParamID::stereoMode, 1.0f }, { ParamID::color, 0.0f }, { ParamID::oversampling, 0.0f } });
        Audio q (2); q[0] = sineAudio (1, 800.0, -40.0, n, fs)[0]; q[1] = sineAudio (1, 1300.0, -44.0, n, fs)[0];
        auto y = run (*p, q, nullptr, 512);
        const double d = std::max (maxAbsDiff (y[0], q[0], 100, (size_t) n), maxAbsDiff (y[1], q[1], 100, (size_t) n));
        std::printf ("    M/S bajo el umbral: max|out - in| = %.2e\n", d);
        CHECK (d < 1e-6, "M/S no es transparente (%.2e)", d);
    }
}

static void testAutoGain()
{
    std::printf ("[7] Auto gain\n");
    const double fs = 48000.0; const int n = (int) (8 * fs);
    for (bool on : { false, true })
    {
        auto p = makeProc (2, 0, fs, 512, { { ParamID::ratio, 1.0f }, { ParamID::inputGain, 10.0f }, { ParamID::color, 0.0f }, { ParamID::autoGain, on ? 1.0f : 0.0f }, { ParamID::oversampling, 0.0f } });
        auto x = sineAudio (2, 1000.0, -14.0, n, fs);
        auto y = run (*p, x, nullptr, 512);
        const double diff = rmsDb (y[0], (size_t) (6 * fs), (size_t) n) - rmsDb (x[0], (size_t) (6 * fs), (size_t) n);
        std::printf ("    Auto gain %s: nivel de salida - entrada = %+.2f dB (Input +10 dB, 8:1)\n", on ? "ON " : "OFF", diff);
        if (on) CHECK (std::abs (diff) < 0.6, "no iguala la sonoridad (%+.2f dB)", diff);
        else    CHECK (std::abs (diff) > 2.0, "caso de referencia sin auto gain debería diferir");
    }
}

static void testSmoothing()
{
    std::printf ("[8] Smoothing de Input y Mix (sin zipper)\n");
    const double fs = 48000.0; const int n = 24000;
    auto step = [&] (const char* id, float before, float after, ParamList extra)
    {
        auto p = makeProc (2, 0, fs, 512, extra);
        setParam (*p, id, before);
        p->prepareToPlay (fs, 512);
        auto x = sineAudio (2, 1000.0, -40.0, n, fs);
        Audio a (2), b (2);
        for (int c = 0; c < 2; ++c) { a[(size_t) c].assign (x[(size_t) c].begin(), x[(size_t) c].begin() + 12000); b[(size_t) c].assign (x[(size_t) c].begin() + 12000, x[(size_t) c].end()); }
        auto ya = run (*p, a, nullptr, 512);
        setParam (*p, id, after);
        auto yb = run (*p, b, nullptr, 512);
        double mx = 0; std::vector<float> all = ya[0]; all.insert (all.end(), yb[0].begin(), yb[0].end());
        for (size_t i = 13000; i < all.size(); ++i) mx = std::max (mx, (double) std::abs (all[i] - all[i - 1]));
        return mx;
    };
    // Input +0 -> +12 dB con señal bajo el umbral (sin GR): el salto natural máximo entre muestras ~0.0052
    const double sIn  = step (ParamID::inputGain, 0.0f, 12.0f, { { ParamID::color, 0.0f }, { ParamID::oversampling, 0.0f } });
    std::printf ("    Input 0 -> +12 dB de golpe: máximo salto entre muestras = %.4f (sin smoothing sería ~0.03)\n", sIn);
    CHECK (sIn < 0.0075, "Input hace zipper (%.4f)", sIn);
    // Mix 0 -> 100 con una señal wet distinta del dry (Output +12 dB)
    const double sMix = step (ParamID::mix, 0.0f, 100.0f, { { ParamID::outputGain, 12.0f }, { ParamID::color, 0.0f }, { ParamID::oversampling, 0.0f } });
    std::printf ("    Mix 0 -> 100 con Output +12: máximo salto = %.4f\n", sMix);
    CHECK (sMix < 0.0075, "Mix hace zipper (%.4f)", sMix);
}

static void testStateAndPresets()
{
    std::printf ("[9] Estado y presets\n");
    {
        auto a = makeProc (2, 0, 48000.0, 512, { { ParamID::inputGain, 7.5f }, { ParamID::ratio, 3.0f }, { ParamID::scHpf, 120.0f }, { ParamID::link, 40.0f },
                                                  { ParamID::stereoMode, 1.0f }, { ParamID::color, 60.0f }, { ParamID::oversampling, 2.0f }, { ParamID::autoGain, 1.0f } });
        juce::MemoryBlock mb; a->getStateInformation (mb);
        Proc b; b.setStateInformation (mb.getData(), (int) mb.getSize());
        auto get = [] (Proc& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); };
        const bool same = approxEq (get (b, ParamID::inputGain), 7.5f) && approxEq (get (b, ParamID::ratio), 3.0f) && approxEq (get (b, ParamID::scHpf), 120.0f) && approxEq (get (b, ParamID::link), 40.0f)
                       && approxEq (get (b, ParamID::stereoMode), 1.0f) && approxEq (get (b, ParamID::color), 60.0f) && approxEq (get (b, ParamID::oversampling), 2.0f) && approxEq (get (b, ParamID::autoGain), 1.0f);
        std::printf ("    ida y vuelta del estado: %s\n", same ? "ok" : "NO coincide");
        CHECK (same, "el estado no hace ida y vuelta");
    }
    {
        // Estado de la build original (6 parámetros, sin 'stateVersion'): tiene que cargar y dejar los nuevos en su default.
        juce::ValueTree old ("PARAMETERS");
        auto add = [&old] (const char* id, float v) { juce::ValueTree t ("PARAM"); t.setProperty ("id", id, nullptr); t.setProperty ("value", v, nullptr); old.appendChild (t, nullptr); };
        add ("inputGain", 9.0f); add ("outputGain", -3.0f); add ("attack", 0.1f); add ("release", 150.0f); add ("ratio", 2.0f); add ("mix", 55.0f);
        auto xml = old.createXml(); juce::MemoryBlock mb; juce::AudioProcessor::copyXmlToBinary (*xml, mb);
        Proc p; p.setStateInformation (mb.getData(), (int) mb.getSize());
        auto get = [&p] (const char* id) { return p.apvts.getRawParameterValue (id)->load(); };
        std::printf ("    estado viejo: input %.1f, ratio %.0f, mix %.0f | defaults nuevos: color %.0f, OS %.0f, link %.0f\n", get (ParamID::inputGain), get (ParamID::ratio), get (ParamID::mix), get (ParamID::color), get (ParamID::oversampling), get (ParamID::link));
        CHECK (approxEq (get (ParamID::inputGain), 9.0f) && approxEq (get (ParamID::ratio), 2.0f) && approxEq (get (ParamID::mix), 55.0f), "no cargó el estado viejo");
        CHECK (approxEq (get (ParamID::color), 25.0f) && approxEq (get (ParamID::oversampling), 1.0f) && approxEq (get (ParamID::link), 100.0f), "defaults nuevos incorrectos");
    }
    {
        Proc p;
        CHECK (p.getNumPrograms() >= 5, "faltan presets");
        bool ok = true;
        for (int i = 0; i < p.getNumPrograms(); ++i) { p.setCurrentProgram (i); ok = ok && p.getProgramName (i).isNotEmpty() && p.getCurrentProgram() == i; }
        p.setCurrentProgram (1);
        const bool drum = p.getProgramName (1) == "Drum Bus" && approxEq (p.apvts.getRawParameterValue (ParamID::inputGain)->load(), 8.0f) && approxEq (p.apvts.getRawParameterValue (ParamID::ratio)->load(), 0.0f);
        std::printf ("    %d presets; 'Drum Bus' aplica sus parámetros: %s\n", p.getNumPrograms(), drum ? "sí" : "NO");
        CHECK (ok && drum, "presets rotos");
    }
}

static void testCpu()
{
    std::printf ("[10] Costo de CPU (estéreo 48 kHz, bloques de 512)\n");
    const double fs = 48000.0; const int n = (int) (10 * fs);
    for (int os : { 0, 1, 2 })
    {
        auto p = makeProc (2, 0, fs, 512, { { ParamID::oversampling, (float) os }, { ParamID::color, 40.0f }, { ParamID::inputGain, 8.0f } });
        auto x = sineAudio (2, 440.0, -12.0, n, fs);
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        auto y = run (*p, x, nullptr, 512);
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        const double cpu = 100.0 * ms / 10000.0;
        std::printf ("    OS %s: %.1f%% de un núcleo (%.0fx tiempo real)\n", os == 0 ? "Off" : os == 1 ? "2x " : "4x ", cpu, 100.0 / cpu);
        CHECK (cpu < 25.0, "demasiado CPU (%.1f%%)", cpu);
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    testLayouts(); testLatencyAndDry(); testWetDryAlignment(); testAliasing(); testRobustness(); testSidechain();
    testLinkAndMs(); testAutoGain(); testSmoothing(); testStateAndPresets(); testCpu();

    std::printf ("\n%d checks, %d fallos\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
