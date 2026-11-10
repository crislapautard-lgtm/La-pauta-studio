cmake_minimum_required(VERSION 3.22)

project(La-Pauta-Studio VERSION 0.0.1)

# Incluir JUCE desde la carpeta descargada
add_subdirectory(JUCE)

juce_add_plugin(La-Pauta-Studio
    PLUGIN_MANUFACTURER_CODE Paut
    PLUGIN_CODE Past
    FORMATS Standalone VST3
    PRODUCT_NAME "La Pauta Studio"
)

# Generar automáticamente el JuceHeader.h para que reconozca todos tus includes sin modificar el código
juce_generate_juce_header(La-Pauta-Studio)

# Archivos del proyecto apuntando a la subcarpeta Source/
target_sources(La-Pauta-Studio
    PRIVATE
        Source/PluginProcessor.h
        Source/PluginProcessor.cpp
        Source/MainComponent.h
        Source/MainComponent.cpp
        Source/TrackComponent.h
        Source/TrackComponent.cpp
)

target_compile_definitions(La-Pauta-Studio
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(La-Pauta-Studio
    PRIVATE
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
