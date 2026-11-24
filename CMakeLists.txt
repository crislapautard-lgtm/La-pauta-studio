cmake_minimum_required(VERSION 3.22)

# Runtime estático: el .exe corre en Windows sin instalar Visual C++ Redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(LaPautaStudio VERSION 0.1.0 LANGUAGES C CXX)
if(WIN32)
    enable_language(RC)
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Opcional: ruta al SDK de ASIO de Steinberg (descargado por ti, no se puede redistribuir).
# Con ASIO activado verás los drivers ASIO de tu tarjeta además de WASAPI y DirectSound.
set(LAPAUTA_ASIO_SDK "" CACHE PATH "Carpeta del SDK de ASIO (opcional)")


include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.8
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

# Busca tu icono: Assets/icon.png, Assets/icon.ico, o cualquier .png/.ico en Assets o en la raiz del repo.
file(GLOB LAPAUTA_ICON_CANDIDATES
    "${CMAKE_CURRENT_SOURCE_DIR}/Assets/icon.png" "${CMAKE_CURRENT_SOURCE_DIR}/Assets/icon.ico"
    "${CMAKE_CURRENT_SOURCE_DIR}/Assets/*.png" "${CMAKE_CURRENT_SOURCE_DIR}/Assets/*.ico"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.png" "${CMAKE_CURRENT_SOURCE_DIR}/*.ico")
list(GET LAPAUTA_ICON_CANDIDATES 0 LAPAUTA_ICON)
if(NOT LAPAUTA_ICON)
    message(FATAL_ERROR "No se encontro ningun icono (.png o .ico) en Assets ni en la raiz del repositorio.")
endif()
message(STATUS "Icono de La Pauta Studio: ${LAPAUTA_ICON}")

# Un .png lo convierte JUCE solo. Un .ico se incrusta directo con un recurso de Windows (JUCE falla al reconvertir algunos .ico).
get_filename_component(LAPAUTA_ICON_EXT "${LAPAUTA_ICON}" LAST_EXT)
string(TOLOWER "${LAPAUTA_ICON_EXT}" LAPAUTA_ICON_EXT)
set(LAPAUTA_JUCE_ICON_ARGS "")
if(LAPAUTA_ICON_EXT STREQUAL ".png")
    set(LAPAUTA_JUCE_ICON_ARGS ICON_BIG "${LAPAUTA_ICON}" ICON_SMALL "${LAPAUTA_ICON}")
else()
    file(WRITE "${CMAKE_BINARY_DIR}/lapauta_icon.rc" "1 ICON \"${LAPAUTA_ICON}\"\n")
endif()

juce_add_gui_app(LaPautaStudio
    PRODUCT_NAME "La Pauta Studio"
    COMPANY_NAME "La Pauta"
    ${LAPAUTA_JUCE_ICON_ARGS}
    VERSION 0.1.0)

juce_generate_juce_header(LaPautaStudio)

if(NOT LAPAUTA_ICON_EXT STREQUAL ".png")
    target_sources(LaPautaStudio PRIVATE "${CMAKE_BINARY_DIR}/lapauta_icon.rc")
endif()

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
