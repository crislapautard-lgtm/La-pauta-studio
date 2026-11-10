
cmake_minimum_required(VERSION 3.22)

project(La-Pauta-Studio VERSION 1.0.0)

# JUCE
add_subdirectory(JUCE)

# Plugin y aplicación Standalone
juce_add_plugin(La-Pauta-Studio
    COMPANY_NAME "La Pauta"
    PLUGIN_MANUFACTURER_CODE Paut
    PLUGIN_CODE Past
    FORMATS Standalone VST3
    PRODUCT_NAME "La Pauta Studio"
    DESCRIPTION "La Pauta Studio"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
)

# Generar JuceHeader.h
juce_generate_juce_header(La-Pauta-Studio)

# Archivos del proyecto
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h"
)

target_sources(La-Pauta-Studio
    PRIVATE
        ${SOURCE_FILES}
)

# Configuración
target_compile_definitions(La-Pauta-Studio
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# Módulos JUCE
target_link_libraries(La-Pauta-Studio
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_basics
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
