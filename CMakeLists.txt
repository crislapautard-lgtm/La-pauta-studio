cmake_minimum_required(VERSION 3.15)

project(La-Pauta-Studio VERSION 0.1.0)

# Descargar e integrar JUCE automáticamente si no está presente localmente
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(La-Pauta-Studio
    VERSION 0.1.0
    COMPANY_NAME "La Pauta"
    PRODUCT_NAME "La Pauta Studio"
)

# Registrar todos los archivos fuente y cabeceras del proyecto
target_sources(La-Pauta-Studio
    PRIVATE
        Source/Main.cpp
        Source/MainComponent.cpp
        Source/MainComponent.h
        Source/TrackComponent.cpp
        Source/TrackComponent.h
)

# Enlazar los módulos necesarios de JUCE
target_link_libraries(La-Pauta-Studio
    PRIVATE
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_graphics
        juce::juce_events
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

target_compile_definitions(La-Pauta-Studio
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)
