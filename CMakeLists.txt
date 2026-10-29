cmake_minimum_required(VERSION 3.22)

# macOS: build Universal (Apple Silicon + Intel), รองรับ macOS 11 ขึ้นไป
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

project(EduSaturator VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

option(EDU_COPY_AFTER_BUILD "Copy plugin to ~/Library/Audio/Plug-Ins/VST3 after build (macOS)" ON)

juce_add_plugin(EduSaturator
    COMPANY_NAME "EduAudio"
    PLUGIN_MANUFACTURER_CODE Edua
    PLUGIN_CODE Edsa
    FORMATS VST3
    PRODUCT_NAME "Edu Saturator"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Distortion
    COPY_PLUGIN_AFTER_BUILD ${EDU_COPY_AFTER_BUILD})

target_sources(EduSaturator PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h)

target_compile_definitions(EduSaturator PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(EduSaturator
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ทดสอบ DSP แบบ headless (ไม่บังคับ): cmake -DEDU_BUILD_TESTS=ON
option(EDU_BUILD_TESTS "Build headless DSP test" OFF)
if(EDU_BUILD_TESTS)
    juce_add_console_app(EduDspTest PRODUCT_NAME "EduDspTest")
    target_sources(EduDspTest PRIVATE Tests/DspTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(EduDspTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(EduDspTest PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
