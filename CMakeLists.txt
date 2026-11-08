cmake_minimum_required(VERSION 3.15)
project(La-Pauta-Studio VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 7.0.12
)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(La-Pauta-Studio
    PRODUCT_NAME "La Pauta Studio"
    COMPANY_NAME "La Pauta Studio"
)

juce_generate_juce_header(La-Pauta-Studio)

target_sources(La-Pauta-Studio PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
)

target_compile_definitions(La-Pauta-Studio PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="La Pauta Studio"
    JUCE_APPLICATION_VERSION_STRING="1.0.0"
)

target_link_libraries(La-Pauta-Studio PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_gui_extra
)
