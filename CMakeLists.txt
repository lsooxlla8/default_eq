cmake_minimum_required(VERSION 3.15)
project(default_eq VERSION 0.4.0)

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

set(DEFAULT_EQ_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND DEFAULT_EQ_FORMATS AU)
endif()

juce_add_plugin(DefaultEQ
    COMPANY_NAME "icanseesounds"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Icss
    PLUGIN_CODE Dfeq
    FORMATS ${DEFAULT_EQ_FORMATS}
    # juce_vst3_helper.exe loads the built plugin to emit moduleinfo.json and
    # has been observed hanging indefinitely on Windows CI. The manifest is
    # optional metadata, so skip generating it during the build.
    VST3_AUTO_MANIFEST FALSE
    PRODUCT_NAME "default_eq"
    BUNDLE_ID "com.icanseesounds.default-eq"
    COMPANY_URL "https://github.com/lsooxlla8/default_eq"
    DESCRIPTION "Eight-band parametric dynamic equalizer"
)

target_sources(DefaultEQ PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/ProcessorParameters.cpp
    Source/ProcessorRouting.cpp
    Source/ProcessorState.cpp
    Source/ProcessorLinearPhase.cpp
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/EditorControls.cpp
    Source/EditorBandControls.cpp
    Source/Config.h
    Source/DSP/Biquad.h
    Source/DSP/FilterTypes.h
    Source/DSP/SvfBiquad.h
    Source/DSP/EQBand.h
    Source/DSP/EQBandProcessing.inl
    Source/DSP/EQBandDrive.inl
    Source/DSP/EQBandInternals.inl
    Source/DSP/ZLFilter.h
    Source/DSP/TransientSplitter.h
    Source/DSP/DriveAutoGainTable.h
    Source/DSP/EQAutoGain.h
    Source/DSP/KWeightedLoudness.h
    Source/DSP/SpectrumFIFO.h
    Source/DSP/LinearPhaseEngine.h
    Source/DSP/GlobalBypass.h
    Source/UI/ResponseCurveComponent.h
    Source/UI/ResponseCurveComponent.cpp
    Source/UI/ResponseCurveDrawing.cpp
    Source/UI/ResponseCurveInteraction.cpp
    Source/UI/DefaultFamilyUI.h
    Source/UI/DefaultFamilyUI.cpp
    Source/UI/DriveCharacterFormatting.h
    Source/UI/ContextMenuLayout.h
    Source/UI/EditorLayout.h
)

target_compile_definitions(DefaultEQ PRIVATE
    DEFAULT_EQ_FULL=1
    DEFAULT_EQ_OPEN_SOURCE=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(DefaultEQ PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
)

# ── Optional test target (standalone, no JUCE dependency) ────────────
option(DEFAULT_EQ_BUILD_TESTS "Build unit tests" OFF)

if (DEFAULT_EQ_BUILD_TESTS)
    enable_testing()
    add_executable(DefaultEQ_Tests Tests/BiquadTest.cpp)
    target_compile_features(DefaultEQ_Tests PRIVATE cxx_std_17)

    # Shelf regression guards. Both are standalone (Biquad.h pulls only
    # <cmath>/<algorithm>) and return non-zero on failure, so CI fails loudly if
    # the shelf coefficient domain error ever reappears.
    add_executable(DefaultEQ_ShelfNaNSweep Tests/ShelfNaNSweepTest.cpp)
    target_compile_features(DefaultEQ_ShelfNaNSweep PRIVATE cxx_std_17)

    add_executable(DefaultEQ_ShelfResponse Tests/ShelfResponseTest.cpp)
    target_compile_features(DefaultEQ_ShelfResponse PRIVATE cxx_std_17)

    # These three existed as standalone sources but were not built by anything,
    # so the claims they substantiate were never actually verified on any commit.
    # BiquadVsSvfComparison in particular is cited in SvfBiquad.h as evidence
    # that RBJ and SVF are identical, so it needs to run.
    add_executable(DefaultEQ_BiquadVsSvf Tests/BiquadVsSvfComparison.cpp)
    target_compile_features(DefaultEQ_BiquadVsSvf PRIVATE cxx_std_17)

    add_executable(DefaultEQ_SvfTest Tests/SvfTest.cpp)
    target_compile_features(DefaultEQ_SvfTest PRIVATE cxx_std_17)

    add_executable(DefaultEQ_EngineIsolation Tests/EngineIsolationTest.cpp)
    target_compile_features(DefaultEQ_EngineIsolation PRIVATE cxx_std_17)

    add_executable(DefaultEQ_AuditTests Tests/AuditRegressionTest.cpp)
    target_compile_features(DefaultEQ_AuditTests PRIVATE cxx_std_17)
    find_package(Threads REQUIRED)
    target_link_libraries(DefaultEQ_AuditTests PRIVATE Threads::Threads)

    add_executable(DefaultEQ_LatencyProbe Tests/LatencyProbe.cpp)
    target_link_libraries(DefaultEQ_LatencyProbe PRIVATE juce::juce_dsp)
    target_compile_definitions(DefaultEQ_LatencyProbe PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)

    add_executable(DefaultEQ_DspIntegration Tests/DspIntegrationTest.cpp)
    target_link_libraries(DefaultEQ_DspIntegration PRIVATE DefaultEQ juce::juce_audio_utils juce::juce_dsp)
    target_compile_definitions(DefaultEQ_DspIntegration PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)

    # Manual processor benchmark. It is intentionally not a CTest gate: CPU
    # timings vary by machine, power state and host load. The executable keeps
    # source-buffer preparation outside the reported processor cost and prints
    # a matrix suitable for before/after performance work.
    add_executable(DefaultEQ_PerformanceBenchmark Tests/PerformanceBenchmark.cpp)
    target_link_libraries(DefaultEQ_PerformanceBenchmark PRIVATE
        DefaultEQ juce::juce_audio_utils juce::juce_dsp)
    target_compile_definitions(DefaultEQ_PerformanceBenchmark PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)

    # Phase-1 safety net uses one runner so JUCE's module sources are compiled
    # once rather than once per regression executable. CTest still exposes the
    # three gates independently for focused failures and reruns.
    add_executable(DefaultEQ_SafetyNet
        Tests/SafetyNetMain.cpp
        Tests/HostParameterRegressionTest.cpp
        Tests/AutomationFuzzTest.cpp
        Tests/MemoryRegressionTest.cpp
        Tests/DspEquivalenceTest.cpp
        Tests/EditorLayoutRegressionTest.cpp)
    target_link_libraries(DefaultEQ_SafetyNet PRIVATE
        DefaultEQ juce::juce_audio_utils juce::juce_dsp)
    target_compile_definitions(DefaultEQ_SafetyNet PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    if (WIN32)
        target_link_libraries(DefaultEQ_SafetyNet PRIVATE Psapi)
    endif()
    if (MSVC)
        # The processor deliberately owns large fixed DSP workspaces. Plugin
        # hosts instantiate it on the heap, while this integration runner uses
        # scoped instances and therefore needs more than Windows' 1 MiB default.
        target_link_options(DefaultEQ_DspIntegration PRIVATE "/STACK:33554432")
    elseif (APPLE)
        # The same scoped-instance test pattern needs an explicit stack reserve
        # in an unoptimised macOS build. Hosts still create the processor on the
        # heap; this affects only the integration-test executable.
        target_link_options(DefaultEQ_DspIntegration PRIVATE
            "LINKER:-stack_size,0x4000000")
        target_link_options(DefaultEQ_PerformanceBenchmark PRIVATE
            "LINKER:-stack_size,0x4000000")
    endif()

    add_test(NAME biquad COMMAND DefaultEQ_Tests)
    add_test(NAME shelf_nan_sweep COMMAND DefaultEQ_ShelfNaNSweep)
    add_test(NAME shelf_response COMMAND DefaultEQ_ShelfResponse)
    add_test(NAME biquad_vs_svf COMMAND DefaultEQ_BiquadVsSvf)
    add_test(NAME svf COMMAND DefaultEQ_SvfTest)
    add_test(NAME engine_isolation COMMAND DefaultEQ_EngineIsolation)
    add_test(NAME audit_regressions COMMAND DefaultEQ_AuditTests)
    add_test(NAME latency_probe COMMAND DefaultEQ_LatencyProbe)
    add_test(NAME dsp_integration COMMAND DefaultEQ_DspIntegration)
    add_test(NAME host_parameter_regression COMMAND DefaultEQ_SafetyNet host-parameters)
    add_test(NAME automation_fuzz COMMAND DefaultEQ_SafetyNet automation-fuzz)
    add_test(NAME memory_regression COMMAND DefaultEQ_SafetyNet memory)
    add_test(NAME dsp_equivalence COMMAND DefaultEQ_SafetyNet dsp-self)
    add_test(NAME editor_layout_regression COMMAND DefaultEQ_SafetyNet editor-layout)
    set_tests_properties(automation_fuzz PROPERTIES TIMEOUT 240)
    set_tests_properties(memory_regression PROPERTIES TIMEOUT 120)
endif()
