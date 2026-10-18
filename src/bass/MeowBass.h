#pragma once
#include "common/FxBase.h"

// Meowbass: psychoacoustic bass enhancer. The low band is split off with a phase-aligned crossover,
// harmonics are generated from it with a rectifier / soft clipper blend, filtered, level-normalised
// by a dynamics stage and added back, so small speakers suggest bass they cannot reproduce.
// Original design from the general idea of "missing fundamental" bass enhancement.
class MeowBass : public catfx::FxBase
{
public:
    MeowBass();
    void prepare(double sr, int maxBlock) override;
    void process(juce::AudioBuffer<float>&, const juce::AudioBuffer<float>*) override;
    void paintViz(juce::Graphics&, juce::Rectangle<float>) override;
    juce::String vizCaption() const override { return "Original bass (blue) and added harmonics (orange)"; }
private:
    juce::dsp::LinkwitzRileyFilter<float> split[2];
    juce::dsp::IIR::Filter<float> hp[2], lp[2];
    float env = 0.f, dcState = 0.f;
    std::atomic<float> lowLevel { 0.f }, harmLevel { 0.f };
};
