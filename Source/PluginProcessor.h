// Copyright (c) 2026 FSMAudio. Todos los derechos reservados. Software propietario: ver LICENSE y EULA.txt.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <memory>
#include "Fet76Compressor.h"

//==============================================================================
// IDs de parámetros (versión 1 = formato de estado compatible con las builds viejas).
namespace ParamID
{
    inline constexpr const char* inputGain    = "inputGain";
    inline constexpr const char* outputGain   = "outputGain";
    inline constexpr const char* attack       = "attack";
    inline constexpr const char* release      = "release";
    inline constexpr const char* ratio        = "ratio";
    inline constexpr const char* mix          = "mix";
    // nuevos
    inline constexpr const char* scHpf        = "scHpf";
    inline constexpr const char* link         = "link";
    inline constexpr const char* stereoMode   = "stereoMode";
    inline constexpr const char* color        = "color";
    inline constexpr const char* oversampling = "oversampling";
    inline constexpr const char* autoGain     = "autoGain";
    inline constexpr const char* scExternal   = "scExternal";
}

//==============================================================================
class ElFratachoAudioProcessor : public juce::AudioProcessor,
                                 private juce::AsyncUpdater
{
public:
    ElFratachoAudioProcessor();
    ~ElFratachoAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    using juce::AudioProcessor::processBlock;       // no ocultar el overload de double
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EFRT"; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Presets de fábrica (puntos de partida, no "el sonido de" nadie).
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    juce::AudioProcessorValueTreeState apvts;

    // Medidores: el audio thread acumula el máximo; la UI lo lee y lo resetea.
    float readAndResetGainReductionDb() noexcept { return meterGr.exchange (0.0f); }
    float readAndResetInputPeak()       noexcept { return meterIn.exchange (0.0f); }
    float readAndResetOutputPeak()      noexcept { return meterOut.exchange (0.0f); }

    int getLatencyForOversamplingChoice (int choice) const;

private:
    struct Params;   // snapshot de parámetros de un bloque

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void handleAsyncUpdate() override;
    void applyOversampling (int choice);
    void processChunk (juce::AudioBuffer<float>& main, int start, int n, int nCh,
                       const float* const* sc, int scCh, const Params& p);

    // --- DSP ---
    efrt::Fet76Compressor comp;
    std::array<efrt::FetSaturator, 2> sat;
    efrt::LoudnessMatcher matcher;

    // 2x, 4x. 'dryOversamplers' son copias idénticas por las que pasa el dry SIN procesar: así wet y dry
    // comparten exactamente la misma respuesta de fase (el truco de "latencia entera" de JUCE usa un allpass
    // que solo es exacto en graves; un delay entero para el dry no alcanzaría y el Mix haría peine en agudos).
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 2> oversamplers, dryOversamplers;
    int osChoice = 0;                       // 0 = Off, 1 = 2x, 2 = 4x
    std::atomic<int> pendingLatency { 0 };
    int latencySamples = 0;

    juce::AudioBuffer<float> work, dry;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> inputGainSm, outputGainSm, autoGainSm;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSm, colorSm;

    double baseSampleRate = 44100.0;
    int maxBlock = 512;
    int numChannelsPrepared = 2;
    bool prepared = false;

    // --- parámetros (punteros cacheados: nada de búsquedas por string en el audio thread) ---
    std::atomic<float> *pInput = nullptr, *pOutput = nullptr, *pAttack = nullptr, *pRelease = nullptr,
                       *pRatio = nullptr, *pMix = nullptr, *pHpf = nullptr, *pLink = nullptr,
                       *pStereoMode = nullptr, *pColor = nullptr, *pOversampling = nullptr,
                       *pAutoGain = nullptr, *pScExternal = nullptr;

    std::atomic<float> meterGr { 0.0f }, meterIn { 0.0f }, meterOut { 0.0f };
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ElFratachoAudioProcessor)
};
