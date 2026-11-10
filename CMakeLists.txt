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

# Generar automáticamente el JuceHeader.h
juce_generate_juce_header(La-Pauta-Studio)

# Buscar automáticamente todos los archivos .cpp y .h dentro de la carpeta Source/
file(GLOB_RECURSE SOURCE_FILES 
    "Source/*.cpp"
    "Source/*.h"
)

target_sources(La-Pauta-Studio
    PRIVATE
        ${SOURCE_FILES}
)

target_compile_definitions(La-Pauta-Studio
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
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
