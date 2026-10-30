cmake_minimum_required(VERSION 3.22)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
project(NpnoPlayer VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
if(NOT DEFINED JUCE_PATH)
    message(FATAL_ERROR "Configure -DJUCE_PATH=/path/to/JUCE")
endif()
add_subdirectory("${JUCE_PATH}" juce)
find_program(CARGO cargo HINTS "$ENV{HOME}/.cargo/bin" REQUIRED NO_DEFAULT_PATH)
find_program(RUSTC rustc HINTS "$ENV{HOME}/.cargo/bin" REQUIRED NO_DEFAULT_PATH)
set(BRIDGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bridge")
set(BRIDGE_TARGET "${CMAKE_BINARY_DIR}/rust")
add_custom_command(OUTPUT "${BRIDGE_TARGET}/release/libnpno_bridge.a"
    COMMAND ${CMAKE_COMMAND} -E env RUSTC=${RUSTC} CARGO_TARGET_DIR=${BRIDGE_TARGET} MACOSX_DEPLOYMENT_TARGET=10.13 RUSTFLAGS=-Ctarget-cpu=x86-64 ${CARGO} build --release --offline --target x86_64-apple-darwin --manifest-path "${BRIDGE_DIR}/Cargo.toml"
    COMMAND ${CMAKE_COMMAND} -E env RUSTC=${RUSTC} CARGO_TARGET_DIR=${BRIDGE_TARGET} MACOSX_DEPLOYMENT_TARGET=11.0 RUSTFLAGS= ${CARGO} build --release --offline --target aarch64-apple-darwin --manifest-path "${BRIDGE_DIR}/Cargo.toml"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BRIDGE_TARGET}/release"
    COMMAND /usr/bin/lipo -create "${BRIDGE_TARGET}/x86_64-apple-darwin/release/libnpno_bridge.a" "${BRIDGE_TARGET}/aarch64-apple-darwin/release/libnpno_bridge.a" -output "${BRIDGE_TARGET}/release/libnpno_bridge.a"
    DEPENDS bridge/src/lib.rs bridge/Cargo.toml VERBATIM)
add_custom_target(npno_rust DEPENDS "${BRIDGE_TARGET}/release/libnpno_bridge.a")
add_library(npno_bridge STATIC IMPORTED GLOBAL)
set_target_properties(npno_bridge PROPERTIES IMPORTED_LOCATION "${BRIDGE_TARGET}/release/libnpno_bridge.a")
add_dependencies(npno_bridge npno_rust)
juce_add_plugin(NpnoPlayer
    COMPANY_NAME "Vinizera" BUNDLE_ID "audio.vd.npnoplayer"
    PLUGIN_MANUFACTURER_CODE Vdau PLUGIN_CODE Npno
    FORMATS VST3 Standalone PRODUCT_NAME "Vini Stage"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/ViniStage.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/assets/ViniStage-small.png"
    IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Sampler)
# Replace JUCE's generated ICNS with native ICNS representations, including every
# standard/Retina size. In particular, avoid JUCE's corrupt 16px PNG icp4 chunk.
if(APPLE)
    get_target_property(vini_generated_sources NpnoPlayer JUCE_GENERATED_SOURCES_DIRECTORY)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/assets/ViniStage.icns"
        "${vini_generated_sources}/Icon.icns" COPYONLY)
endif()
juce_generate_juce_header(NpnoPlayer)
target_sources(NpnoPlayer PRIVATE Source/Plugin.cpp Source/Bridge.h Source/Volume.h Source/HardwareUI.h Source/Playback.h)
target_compile_definitions(NpnoPlayer PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(NpnoPlayer PRIVATE npno_bridge juce::juce_audio_utils juce::juce_recommended_config_flags)
add_executable(NpnoLoadBench Source/LoadBench.cpp)
target_link_libraries(NpnoLoadBench PRIVATE npno_bridge "-framework Security")
add_executable(NpnoProgress Source/ProgressSmoke.cpp)
target_link_libraries(NpnoProgress PRIVATE npno_bridge "-framework Security")
juce_add_console_app(NpnoSmoke PRODUCT_NAME "NPno Smoke")
juce_generate_juce_header(NpnoSmoke)
target_sources(NpnoSmoke PRIVATE Source/Smoke.cpp Source/LayerSmoke.cpp Source/PlayabilitySmoke.cpp Source/ReferenceHost.cpp Source/DecaySmoke.cpp)
target_link_libraries(NpnoSmoke PRIVATE npno_bridge juce::juce_audio_utils juce::juce_recommended_config_flags)
target_compile_definitions(NpnoSmoke PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)

juce_add_console_app(NpnoPreview PRODUCT_NAME "Vini Stage UI Preview")
juce_generate_juce_header(NpnoPreview)
target_sources(NpnoPreview PRIVATE Source/Preview.cpp)
target_link_libraries(NpnoPreview PRIVATE NpnoPlayer juce::juce_audio_utils juce::juce_recommended_config_flags)

option(NPNO_INSTALL_AFTER_BUILD "Install the signed VST3 in the user plug-in folder" ON)
if(APPLE AND NPNO_INSTALL_AFTER_BUILD)
    add_custom_command(TARGET NpnoPlayer_VST3 POST_BUILD
        COMMAND /usr/bin/codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:NpnoPlayer_VST3>"
        COMMAND /usr/bin/codesign --verify --deep --strict "$<TARGET_BUNDLE_DIR:NpnoPlayer_VST3>"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$ENV{HOME}/Library/Audio/Plug-Ins/VST3"
        COMMAND /usr/bin/ditto "$<TARGET_BUNDLE_DIR:NpnoPlayer_VST3>" "$ENV{HOME}/Library/Audio/Plug-Ins/VST3/Vini Stage.vst3"
        COMMAND /usr/bin/codesign --verify --deep --strict "$ENV{HOME}/Library/Audio/Plug-Ins/VST3/Vini Stage.vst3"
        COMMENT "Signing and installing Vini Stage.vst3 in the user plug-in folder"
        VERBATIM)
endif()
