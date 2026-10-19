#pragma once
#include "common/FxBase.h"
#include "common/Viz.h"

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
    std::vector<catfx::PanelSpec> panels() const override;
    void paintPanel(int index, juce::Graphics&, juce::Rectangle<float>) override;
    std::vector<Preset> presets() const override;
    juce::StringArray randomExclude() const override { return { "in", "out" }; }
private:
    // Plain biquad (RBJ, Q 0.7071): coefficients are set with arithmetic only, so nothing is allocated on the audio thread.
    struct Bq
    {
        float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f, z1 = 0.f, z2 = 0.f;
        void set(bool high, double fs, float f)
        {
            const double w = 2.0 * juce::MathConstants<double>::pi * juce::jlimit(5.0, 0.45 * fs, (double) f) / fs, c = std::cos(w), al = std::sin(w) / (2.0 * 0.7071), a0 = 1.0 + al;
            const double nb0 = high ? (1.0 + c) * 0.5 : (1.0 - c) * 0.5, nb1 = high ? -(1.0 + c) : (1.0 - c);
            b0 = (float) (nb0 / a0); b1 = (float) (nb1 / a0); b2 = (float) (nb0 / a0); a1 = (float) (-2.0 * c / a0); a2 = (float) ((1.0 - al) / a0);
        }
        void reset() { z1 = z2 = 0.f; }
        float tick(float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    };
    juce::dsp::LinkwitzRileyFilter<float> split[2];
    Bq hpE, lpE, hpO, lpO;                       // separate even and odd branch filters
    float env = 0.f, dcState = 0.f;
    juce::SmoothedValue<float> gIn, gOrig, gHarm, gOut, sDyn, sDrive;   // 20 ms or less: no zipper noise, no audible lag
    std::atomic<float> lowLevel { 0.f }, harmLevel { 0.f };
};
