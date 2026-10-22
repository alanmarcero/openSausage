cmake_minimum_required(VERSION 3.22)
project(openSausage VERSION 1.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    add_compile_options(/utf-8)
endif()
option(OS_BUILD_PLUGIN "Build native plugin formats and editor capture tool" ON)
option(OS_BUILD_TESTS "Build DSP tests" ON)
include(FetchContent)
add_library(sausage_core STATIC Source/Core/SausageCore.cpp Source/Core/SauceRack.cpp)
target_include_directories(sausage_core PUBLIC Source/Core)
set_target_properties(sausage_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(OS_BUILD_PLUGIN)
    FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.9 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
    juce_add_binary_data(SausageAssets SOURCES assets/sausage-expressions.png assets/analog-cyber-faceplate.png)
    set_target_properties(SausageAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set(OS_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND OS_FORMATS AU)
    endif()
    juce_add_plugin(openSausage
        COMPANY_NAME "Alan Marcero"
        COMPANY_WEBSITE "https://github.com/alanmarcero/openSausage"
        MICROPHONE_PERMISSION_ENABLED TRUE
        MICROPHONE_PERMISSION_TEXT "openSausage processes live audio from your selected input device."
        COPYRIGHT "Copyright (c) 2026 Alan Marcero"
        BUNDLE_ID "com.alanmarcero.opensausage"
        PRODUCT_NAME "openSausage"
        PLUGIN_MANUFACTURER_CODE Amrc
        PLUGIN_CODE Osag
        FORMATS ${OS_FORMATS}
        VST3_CATEGORIES Fx Distortion Dynamics
        IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE)
    target_sources(openSausage PRIVATE
        Source/Plugin/PluginProcessor.cpp
        Source/Plugin/PluginEditor.cpp
        Source/Plugin/SausageLookAndFeel.cpp)
    target_include_directories(openSausage PUBLIC Source/Plugin)
    target_compile_definitions(openSausage PUBLIC
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(openSausage PRIVATE sausage_core SausageAssets
        juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    add_executable(sausage_editor_capture Tools/EditorCapture.cpp)
    set_target_properties(sausage_editor_capture PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/sausage_editor_capture_artefacts/$<CONFIG>")
    target_link_libraries(sausage_editor_capture PRIVATE openSausage sausage_core SausageAssets juce::juce_audio_utils)
    target_compile_definitions(sausage_editor_capture PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
endif()

if(OS_BUILD_TESTS)
    FetchContent_Declare(Catch2 GIT_REPOSITORY https://github.com/catchorg/Catch2.git GIT_TAG v3.9.1 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(Catch2)
    add_executable(sausage_tests Tests/CoreTests.cpp Tests/RackTests.cpp)
    target_link_libraries(sausage_tests PRIVATE sausage_core Catch2::Catch2WithMain)
    enable_testing()
    include(Catch)
    catch_discover_tests(sausage_tests)
endif()
