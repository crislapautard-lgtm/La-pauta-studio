
cmake_minimum_required(VERSION 3.22)

project(La-Pauta-Studio VERSION 0.0.1)

# ============================================================
# LA PAUTA STUDIO
# Configuracion JUCE / CMake
# ============================================================

# Incluir JUCE
add_subdirectory(JUCE)

# Crear aplicacion
juce_add_gui_app(La-Pauta-Studio
    PRODUCT_NAME "La Pauta Studio"
)

# Archivos fuente
target_sources(La-Pauta-Studio
    PRIVATE
        Source/MainComponent.h
        Source/MainComponent.cpp
        Source/TrackComponent.h
        Source/TrackComponent.cpp
)

# Definiciones
target_compile_definitions(La-Pauta-Studio
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

# Librerias JUCE
target_link_libraries(La-Pauta-Studio
    PRIVATE
        juce::juce_audio_utils
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
