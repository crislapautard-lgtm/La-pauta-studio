cmake_minimum_required(VERSION 3.22)

# Runtime estático: el .exe corre en Windows sin instalar Visual C++ Redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(LaPautaStudio VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Opcional: ruta al SDK de ASIO de Steinberg (descargado por ti, no se puede redistribuir).
# Con ASIO activado verás los drivers ASIO de tu tarjeta además de WASAPI y DirectSound.
set(LAPAUTA_ASIO_SDK "" CACHE PATH "Carpeta del SDK de ASIO (opcional)")

# Si pones el SDK dentro del repositorio, en la carpeta "asiosdk" (debe existir asiosdk/common/asio.h),
# se detecta solo y ASIO queda activado; así también funciona en GitHub Actions.
if(NOT LAPAUTA_ASIO_SDK AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/asiosdk/common/asio.h")
    set(LAPAUTA_ASIO_SDK "${CMAKE_CURRENT_SOURCE_DIR}/asiosdk")
endif()
if(LAPAUTA_ASIO_SDK)
    message(STATUS "ASIO activado: ${LAPAUTA_ASIO_SDK}")
else()
    message(STATUS "ASIO desactivado (falta la carpeta asiosdk, ver README)")
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.8
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(LaPautaStudio
    PRODUCT_NAME "La Pauta Studio"
    COMPANY_NAME "La Pauta"
    VERSION 0.1.0)

juce_generate_juce_header(LaPautaStudio)

target_sources(LaPautaStudio PRIVATE
    Source/Main.cpp
    Source/Mp3Writer.cpp
    Source/Mp3Writer.h
    Source/JSFX.h
    Source/Engine.h
    Source/PluginHost.h
    Source/UI.h)

target_compile_definitions(LaPautaStudio PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_VST=0
    JUCE_PLUGINHOST_LADSPA=0
    JUCE_WASAPI=1
    JUCE_DIRECTSOUND=1
    JUCE_APPLICATION_NAME_STRING="La Pauta Studio"
    JUCE_APPLICATION_VERSION_STRING="0.1.0")

if(LAPAUTA_ASIO_SDK)
    target_compile_definitions(LaPautaStudio PRIVATE JUCE_ASIO=1)
    target_include_directories(LaPautaStudio PRIVATE "${LAPAUTA_ASIO_SDK}/common")
endif()

if(MSVC)
    target_compile_options(LaPautaStudio PRIVATE /utf-8)
endif()

if(WIN32)
    target_link_libraries(LaPautaStudio PRIVATE mfplat mfreadwrite mfuuid ole32)
endif()

target_link_libraries(LaPautaStudio
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
