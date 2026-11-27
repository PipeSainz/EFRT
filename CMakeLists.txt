cmake_minimum_required(VERSION 3.22)

project(ElFratacho VERSION 1.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(EFRT_BUILD_TESTS "Compilar los tests (DSP sin JUCE + procesador completo)" ON)
option(EFRT_ENABLE_LTO  "Usar LTO en el plugin (más lento de compilar, más rápido de correr)" ON)

# Copiar el plugin a la carpeta de plugins del usuario al compilar: sí en Windows/macOS, no en Linux/CI.
if(WIN32 OR APPLE)
    set(EFRT_COPY_DEFAULT TRUE)
else()
    set(EFRT_COPY_DEFAULT FALSE)
endif()
option(EFRT_COPY_AFTER_BUILD "Copiar el plugin compilado a la carpeta de plugins del sistema" ${EFRT_COPY_DEFAULT})

# ---------------------------------------------------------------------------
# JUCE se descarga automáticamente la primera vez que corrés cmake (necesita
# internet). Si ya tenés JUCE clonado en otro lado, pasá:
#   -DFETCHCONTENT_SOURCE_DIR_JUCE=C:/ruta/a/JUCE
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.12
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# AU solo existe en macOS (es lo que pide Logic / GarageBand).
if(APPLE)
    set(EFRT_FORMATS AU VST3 Standalone)
else()
    set(EFRT_FORMATS VST3 Standalone)
endif()

juce_add_plugin(ElFratacho
    COMPANY_NAME "FSMAudio"
    PLUGIN_MANUFACTURER_CODE Fsma
    PLUGIN_CODE Fr76
    FORMATS ${EFRT_FORMATS}
    PRODUCT_NAME "EFRT"
    VST3_CATEGORIES "Fx" "Dynamics"
    AU_MAIN_TYPE kAudioUnitType_Effect
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${EFRT_COPY_AFTER_BUILD}
)

target_sources(ElFratacho PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

# ---------------------------------------------------------------------------
# Assets del diseño (fondo, knob de madera, dot de mármol) embebidos como
# BinaryData dentro del plugin, para no depender de archivos externos en la
# máquina del usuario final.
# ---------------------------------------------------------------------------
juce_add_binary_data(ElFratachoData SOURCES
    Assets/Background.png
    Assets/KnobWood.png
    Assets/DotMarble.png
    Assets/Fonts/JosefinSans-SemiBold.ttf
    Assets/Fonts/JosefinSans-Bold.ttf
)

set_target_properties(ElFratachoData PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_definitions(ElFratacho PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(ElFratacho PRIVATE
    ElFratachoData
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_audio_processors
    juce::juce_gui_basics
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

if(EFRT_ENABLE_LTO)
    target_link_libraries(ElFratacho PUBLIC juce::juce_recommended_lto_flags)
endif()

# ---------------------------------------------------------------------------
# Tests
#   EfrtDspTests        -> núcleo DSP (Source/Fet76Compressor.h), sin JUCE
#   EfrtProcessorTests  -> procesador completo: buses, latencia, oversampling,
#                          sidechain, link, M/S, auto gain, estado, presets...
# Correr con:  ctest --output-on-failure
# ---------------------------------------------------------------------------
if(EFRT_BUILD_TESTS)
    enable_testing()

    add_executable(EfrtDspTests Tests/DspTests.cpp)
    target_include_directories(EfrtDspTests PRIVATE Source)
    add_test(NAME dsp COMMAND EfrtDspTests)

    juce_add_console_app(EfrtProcessorTests PRODUCT_NAME "EfrtProcessorTests")
    target_sources(EfrtProcessorTests PRIVATE
        Tests/ProcessorTests.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
    )
    target_include_directories(EfrtProcessorTests PRIVATE Source)
    target_compile_definitions(EfrtProcessorTests PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_UNIT_TESTS=0
    )
    target_link_libraries(EfrtProcessorTests PRIVATE
        ElFratachoData
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
    )
    add_test(NAME processor COMMAND EfrtProcessorTests)
endif()

# ---------------------------------------------------------------------------
# Herramienta de desarrollo (apagada por defecto): renderiza el editor a un PNG
# sin abrir ninguna ventana, para revisar la UI en CI o sin DAW.
#   cmake -DEFRT_BUILD_SNAPSHOT=ON ...  &&  EfrtEditorSnapshot salida.png 960 1
# ---------------------------------------------------------------------------
option(EFRT_BUILD_SNAPSHOT "Compilar EfrtEditorSnapshot (render del editor a PNG)" OFF)
if(EFRT_BUILD_SNAPSHOT)
    juce_add_console_app(EfrtEditorSnapshot PRODUCT_NAME "EfrtEditorSnapshot")
    target_sources(EfrtEditorSnapshot PRIVATE
        Tests/EditorSnapshot.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
    )
    target_include_directories(EfrtEditorSnapshot PRIVATE Source)
    target_compile_definitions(EfrtEditorSnapshot PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(EfrtEditorSnapshot PRIVATE
        ElFratachoData
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
    )
endif()
