cmake_minimum_required(VERSION 3.22)
project(meowbass VERSION 0.0.1 LANGUAGES C CXX)

include(FetchContent)
FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.16-security GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(juce)
FetchContent_Declare(cat_ui GIT_REPOSITORY https://github.com/SpeakingColors/cat-ui GIT_TAG main)
FetchContent_MakeAvailable(cat_ui)

juce_add_plugin(Meowbass
    COMPANY_NAME "SpeakingColors"
    PLUGIN_MANUFACTURER_CODE Spco
    PLUGIN_CODE Mbas
    FORMATS VST3 Standalone
    PRODUCT_NAME "Meowbass"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE)
target_sources(Meowbass PRIVATE src/common/FxBase.cpp src/common/FxEditor.cpp src/Plugins.cpp src/bass/MeowBass.cpp)
target_include_directories(Meowbass PRIVATE src)
target_compile_definitions(Meowbass PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(Meowbass PRIVATE cat_ui juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
