juce_add_plugin(WarpedVinylHiFi
    COMPANY_NAME "Round Corner Audio"
    PLUGIN_MANUFACTURER_CODE Rcna
    PLUGIN_CODE Wvhf
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Warped Vinyl HiFi"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Modulation
    COPY_PLUGIN_AFTER_BUILD FALSE)

execute_process(COMMAND git rev-parse --short HEAD WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT_VARIABLE WV_GIT_SHA OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT WV_GIT_SHA)
  set(WV_GIT_SHA "local")
endif()
target_compile_definitions(WarpedVinylHiFi PRIVATE WV_BUILD_ID="${WV_GIT_SHA}")

target_sources(WarpedVinylHiFi PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(WarpedVinylHiFi PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
                           JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(WarpedVinylHiFi PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
