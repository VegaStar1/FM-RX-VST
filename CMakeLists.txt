cmake_minimum_required(VERSION 3.22)
project(FmRx VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(FMRX_BUILD_PLUGIN "Собирать VST3 (нужен JUCE)" ON)

# Юнит-тесты FM-движка (чистый C++, без JUCE): ctest -C Release --output-on-failure
enable_testing()
add_executable(fmrx_tests tests/test_fm.cpp Source/Engine/FmBand.cpp)
add_executable(fmrx_player_tests tests/test_player.cpp Source/Stations/StationPrep.cpp)
add_executable(fmrx_control_tests tests/test_control.cpp)
add_executable(fmrx_render_demo tests/render_demo.cpp Source/Engine/FmBand.cpp)
foreach(t fmrx_tests fmrx_player_tests fmrx_control_tests fmrx_render_demo)
    target_include_directories(${t} PRIVATE Source)
    target_compile_features(${t} PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(${t} PRIVATE /W4 /permissive- /O2)
    else()
        target_compile_options(${t} PRIVATE -Wall -Wextra)
    endif()
endforeach()
add_test(NAME fm     COMMAND fmrx_tests)
add_test(NAME player COMMAND fmrx_player_tests)
add_test(NAME control COMMAND fmrx_control_tests)

if(NOT FMRX_BUILD_PLUGIN)
    return()
endif()

# JUCE: либо локальная копия (-DFMRX_JUCE_DIR=C:/dev/JUCE), либо закреплённый тег с GitHub.
set(FMRX_JUCE_DIR "" CACHE PATH "Путь к локальному JUCE (необязательно)")
if(FMRX_JUCE_DIR)
    add_subdirectory(${FMRX_JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(FmRx
    COMPANY_NAME "NUWMUN"
    PRODUCT_NAME "FM RX"
    PLUGIN_MANUFACTURER_CODE Nuwm
    PLUGIN_CODE Fmrx
    FORMATS VST3
    VST3_CATEGORIES Fx
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(FmRx PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Engine/FmBand.cpp
    Source/Stations/StationPrep.cpp
    Source/Stations/StationManager.cpp)

target_include_directories(FmRx PRIVATE Source)

target_compile_definitions(FmRx PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(FmRx
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_warning_flags)

# Проверка без DAW (локально): процессор + редактор в консольном хосте, рендер скриншота UI.
option(FMRX_BUILD_CHECK "Собирать консольную проверку плагина" OFF)
if(FMRX_BUILD_CHECK)
    juce_add_console_app(FmRxCheck PRODUCT_NAME "FmRxCheck")
    target_sources(FmRxCheck PRIVATE tests/plugin_check.cpp)
    target_include_directories(FmRxCheck PRIVATE Source)
    target_compile_definitions(FmRxCheck PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(FmRxCheck PRIVATE FmRx juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
