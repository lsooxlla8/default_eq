#include <cstdio>
#include <cstring>
#include <juce_gui_basics/juce_gui_basics.h>

int runHostParameterRegression();
int runAutomationFuzz();
int runMemoryRegression();
int runDspEquivalence(const char* mode, const char* path);
int runEditorLayoutRegression();

int main(int argc, char** argv)
{
#if JUCE_WINDOWS
    // Windows needs the JUCE GUI subsystem initialised before its software
    // renderer can produce meaningful offscreen editor snapshots. Linux stays
    // headless here because its CI test step intentionally has no X server.
    juce::ScopedJuceInitialiser_GUI guiInitialiser;
#endif

    if (argc < 2)
    {
        std::printf("usage: DefaultEQ_SafetyNet <host-parameters|automation-fuzz|memory|dsp-self|dsp-write|dsp-compare|editor-layout> [baseline]\n");
        return 2;
    }
    if (std::strcmp(argv[1], "host-parameters") == 0) return runHostParameterRegression();
    if (std::strcmp(argv[1], "automation-fuzz") == 0) return runAutomationFuzz();
    if (std::strcmp(argv[1], "memory") == 0) return runMemoryRegression();
    if (std::strcmp(argv[1], "dsp-self") == 0) return runDspEquivalence("self", nullptr);
    if (std::strcmp(argv[1], "dsp-write") == 0) return runDspEquivalence("write", argc > 2 ? argv[2] : nullptr);
    if (std::strcmp(argv[1], "dsp-compare") == 0) return runDspEquivalence("compare", argc > 2 ? argv[2] : nullptr);
    if (std::strcmp(argv[1], "editor-layout") == 0) return runEditorLayoutRegression();
    std::printf("unknown safety-net test: %s\n", argv[1]);
    return 2;
}
