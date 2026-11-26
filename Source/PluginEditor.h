// Copyright (c) 2026 FSMAudio. Todos los derechos reservados. Software propietario: ver LICENSE y EULA.txt.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

//==============================================================================
// Tipografía de la UI: Baron Neue (embebida como BinaryData). `height` es la altura
// en el mismo sentido que juce::Font (13.0f), así que reemplaza 1:1 a las llamadas viejas.
namespace efrt
{
    juce::Font uiFont (float height, bool bold = false);
}

//==============================================================================
// LookAndFeel que dibuja el knob de madera (imagen) rotando según el valor,
// en vez del knob genérico de JUCE.
class WoodKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    WoodKnobLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                            float sliderPos, float minSliderPos, float maxSliderPos,
                            const juce::Slider::SliderStyle style, juce::Slider&) override;

    // Texto de los knobs (valores), menú de presets y popups: todo en Baron Neue.
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getSliderPopupFont (juce::Slider&) override;

private:
    juce::Image knobImage;
};

//==============================================================================
// Uno de los 5 "botones" de ratio (4:1, 8:1, 12:1, 20:1, !!!), dibujados con
// el asset de mármol y un aro cuando está seleccionado.
class RatioDot : public juce::Component
{
public:
    explicit RatioDot (juce::Image dotImage) : image (std::move (dotImage))
    {
        // Sin esto, JUCE dibuja un cuadrado de "foco de teclado" alrededor
        // del componente al hacer click — no queremos eso, queremos solo
        // nuestro propio aro circular.
        setWantsKeyboardFocus (false);
    }

    void paint (juce::Graphics& g) override
    {
        // El componente es mas grande que el dot en si (ver resized() en el
        // editor), para darle lugar al aro sin que JUCE lo recorte.
        auto imageBounds = getLocalBounds().toFloat().reduced (6.0f);

        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (image, imageBounds, juce::RectanglePlacement::stretchToFit);

        if (selected)
        {
            auto ringBounds = imageBounds.expanded (3.0f);

            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.drawEllipse (ringBounds.expanded (1.5f), 3.0f);

            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawEllipse (ringBounds, 1.5f);
        }
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        if (onClick) onClick();
    }

    void setSelected (bool s)
    {
        if (selected != s) { selected = s; repaint(); }
    }

    std::function<void()> onClick;

private:
    juce::Image image;
    bool selected = false;
};

//==============================================================================
// Control de texto compacto, mismo estilo que el "MIX: xx" original:
//   Drag   -> arrastrar arriba/abajo cambia el valor (Shift = fino), doble click = default,
//             rueda del mouse = paso.
//   Cycle  -> click para pasar a la siguiente opción (parámetros Choice / Bool).
//   Action -> click ejecuta onAction (usado por el selector de presets).
class Chip : public juce::Component,
             public juce::SettableTooltipClient
{
public:
    enum class Kind { Drag, Cycle, Action };

    Chip (Kind k, juce::String labelText, juce::String tooltipText);

    using Formatter = std::function<juce::String (const juce::RangedAudioParameter&)>;
    void attach (juce::RangedAudioParameter& p, Formatter fmt = {});

    std::function<juce::String()> actionText;     // solo Kind::Action
    std::function<void()> onAction;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true;  repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hover = false; repaint(); }

private:
    juce::String getDisplayText() const;
    bool isNonDefault() const;
    void cycle();

    Kind kind;
    juce::String label;
    Formatter formatter;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;

    bool hover = false, dragging = false, moved = false;
    float dragStartNorm = 0.0f;
};

//==============================================================================
// Medidor de reducción de ganancia + picos de entrada/salida.
class InfoStrip : public juce::Component
{
public:
    void setValues (float inDb, float outDb, float grDb);
    void paint (juce::Graphics&) override;

private:
    float inDb = -100.0f, outDb = -100.0f, grDb = 0.0f, grHoldDb = 0.0f;
};

//==============================================================================
// Lienzo de 480x320 con todo adentro. El editor lo escala con un transform, así
// que la UI se puede redimensionar (aspecto fijo) sin tocar las coordenadas.
class Canvas : public juce::Component
{
public:
    void paint (juce::Graphics& g) override
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (background, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    }

    juce::Image background;
};

//==============================================================================
class ElFratachoAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit ElFratachoAudioProcessorEditor (ElFratachoAudioProcessor&);
    ~ElFratachoAudioProcessorEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    static constexpr int kBaseWidth = 480, kBaseHeight = 320;

private:
    void timerCallback() override;
    void updateRatioDots (int currentIndex);
    void showPresetMenu();

    ElFratachoAudioProcessor& audioProcessor;

    Canvas canvas;
    WoodKnobLookAndFeel woodLookAndFeel;

    juce::Slider inputSlider, attackSlider, releaseSlider, outputSlider;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> inputAttach, attackAttach, releaseAttach, outputAttach;

    // Control de Mix: no es un knob, es el texto "MIX: xx" arriba a la derecha,
    // que funciona como slider vertical invisible (arrastrar arriba/abajo).
    juce::Slider mixDragSlider;
    std::unique_ptr<SliderAttachment> mixAttach;

    juce::Image dotImage;
    std::array<std::unique_ptr<RatioDot>, 5> ratioDots;
    std::unique_ptr<juce::ParameterAttachment> ratioAttach;

    InfoStrip infoStrip;
    float meterIn = -100.0f, meterOut = -100.0f, meterGr = 0.0f;   // valores mostrados (con caída suave)
    Chip presetChip, scChip, hpfChip, linkChip, modeChip, colorChip, osChip, autoChip;

    juce::TooltipWindow tooltipWindow { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ElFratachoAudioProcessorEditor)
};
