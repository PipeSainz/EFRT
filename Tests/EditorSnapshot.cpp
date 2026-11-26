// Copyright (c) 2026 FSMAudio. Todos los derechos reservados. Software propietario: ver LICENSE y EULA.txt.
// Renderiza el editor a un PNG (sin ventana). Uso:  EfrtEditorSnapshot salida.png [ancho=480] [preset=0]
// Procesa unos bloques de audio con compresión fuerte antes, para que el medidor muestre algo.
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::String out = argc > 1 ? argv[1] : "editor.png";
    const int width        = argc > 2 ? juce::String (argv[2]).getIntValue() : 480;
    const int preset       = argc > 3 ? juce::String (argv[3]).getIntValue() : 0;

    ElFratachoAudioProcessor proc;
    proc.setCurrentProgram (preset);
    proc.setRateAndBufferSizeDetails (48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    {   // un poco de señal fuerte para llenar los medidores
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        for (int b = 0; b < 40; ++b)
        {
            for (int i = 0; i < 512; ++i)
            {
                const float v = 0.7f * std::sin (0.13f * (float) (b * 512 + i));
                buf.setSample (0, i, v); buf.setSample (1, i, v);
            }
            proc.processBlock (buf, midi);
        }
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    editor->setSize (width, width * 2 / 3);

    // dejar correr el timer del editor unos ciclos (lee los medidores)
    for (int i = 0; i < 4; ++i)
    {
        for (int b = 0; b < 8; ++b) { juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer m; for (int s = 0; s < 512; ++s) { const float v = 0.7f * std::sin (0.13f * (float) s); buf.setSample (0, s, v); buf.setSample (1, s, v); } proc.processBlock (buf, m); }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
    }

    auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (out));
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat png;
    if (! os.openedOk() || ! png.writeImageToStream (image, os)) { std::fprintf (stderr, "no pude escribir %s\n", f.getFullPathName().toRawUTF8()); return 1; }
    std::printf ("ok: %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight());
    return 0;
}
