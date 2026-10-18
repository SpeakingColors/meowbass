// Plugin entry point.
#include "common/FxBase.h"
#include "bass/MeowBass.h"
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MeowBass(); }
