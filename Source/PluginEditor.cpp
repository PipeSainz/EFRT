// Copyright (c) 2026 FSMAudio. Todos los derechos reservados. Software propietario: ver LICENSE y EULA.txt.
#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    const juce::Colour kText  (0xffe8e0d4);
    const juce::Colour kAmber (0xffe9a44a);

    juce::String dbText (float db)
    {
        return db <= -99.0f ? juce::String ("-inf") : juce::String (db, 1);
    }
}

//==============================================================================
// Tipografía (Baron Neue)
//==============================================================================
juce::Font efrt::uiFont (float height, bool bold)
{
    // Se crean una sola vez; Typeface::Ptr es ref-counted y se comparte entre instancias del plugin.
    static const juce::Typeface::Ptr regular = juce::Typeface::createSystemTypefaceFor (BinaryData::BaronNeue_otf,     (size_t) BinaryData::BaronNeue_otfSize);
    static const juce::Typeface::Ptr heavy   = juce::Typeface::createSystemTypefaceFor (BinaryData::BaronNeueBold_otf, (size_t) BinaryData::BaronNeueBold_otfSize);

    const auto& face = bold ? heavy : regular;
    if (face == nullptr)                                    // asset corrupto: caer a la fuente por defecto
        return juce::Font (height, bold ? juce::Font::bold : juce::Font::plain);

    return juce::Font (face).withHeight (height);
}

juce::Font WoodKnobLookAndFeel::getLabelFont (juce::Label&)         { return efrt::uiFont (11.0f); }
juce::Font WoodKnobLookAndFeel::getPopupMenuFont()                  { return efrt::uiFont (14.0f); }
juce::Font WoodKnobLookAndFeel::getSliderPopupFont (juce::Slider&)  { return efrt::uiFont (14.0f, true); }

//==============================================================================
// WoodKnobLookAndFeel
//==============================================================================
WoodKnobLookAndFeel::WoodKnobLookAndFeel()
{
    knobImage = juce::ImageCache::getFromMemory (BinaryData::KnobWood_png, BinaryData::KnobWood_pngSize);
}

void WoodKnobLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPosProportional, float rotaryStartAngle,
                                             float rotaryEndAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);

    // El knob es cuadrado: usamos el lado mas chico para no deformar la imagen.
    auto diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
    auto imgBounds = juce::Rectangle<float> (0.0f, 0.0f, diameter, diameter)
                        .withCentre (bounds.getCentre());

    auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    g.saveState();
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.addTransform (juce::AffineTransform::rotation (angle, bounds.getCentreX(), bounds.getCentreY()));
    g.drawImage (knobImage, imgBounds, juce::RectanglePlacement::stretchToFit);
    g.restoreState();
}

void WoodKnobLookAndFeel::drawLinearSlider (juce::Graphics& g, int, int, int, int,
                                             float, float, float,
                                             const juce::Slider::SliderStyle, juce::Slider& slider)
{
    // Usado solo por el control de Mix: no dibujamos ningun track, solo el
    // texto "MIX: xx", que ademas responde al arrastre porque el Slider
    // entero (aunque invisible) sigue procesando el mouse.
    //
    // Se dibuja en los limites completos del slider, no en el rectangulo
    // (width, height) que JUCE pasa: para un slider vertical de 16 px de alto
    // JUCE le resta el radio del "thumb" y ese rectangulo llega con altura 0,
    // asi que el texto "MIX: xx" nunca se veia.
    const auto bounds = slider.getLocalBounds().toFloat();

    g.setColour (kText);
    g.setFont (efrt::uiFont (13.0f, true));
    g.drawText ("MIX: " + juce::String ((int) std::round (slider.getValue())),
                bounds, juce::Justification::centredRight);
}

//==============================================================================
// Chip
//==============================================================================
Chip::Chip (Kind k, juce::String labelText, juce::String tooltipText)
    : kind (k), label (std::move (labelText))
{
    setWantsKeyboardFocus (false);
    setTooltip (tooltipText);
    setMouseCursor (k == Kind::Drag ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::PointingHandCursor);
}

void Chip::attach (juce::RangedAudioParameter& p, Formatter fmt)
{
    param = &p;
    formatter = std::move (fmt);
    attachment = std::make_unique<juce::ParameterAttachment> (p, [this] (float) { repaint(); });
    attachment->sendInitialUpdate();
}

juce::String Chip::getDisplayText() const
{
    if (kind == Kind::Action)
        return actionText ? actionText() : label;

    if (param == nullptr)
        return label;

    if (formatter)
        return formatter (*param);

    return (label + " " + param->getCurrentValueAsText()).toUpperCase();
}

bool Chip::isNonDefault() const
{
    return param != nullptr && std::abs (param->getValue() - param->getDefaultValue()) > 1.0e-4f;
}

void Chip::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.5f);

    g.setColour (juce::Colours::black.withAlpha (hover || dragging ? 0.55f : 0.35f));
    g.fillRoundedRectangle (b, 4.0f);

    g.setColour (dragging || (kind == Kind::Action) ? kText : (isNonDefault() ? kAmber : kText.withAlpha (0.85f)));
    if (hover) g.setColour (juce::Colours::white);
    g.setFont (efrt::uiFont (10.5f, true));
    g.drawText (getDisplayText(), getLocalBounds(), juce::Justification::centred, true);
}

void Chip::cycle()
{
    if (param == nullptr) return;

    const int steps = juce::jmax (2, param->getNumSteps());
    const int cur   = (int) std::lround (param->convertFrom0to1 (param->getValue()));
    const int next  = (cur + 1) % steps;

    param->beginChangeGesture();
    param->setValueNotifyingHost (param->convertTo0to1 ((float) next));
    param->endChangeGesture();
}

void Chip::mouseDown (const juce::MouseEvent&)
{
    moved = false;
    if (kind == Kind::Drag && param != nullptr)
    {
        dragging = true;
        dragStartNorm = param->getValue();
        param->beginChangeGesture();
        repaint();
    }
}

void Chip::mouseDrag (const juce::MouseEvent& e)
{
    if (kind != Kind::Drag || param == nullptr || ! dragging) return;

    const int dy = e.getDistanceFromDragStartY();
    if (std::abs (dy) > 2) moved = true;

    const float pixelsForFullRange = e.mods.isShiftDown() ? 700.0f : 180.0f;
    param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, dragStartNorm - (float) dy / pixelsForFullRange));
}

void Chip::mouseUp (const juce::MouseEvent& e)
{
    if (kind == Kind::Drag && dragging)
    {
        dragging = false;
        param->endChangeGesture();
        repaint();
    }
    else if (! moved && getLocalBounds().contains (e.getPosition()))
    {
        if (kind == Kind::Cycle)  cycle();
        if (kind == Kind::Action && onAction) onAction();
    }
}

void Chip::mouseDoubleClick (const juce::MouseEvent&)
{
    if (kind != Kind::Drag || param == nullptr) return;

    param->beginChangeGesture();
    param->setValueNotifyingHost (param->getDefaultValue());
    param->endChangeGesture();
}

void Chip::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (kind != Kind::Drag || param == nullptr) return;

    param->beginChangeGesture();
    param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, param->getValue() + (w.deltaY > 0 ? 0.02f : -0.02f)));
    param->endChangeGesture();
}

//==============================================================================
// InfoStrip
//==============================================================================
void InfoStrip::setValues (float newIn, float newOut, float newGr)
{
    auto same = [] (float a, float b) { return std::abs (a - b) < 0.05f; };
    if (same (newIn, inDb) && same (newOut, outDb) && same (newGr, grDb) && newGr >= grHoldDb)
        return;

    inDb = newIn; outDb = newOut; grDb = newGr;
    grHoldDb = juce::jmax (newGr, grHoldDb - 0.6f);      // marca de pico con caída lenta
    repaint();
}

void InfoStrip::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const juce::Rectangle<float> bar (24.0f, 2.0f, b.getWidth() - 26.0f, 8.0f);

    g.setColour (kText);
    g.setFont (efrt::uiFont (10.0f, true));
    g.drawText ("GR", 0, 0, 22, 12, juce::Justification::centredLeft);

    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (bar, 3.0f);

    constexpr float kMaxGr = 24.0f;
    const float frac = juce::jlimit (0.0f, 1.0f, grDb / kMaxGr);
    g.setColour (kAmber);
    g.fillRoundedRectangle (bar.withWidth (juce::jmax (0.0f, bar.getWidth() * frac)), 3.0f);

    const float holdX = bar.getX() + bar.getWidth() * juce::jlimit (0.0f, 1.0f, grHoldDb / kMaxGr);
    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.fillRect (holdX - 0.5f, bar.getY(), 1.0f, bar.getHeight());

    g.setColour (kText.withAlpha (0.9f));
    g.setFont (efrt::uiFont (10.0f));
    g.drawText ("IN " + dbText (inDb) + "   OUT " + dbText (outDb) + "   GR " + juce::String (grDb, 1) + " dB",
                0, 13, (int) b.getWidth(), 12, juce::Justification::centredLeft);
}

//==============================================================================
// ElFratachoAudioProcessorEditor
//==============================================================================
ElFratachoAudioProcessorEditor::ElFratachoAudioProcessorEditor (ElFratachoAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      presetChip (Chip::Kind::Action, "PRESET", "Presets de fábrica (puntos de partida)"),
      scChip     (Chip::Kind::Cycle,  "SC",     "Sidechain: click para alternar interno / externo"),
      hpfChip    (Chip::Kind::Drag,   "HPF",    "HPF del sidechain (Off = 20 Hz). Arrastrar; doble click = Off"),
      linkChip   (Chip::Kind::Drag,   "LINK",   "Link estéreo. 100% = la misma reducción en L y R"),
      modeChip   (Chip::Kind::Cycle,  "MODE",   "Modo estéreo: L/R o M/S"),
      colorChip  (Chip::Kind::Drag,   "COLOR",  "Saturación FET/transformador. 0 = totalmente limpio"),
      osChip     (Chip::Kind::Cycle,  "OS",     "Oversampling del saturador (menos aliasing, agrega latencia)"),
      autoChip   (Chip::Kind::Cycle,  "AUTO",   "Auto gain: iguala la sonoridad de salida con la de entrada")
{
    setLookAndFeel (&woodLookAndFeel);      // menú de presets y popups heredan la tipografía
    canvas.background = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);
    dotImage          = juce::ImageCache::getFromMemory (BinaryData::DotMarble_png, BinaryData::DotMarble_pngSize);
    addAndMakeVisible (canvas);

    // --- Los 4 knobs principales (Input, Attack, Release, Output) ---
    for (auto* s : { &inputSlider, &attackSlider, &releaseSlider, &outputSlider })
    {
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 15);
        s->setColour (juce::Slider::textBoxTextColourId, kText);
        s->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s->setLookAndFeel (&woodLookAndFeel);
        canvas.addAndMakeVisible (s);
    }

    auto& apvts = audioProcessor.apvts;
    inputAttach   = std::make_unique<SliderAttachment> (apvts, ParamID::inputGain,  inputSlider);
    attackAttach  = std::make_unique<SliderAttachment> (apvts, ParamID::attack,     attackSlider);
    releaseAttach = std::make_unique<SliderAttachment> (apvts, ParamID::release,    releaseSlider);
    outputAttach  = std::make_unique<SliderAttachment> (apvts, ParamID::outputGain, outputSlider);

    // --- Control de Mix: el texto "MIX: xx" funciona como slider vertical ---
    mixDragSlider.setSliderStyle (juce::Slider::LinearVertical);
    mixDragSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    mixDragSlider.setVelocityBasedMode (true);
    mixDragSlider.setVelocityModeParameters (0.6, 1, 0.0, false);
    mixDragSlider.setLookAndFeel (&woodLookAndFeel);
    mixDragSlider.setTooltip ("Mix dry/wet (compresión paralela). Doble click = 100");
    canvas.addAndMakeVisible (mixDragSlider);
    mixAttach = std::make_unique<SliderAttachment> (apvts, ParamID::mix, mixDragSlider);

    // --- Los 5 "botones" de ratio (4:1, 8:1, 12:1, 20:1, !!!) ---
    for (int i = 0; i < (int) ratioDots.size(); ++i)
    {
        auto dot = std::make_unique<RatioDot> (dotImage);
        dot->onClick = [this, i]
        {
            if (auto* param = audioProcessor.apvts.getParameter (ParamID::ratio))
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost (param->convertTo0to1 ((float) i));
                param->endChangeGesture();
            }
        };
        canvas.addAndMakeVisible (*dot);
        ratioDots[(size_t) i] = std::move (dot);
    }

    // El aro sigue al parámetro (automatización, presets, otro control) sin polling.
    ratioAttach = std::make_unique<juce::ParameterAttachment> (*apvts.getParameter (ParamID::ratio),
                                                               [this] (float v) { updateRatioDots ((int) std::lround (v)); });
    ratioAttach->sendInitialUpdate();

    // --- Medidor y controles nuevos ---
    canvas.addAndMakeVisible (infoStrip);

    presetChip.actionText = [this] { return "PRESET: " + audioProcessor.getProgramName (audioProcessor.getCurrentProgram()).toUpperCase() + "  v"; };
    presetChip.onAction   = [this] { showPresetMenu(); };

    scChip.attach    (*apvts.getParameter (ParamID::scExternal), [] (const juce::RangedAudioParameter& q) { return juce::String (q.getValue() > 0.5f ? "SC EXT" : "SC INT"); });
    hpfChip.attach   (*apvts.getParameter (ParamID::scHpf));
    linkChip.attach  (*apvts.getParameter (ParamID::link));
    modeChip.attach  (*apvts.getParameter (ParamID::stereoMode), [] (const juce::RangedAudioParameter& q) { return q.getCurrentValueAsText().toUpperCase(); });
    colorChip.attach (*apvts.getParameter (ParamID::color));
    osChip.attach    (*apvts.getParameter (ParamID::oversampling));
    autoChip.attach  (*apvts.getParameter (ParamID::autoGain));

    for (auto* c : { &presetChip, &scChip, &hpfChip, &linkChip, &modeChip, &colorChip, &osChip, &autoChip })
        canvas.addAndMakeVisible (c);

    // --- Tamaño: redimensionable con aspecto fijo (el tamaño se guarda en el estado) ---
    setResizeLimits (360, 240, 1440, 960);
    getConstrainer()->setFixedAspectRatio ((double) kBaseWidth / (double) kBaseHeight);
    setResizable (true, true);

    const int savedWidth = juce::jlimit (360, 1440, (int) apvts.state.getProperty ("uiWidth", kBaseWidth));
    setSize (savedWidth, savedWidth * kBaseHeight / kBaseWidth);

    startTimerHz (30);
}

ElFratachoAudioProcessorEditor::~ElFratachoAudioProcessorEditor()
{
    for (auto* s : { &inputSlider, &attackSlider, &releaseSlider, &outputSlider, &mixDragSlider })
        s->setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void ElFratachoAudioProcessorEditor::resized()
{
    // Coordenadas medidas directamente sobre el mockup (480x320); el canvas se escala.
    const float scale = (float) getWidth() / (float) kBaseWidth;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    canvas.setBounds (0, 0, kBaseWidth, kBaseHeight);

    audioProcessor.apvts.state.setProperty ("uiWidth", getWidth(), nullptr);

    inputSlider.setBounds   (62,  210, 64, 79);
    attackSlider.setBounds  (159, 210, 64, 79);
    releaseSlider.setBounds (256, 210, 64, 79);
    outputSlider.setBounds  (354, 210, 64, 79);

    mixDragSlider.setBounds (380, 4, 60, 16);

    const int dotCentersX[5] = { 142, 190, 239, 288, 337 };
    for (int i = 0; i < 5; ++i)
        ratioDots[(size_t) i]->setBounds (dotCentersX[i] - 16, 123 - 16, 32, 32);

    // Medidor + presets (arriba a la izquierda, sobre la madera lisa)
    infoStrip.setBounds  (12, 6, 150, 26);
    presetChip.setBounds (12, 36, 150, 18);

    // Tira inferior de controles. Queda libre la esquina inferior derecha: ahi JUCE dibuja el cartel
    // "Made with JUCE" (licencia gratuita/GPL) y taparia los controles. OS y AUTO van arriba a la derecha.
    const int chipW = 68, chipH = 18, chipY = 296;
    int x = 8;
    for (auto* c : { &scChip, &hpfChip, &linkChip, &modeChip, &colorChip })
    {
        c->setBounds (x, chipY, chipW, chipH);
        x += chipW;
    }
    osChip.setBounds   (372, 24, 52, 17);
    autoChip.setBounds (426, 24, 52, 17);
}

void ElFratachoAudioProcessorEditor::timerCallback()
{
    auto toDb = [] (float lin) { return lin > 1.0e-5f ? juce::Decibels::gainToDecibels (lin) : -100.0f; };

    const float inPeak  = toDb (audioProcessor.readAndResetInputPeak());
    const float outPeak = toDb (audioProcessor.readAndResetOutputPeak());
    const float gr      = audioProcessor.readAndResetGainReductionDb();

    // Los picos caen suave (no parpadean entre bloques); la GR tiene release visual.
    meterIn = juce::jmax (inPeak,  meterIn  - 1.2f);
    meterOut = juce::jmax (outPeak, meterOut - 1.2f);
    meterGr  = juce::jmax (gr,      meterGr  - 1.5f);

    infoStrip.setValues (meterIn, meterOut, meterGr);
    presetChip.repaint();
}

void ElFratachoAudioProcessorEditor::updateRatioDots (int currentIndex)
{
    for (int i = 0; i < (int) ratioDots.size(); ++i)
        ratioDots[(size_t) i]->setSelected (i == currentIndex);
}

void ElFratachoAudioProcessorEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    for (int i = 0; i < audioProcessor.getNumPrograms(); ++i)
        menu.addItem (i + 1, audioProcessor.getProgramName (i), true, i == audioProcessor.getCurrentProgram());

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetChip),
                        [this] (int result)
                        {
                            if (result > 0)
                                audioProcessor.setCurrentProgram (result - 1);
                            presetChip.repaint();
                        });
}
