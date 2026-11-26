// Copyright (c) 2026 FSMAudio. Todos los derechos reservados. Software propietario: ver LICENSE y EULA.txt.
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr float kInvSqrt2 = 0.70710678f;

    using PID = juce::ParameterID;

    juce::String hpfText (float v, int)
    {
        return v <= 20.5f ? juce::String ("Off") : juce::String ((int) std::round (v)) + " Hz";
    }

    float hpfFromText (const juce::String& t)
    {
        return t.containsIgnoreCase ("off") ? 20.0f : t.getFloatValue();
    }

    // Presets de fábrica: PUNTOS DE PARTIDA (ver README), no son el sonido de ningún disco.
    struct Preset
    {
        const char* name;
        float input, output, attackMs, releaseMs;
        int ratio;                    // 0..4 = 4:1, 8:1, 12:1, 20:1, All Buttons In
        float mix, hpfHz, link;       // mix en %, link en %
        int stereoMode;               // 0 = L/R, 1 = M/S
        float color;                  // %
        int oversampling;             // 0 = Off, 1 = 2x, 2 = 4x
        bool autoGain;
    };

    const Preset kPresets[] =
    {
        //  nombre                  in    out   att    rel   rat  mix   hpf   link  st  col  os  auto
        { "Init",                   0.0f, 0.0f, 0.20f, 300.f, 0, 100.f,  20.f, 100.f, 0, 25.f, 1, false },
        { "Drum Bus",               8.0f, 0.0f, 0.08f, 220.f, 0,  70.f,  60.f, 100.f, 0, 25.f, 1, true  },
        { "Vocal",                  6.0f, 0.0f, 0.20f, 200.f, 0, 100.f,  80.f, 100.f, 0, 20.f, 1, true  },
        { "Guitars Parallel",      10.0f, 0.0f, 0.10f, 120.f, 4,  30.f, 100.f, 100.f, 0, 35.f, 1, true  },
        { "Mix Bus Glue",           2.0f, 0.0f, 0.80f, 300.f, 0, 100.f,  90.f, 100.f, 0, 15.f, 1, true  },
        { "Fast & Nasty (20:1)",   12.0f, 0.0f, 0.02f,  80.f, 3,  50.f,  80.f, 100.f, 0, 40.f, 1, true  },
    };
    constexpr int kNumPresets = (int) (sizeof (kPresets) / sizeof (kPresets[0]));
}

//==============================================================================
struct ElFratachoAudioProcessor::Params
{
    float inputDb, outputDb, attackMs, releaseMs, mix01, hpfHz, link01, color01;
    int ratio, stereoMode, os;
    bool autoGain, scExternal;
};

//==============================================================================
ElFratachoAudioProcessor::ElFratachoAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    pInput        = apvts.getRawParameterValue (ParamID::inputGain);
    pOutput       = apvts.getRawParameterValue (ParamID::outputGain);
    pAttack       = apvts.getRawParameterValue (ParamID::attack);
    pRelease      = apvts.getRawParameterValue (ParamID::release);
    pRatio        = apvts.getRawParameterValue (ParamID::ratio);
    pMix          = apvts.getRawParameterValue (ParamID::mix);
    pHpf          = apvts.getRawParameterValue (ParamID::scHpf);
    pLink         = apvts.getRawParameterValue (ParamID::link);
    pStereoMode   = apvts.getRawParameterValue (ParamID::stereoMode);
    pColor        = apvts.getRawParameterValue (ParamID::color);
    pOversampling = apvts.getRawParameterValue (ParamID::oversampling);
    pAutoGain     = apvts.getRawParameterValue (ParamID::autoGain);
    pScExternal   = apvts.getRawParameterValue (ParamID::scExternal);
}

ElFratachoAudioProcessor::~ElFratachoAudioProcessor()
{
    cancelPendingUpdate();
}

juce::AudioProcessorValueTreeState::ParameterLayout ElFratachoAudioProcessor::createParameterLayout()
{
    using Float  = juce::AudioParameterFloat;
    using Attr   = juce::AudioParameterFloatAttributes;
    using Choice = juce::AudioParameterChoice;
    using Bool   = juce::AudioParameterBool;
    using Range  = juce::NormalisableRange<float>;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<Float> (PID { ParamID::inputGain, 1 }, "Input",
                                               Range (-20.0f, 30.0f, 0.1f), 0.0f, Attr().withLabel ("dB")));
    params.push_back (std::make_unique<Float> (PID { ParamID::outputGain, 1 }, "Output",
                                               Range (-30.0f, 20.0f, 0.1f), 0.0f, Attr().withLabel ("dB")));
    params.push_back (std::make_unique<Float> (PID { ParamID::attack, 1 }, "Attack",
                                               Range (0.02f, 0.8f, 0.001f, 0.4f), 0.2f, Attr().withLabel ("ms")));
    params.push_back (std::make_unique<Float> (PID { ParamID::release, 1 }, "Release",
                                               Range (50.0f, 1100.0f, 1.0f, 0.4f), 300.0f, Attr().withLabel ("ms")));
    params.push_back (std::make_unique<Choice> (PID { ParamID::ratio, 1 }, "Ratio",
                                                juce::StringArray { "4:1", "8:1", "12:1", "20:1", "All Buttons In" }, 0));
    params.push_back (std::make_unique<Float> (PID { ParamID::mix, 1 }, "Mix",
                                               Range (0.0f, 100.0f, 1.0f), 100.0f, Attr().withLabel ("%")));

    // --- nuevos (versión 1 del esquema de parámetros; los estados viejos siguen cargando) ---
    params.push_back (std::make_unique<Float> (PID { ParamID::scHpf, 1 }, "SC HPF",
                                               Range (20.0f, 500.0f, 1.0f, 0.4f), 20.0f,
                                               Attr().withLabel ("Hz").withStringFromValueFunction (hpfText)
                                                                      .withValueFromStringFunction (hpfFromText)));
    params.push_back (std::make_unique<Float> (PID { ParamID::link, 1 }, "Stereo Link",
                                               Range (0.0f, 100.0f, 1.0f), 100.0f, Attr().withLabel ("%")));
    params.push_back (std::make_unique<Choice> (PID { ParamID::stereoMode, 1 }, "Stereo Mode",
                                                juce::StringArray { "L/R", "M/S" }, 0));
    params.push_back (std::make_unique<Float> (PID { ParamID::color, 1 }, "Color",
                                               Range (0.0f, 100.0f, 1.0f), 25.0f, Attr().withLabel ("%")));
    params.push_back (std::make_unique<Choice> (PID { ParamID::oversampling, 1 }, "Oversampling",
                                                juce::StringArray { "Off", "2x", "4x" }, 1));
    params.push_back (std::make_unique<Bool> (PID { ParamID::autoGain, 1 }, "Auto Gain", false));
    params.push_back (std::make_unique<Bool> (PID { ParamID::scExternal, 1 }, "Sidechain External", false));

    return { params.begin(), params.end() };
}

//==============================================================================
bool ElFratachoAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn  = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != mainOut) return false;
    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo()) return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }

    return layouts.outputBuses.size() == 1;
}

int ElFratachoAudioProcessor::getLatencyForOversamplingChoice (int choice) const
{
    if (choice <= 0 || choice > 2) return 0;
    const auto& os = oversamplers[(size_t) (choice - 1)];
    return os != nullptr ? (int) std::ceil (os->getLatencyInSamples()) : 0;
}

void ElFratachoAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    baseSampleRate      = sampleRate;
    maxBlock            = juce::jmax (1, samplesPerBlock);
    numChannelsPrepared = juce::jlimit (1, 2, getMainBusNumInputChannels());

    work.setSize (2, maxBlock, false, true, false);
    dry.setSize  (2, maxBlock, false, true, false);

    for (size_t k = 0; k < oversamplers.size(); ++k)
        for (auto* list : { &oversamplers, &dryOversamplers })
        {
            (*list)[k] = std::make_unique<juce::dsp::Oversampling<float>> (
                (size_t) numChannelsPrepared, k + 1,
                juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
            (*list)[k]->initProcessing ((size_t) maxBlock);
        }

    comp.prepare (sampleRate, numChannelsPrepared);
    matcher.prepare (sampleRate);

    // Smoothing: arrancan en el valor actual (sin rampa inicial) y siguen los cambios.
    inputGainSm.reset  (sampleRate, 0.03);   inputGainSm.setCurrentAndTargetValue  (efrt::dbToLin (pInput->load()));
    outputGainSm.reset (sampleRate, 0.03);   outputGainSm.setCurrentAndTargetValue (efrt::dbToLin (pOutput->load()));
    autoGainSm.reset   (sampleRate, 0.15);   autoGainSm.setCurrentAndTargetValue   (1.0f);
    mixSm.reset        (sampleRate, 0.03);   mixSm.setCurrentAndTargetValue        (pMix->load() * 0.01f);
    colorSm.reset      (sampleRate, 0.05);   colorSm.setCurrentAndTargetValue      (pColor->load() * 0.01f);

    applyOversampling (juce::jlimit (0, 2, (int) std::lround (pOversampling->load())));
    setLatencySamples (latencySamples);

    prepared = true;
    reset();
}

void ElFratachoAudioProcessor::reset()
{
    comp.reset();
    for (auto& s : sat) s.reset();
    for (auto& o : oversamplers)    if (o != nullptr) o->reset();
    for (auto& o : dryOversamplers) if (o != nullptr) o->reset();
    matcher.reset();
    autoGainSm.setCurrentAndTargetValue (1.0f);
}

void ElFratachoAudioProcessor::applyOversampling (int choice)
{
    osChoice = choice;
    const double satRate = baseSampleRate * (choice == 0 ? 1.0 : (choice == 1 ? 2.0 : 4.0));

    for (auto& s : sat) s.prepare (satRate);
    for (auto& o : oversamplers)    if (o != nullptr) o->reset();
    for (auto& o : dryOversamplers) if (o != nullptr) o->reset();

    latencySamples = getLatencyForOversamplingChoice (choice);

    pendingLatency.store (latencySamples);
    triggerAsyncUpdate();                             // setLatencySamples fuera del audio thread
}

void ElFratachoAudioProcessor::handleAsyncUpdate()
{
    setLatencySamples (pendingLatency.load());
}

//==============================================================================
void ElFratachoAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto main = getBusBuffer (buffer, true, 0);
    const int nCh   = juce::jmin (main.getNumChannels(), 2);
    const int total = main.getNumSamples();

    if (! prepared || nCh == 0 || total == 0 || nCh != numChannelsPrepared)
        return;

    // Sidechain externo (si el host lo conecta y el usuario lo activó).
    const float* scPtr[2] = { nullptr, nullptr };
    int scCh = 0;
    const bool wantExt = pScExternal->load() > 0.5f;
    if (wantExt && getBusCount (true) > 1)
    {
        if (auto* bus = getBus (true, 1); bus != nullptr && bus->isEnabled())
        {
            auto scBuf = getBusBuffer (buffer, true, 1);
            scCh = juce::jmin (2, scBuf.getNumChannels());
            for (int c = 0; c < scCh; ++c) scPtr[c] = scBuf.getReadPointer (c);
        }
    }

    const int osNow = juce::jlimit (0, 2, (int) std::lround (pOversampling->load()));
    if (osNow != osChoice)
        applyOversampling (osNow);

    Params p;
    p.inputDb    = pInput->load();
    p.outputDb   = pOutput->load();
    p.attackMs   = pAttack->load();
    p.releaseMs  = pRelease->load();
    p.mix01      = pMix->load() * 0.01f;
    p.hpfHz      = pHpf->load();
    p.link01     = pLink->load() * 0.01f;
    p.color01    = pColor->load() * 0.01f;
    p.ratio      = juce::jlimit (0, 4, (int) std::lround (pRatio->load()));
    p.stereoMode = (int) std::lround (pStereoMode->load());
    p.os         = osNow;
    p.autoGain   = pAutoGain->load() > 0.5f;
    p.scExternal = scCh > 0;

    // Por si el host manda bloques más grandes que los anunciados.
    for (int start = 0; start < total; start += maxBlock)
        processChunk (main, start, juce::jmin (maxBlock, total - start), nCh, scPtr, scCh, p);
}

void ElFratachoAudioProcessor::processChunk (juce::AudioBuffer<float>& main, int start, int n, int nCh,
                                             const float* const* sc, int scCh, const Params& p)
{
    const bool ms        = p.stereoMode == 1 && nCh == 2;
    const bool useExt    = p.scExternal && scCh > 0;
    const bool stereo    = nCh == 2;

    float* io[2] = { main.getWritePointer (0) + start, stereo ? main.getWritePointer (1) + start : nullptr };
    float* w[2]  = { work.getWritePointer (0), work.getWritePointer (1) };
    float* dr[2] = { dry.getWritePointer (0),  dry.getWritePointer (1) };

    efrt::Fet76Compressor::Settings cs;
    cs.attackMs  = p.attackMs;
    cs.releaseMs = p.releaseMs;
    cs.ratio     = (efrt::RatioMode) p.ratio;
    cs.hpfHz     = p.hpfHz;
    cs.link      = p.link01;
    comp.setSettings (cs);

    inputGainSm.setTargetValue  (efrt::dbToLin (p.inputDb));
    outputGainSm.setTargetValue (efrt::dbToLin (p.outputDb));
    mixSm.setTargetValue        (p.mix01);
    colorSm.setTargetValue      (p.color01);
    autoGainSm.setTargetValue   (p.autoGain ? matcher.getGain() : 1.0f);

    float inPeak = 0.0f, grMax = 0.0f;

    //--------------------------------------------------------------------------
    // 1) Input gain + detección + ganancia del compresor (a la tasa base)
    for (int i = 0; i < n; ++i)
    {
        float in[2] = { efrt::sanitize (io[0][i]), stereo ? efrt::sanitize (io[1][i]) : 0.0f };
        inPeak = juce::jmax (inPeak, std::abs (in[0]), std::abs (in[1]));

        // camino dry (se alinea con el wet más abajo, pasando por un oversampler idéntico)
        dr[0][i] = in[0];
        dr[1][i] = in[1];

        if (ms)
        {
            const float m = (in[0] + in[1]) * kInvSqrt2, s = (in[0] - in[1]) * kInvSqrt2;
            in[0] = m; in[1] = s;
        }

        const float ig = inputGainSm.getNextValue();
        const float x[2] = { in[0] * ig, in[1] * ig };

        float det[2] = { x[0], x[1] };
        if (useExt)
        {
            float s0 = efrt::sanitize (sc[0][start + i]);
            float s1 = scCh > 1 ? efrt::sanitize (sc[1][start + i]) : s0;
            if (! stereo)       { s0 = scCh > 1 ? 0.5f * (s0 + s1) : s0; }
            else if (ms)        { const float m = (s0 + s1) * kInvSqrt2, s = (s0 - s1) * kInvSqrt2; s0 = m; s1 = s; }
            det[0] = s0 * ig;
            det[1] = s1 * ig;
        }

        float g[2] = { 1.0f, 1.0f };
        comp.processSample (det, g);

        w[0][i] = x[0] * g[0];
        w[1][i] = x[1] * g[1];
        grMax = juce::jmax (grMax, comp.getMaxGainReductionDb());
    }

    //--------------------------------------------------------------------------
    // 2) Saturación FET/transformador, sobremuestreada
    const float color = colorSm.skip (n);
    const float extra = comp.getCurve().extraDrive;
    for (int c = 0; c < nCh; ++c)
        sat[(size_t) c].setColor (color, extra);

    juce::dsp::AudioBlock<float> block (work.getArrayOfWritePointers(), (size_t) nCh, (size_t) n);

    if (osChoice > 0)
    {
        auto& os = *oversamplers[(size_t) (osChoice - 1)];
        auto up = os.processSamplesUp (block);
        for (int c = 0; c < nCh; ++c)
        {
            float* d = up.getChannelPointer ((size_t) c);
            for (size_t j = 0; j < up.getNumSamples(); ++j)
                d[j] = sat[(size_t) c].process (d[j]);
        }
        os.processSamplesDown (block);
    }
    else
    {
        for (int c = 0; c < nCh; ++c)
            for (int i = 0; i < n; ++i)
                w[c][i] = sat[(size_t) c].process (w[c][i]);
    }

    // El dry atraviesa un oversampler idéntico (sin procesar): misma latencia y misma fase que el wet.
    if (osChoice > 0)
    {
        juce::dsp::AudioBlock<float> dryBlock (dry.getArrayOfWritePointers(), (size_t) nCh, (size_t) n);
        auto& dos = *dryOversamplers[(size_t) (osChoice - 1)];
        dos.processSamplesUp (dryBlock);
        dos.processSamplesDown (dryBlock);
    }

    //--------------------------------------------------------------------------
    // 3) Auto gain, output, decodificación M/S y mezcla dry/wet
    double sumDry = 0.0, sumWet = 0.0;
    float outPeak = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float gain = outputGainSm.getNextValue() * autoGainSm.getNextValue();
        const float mx   = mixSm.getNextValue();

        float a = w[0][i], b = stereo ? w[1][i] : 0.0f;
        const float d0 = dr[0][i], d1 = stereo ? dr[1][i] : 0.0f;

        sumWet += (double) a * a + (double) b * b;
        sumDry += (double) d0 * d0 + (double) d1 * d1;

        a *= gain; b *= gain;
        if (ms) { const float l = (a + b) * kInvSqrt2, r = (a - b) * kInvSqrt2; a = l; b = r; }

        const float o0 = efrt::sanitize (d0 + mx * (a - d0));
        io[0][i] = o0;
        outPeak = juce::jmax (outPeak, std::abs (o0));

        if (stereo)
        {
            const float o1 = efrt::sanitize (d1 + mx * (b - d1));
            io[1][i] = o1;
            outPeak = juce::jmax (outPeak, std::abs (o1));
        }
    }

    matcher.pushBlock (sumDry, sumWet, n, nCh);

    if (inPeak  > meterIn.load())  meterIn.store (inPeak);
    if (outPeak > meterOut.load()) meterOut.store (outPeak);
    if (grMax   > meterGr.load())  meterGr.store (grMax);
}

//==============================================================================
int ElFratachoAudioProcessor::getNumPrograms() { return kNumPresets; }

const juce::String ElFratachoAudioProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, kNumPresets) ? juce::String (kPresets[index].name) : juce::String();
}

void ElFratachoAudioProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, kNumPresets))
        return;

    currentProgram = index;
    const auto& pr = kPresets[index];

    auto set = [this] (const char* id, float value)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };

    set (ParamID::inputGain,    pr.input);
    set (ParamID::outputGain,   pr.output);
    set (ParamID::attack,       pr.attackMs);
    set (ParamID::release,      pr.releaseMs);
    set (ParamID::ratio,        (float) pr.ratio);
    set (ParamID::mix,          pr.mix);
    set (ParamID::scHpf,        pr.hpfHz);
    set (ParamID::link,         pr.link);
    set (ParamID::stereoMode,   (float) pr.stereoMode);
    set (ParamID::color,        pr.color);
    set (ParamID::oversampling, (float) pr.oversampling);
    set (ParamID::autoGain,     pr.autoGain ? 1.0f : 0.0f);
}

//==============================================================================
void ElFratachoAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", 2, nullptr);      // 1 = build original (6 parámetros)
    state.setProperty ("program", currentProgram, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ElFratachoAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        currentProgram = juce::jlimit (0, kNumPresets - 1, (int) tree.getProperty ("program", 0));
        apvts.replaceState (tree);
    }
}

juce::AudioProcessorEditor* ElFratachoAudioProcessor::createEditor()
{
    return new ElFratachoAudioProcessorEditor (*this);
}

// Esta función la requiere JUCE para crear el plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ElFratachoAudioProcessor();
}
