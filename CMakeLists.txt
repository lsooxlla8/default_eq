cmake_minimum_required(VERSION 3.15)
project(default_equalizer VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)

# Universal binary for macOS (Intel + Apple Silicon)
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build universal binary")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
endif()

# ---- JUCE ----
# Option A: JUCE as submodule in ./JUCE (recommended)
#   git submodule add https://github.com/juce-framework/JUCE.git JUCE
# Option B: Point JUCE_DIR to your local JUCE folder:
#   cmake -DJUCE_DIR=/path/to/JUCE ..
#
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
  add_subdirectory(JUCE)
elseif (DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
  add_subdirectory("${JUCE_DIR}" JUCE)
else()
  message(FATAL_ERROR "JUCE not found. Add as submodule in ./JUCE or pass -DJUCE_DIR=/path/to/JUCE")
endif()

set(DEFAULT_EQUALIZER_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND DEFAULT_EQUALIZER_FORMATS AU)
endif()

juce_add_plugin(DefaultEqualizer
    COMPANY_NAME "icanseesounds"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Icss
    PLUGIN_CODE Deq1
    FORMATS ${DEFAULT_EQUALIZER_FORMATS}
    # juce_vst3_helper.exe loads the built plugin to emit moduleinfo.json and
    # has been observed hanging indefinitely on Windows CI. The manifest is
    # optional metadata, so skip generating it during the build.
    VST3_AUTO_MANIFEST FALSE
    PRODUCT_NAME "default_equalizer"
    BUNDLE_ID "com.icanseesounds.default-equalizer"
    COMPANY_URL "https://github.com/lsooxlla8/default_equalizer"
    DESCRIPTION "Eight-band parametric dynamic equalizer"
)

target_sources(DefaultEqualizer PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Config.h
    Source/DSP/Biquad.h
    Source/DSP/SvfBiquad.h
    Source/DSP/EQBand.h
    Source/DSP/DriveAutoGainTable.h
    Source/DSP/SpectrumFIFO.h
    Source/DSP/LinearPhaseEngine.h
    Source/DSP/MatchEQ.h
    Source/DSP/GlobalBypass.h
    Source/UI/ResponseCurveComponent.h
    Source/UI/ResponseCurveComponent.cpp
    Source/UI/DefaultFamilyUI.h
    Source/UI/DefaultFamilyUI.cpp
    Source/UI/LevelMeter.h
)

target_compile_definitions(DefaultEqualizer PRIVATE
    DEFAULT_EQUALIZER_FULL=1
    DEFAULT_EQUALIZER_OPEN_SOURCE=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(DefaultEqualizer PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_cryptography
    juce::juce_recommended_config_flags
)

# ── Optional test target (standalone, no JUCE dependency) ────────────
option(DEFAULT_EQUALIZER_BUILD_TESTS "Build unit tests" OFF)

if (DEFAULT_EQUALIZER_BUILD_TESTS)
    enable_testing()
    add_executable(DefaultEqualizer_Tests Tests/BiquadTest.cpp)
    target_compile_features(DefaultEqualizer_Tests PRIVATE cxx_std_17)

    # Shelf regression guards. Both are standalone (Biquad.h pulls only
    # <cmath>/<algorithm>) and return non-zero on failure, so CI fails loudly if
    # the shelf coefficient domain error ever reappears.
    add_executable(DefaultEqualizer_ShelfNaNSweep Tests/ShelfNaNSweepTest.cpp)
    target_compile_features(DefaultEqualizer_ShelfNaNSweep PRIVATE cxx_std_17)

    add_executable(DefaultEqualizer_ShelfResponse Tests/ShelfResponseTest.cpp)
    target_compile_features(DefaultEqualizer_ShelfResponse PRIVATE cxx_std_17)

    # These three existed as standalone sources but were not built by anything,
    # so the claims they substantiate were never actually verified on any commit.
    # BiquadVsSvfComparison in particular is cited in SvfBiquad.h as evidence
    # that RBJ and SVF are identical, so it needs to run.
    add_executable(DefaultEqualizer_BiquadVsSvf Tests/BiquadVsSvfComparison.cpp)
    target_compile_features(DefaultEqualizer_BiquadVsSvf PRIVATE cxx_std_17)

    add_executable(DefaultEqualizer_SvfTest Tests/SvfTest.cpp)
    target_compile_features(DefaultEqualizer_SvfTest PRIVATE cxx_std_17)

    add_executable(DefaultEqualizer_EngineIsolation Tests/EngineIsolationTest.cpp)
    target_compile_features(DefaultEqualizer_EngineIsolation PRIVATE cxx_std_17)

    add_executable(DefaultEqualizer_AuditTests Tests/AuditRegressionTest.cpp)
    target_compile_features(DefaultEqualizer_AuditTests PRIVATE cxx_std_17)
    find_package(Threads REQUIRED)
    target_link_libraries(DefaultEqualizer_AuditTests PRIVATE Threads::Threads)

    add_executable(DefaultEqualizer_LatencyProbe Tests/LatencyProbe.cpp)
    target_link_libraries(DefaultEqualizer_LatencyProbe PRIVATE juce::juce_dsp)

    add_executable(DefaultEqualizer_DspIntegration Tests/DspIntegrationTest.cpp)
    target_link_libraries(DefaultEqualizer_DspIntegration PRIVATE DefaultEqualizer juce::juce_audio_utils juce::juce_dsp)

    add_test(NAME biquad COMMAND DefaultEqualizer_Tests)
    add_test(NAME shelf_nan_sweep COMMAND DefaultEqualizer_ShelfNaNSweep)
    add_test(NAME shelf_response COMMAND DefaultEqualizer_ShelfResponse)
    add_test(NAME biquad_vs_svf COMMAND DefaultEqualizer_BiquadVsSvf)
    add_test(NAME svf COMMAND DefaultEqualizer_SvfTest)
    add_test(NAME engine_isolation COMMAND DefaultEqualizer_EngineIsolation)
    add_test(NAME audit_regressions COMMAND DefaultEqualizer_AuditTests)
    add_test(NAME latency_probe COMMAND DefaultEqualizer_LatencyProbe)
    add_test(NAME dsp_integration COMMAND DefaultEqualizer_DspIntegration)
endif()
