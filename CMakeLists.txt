cmake_minimum_required(VERSION 3.22)
project(SlewClip VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    GIT_CONFIG    advice.detachedHead=false)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(SlewClip
    COMPANY_NAME            FadeToFlow
    BUNDLE_ID               com.fadetoflow.slewclip
    PLUGIN_MANUFACTURER_CODE FTFl
    PLUGIN_CODE             Slwc
    FORMATS                 VST3 LV2 AU Standalone
    LV2URI                  "https://github.com/FadeToFlow/SlewClip"
    VST3_CATEGORIES         Fx Distortion
    PRODUCT_NAME            "SlewClip")

juce_generate_juce_header(SlewClip)

target_sources(SlewClip PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(SlewClip PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SlewClip PRIVATE
    juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)