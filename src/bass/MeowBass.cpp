#include "MeowBass.h"

using namespace catfx;

MeowBass::MeowBass()
    : FxBase("Meowbass",
        { { "freq", "Bass range", " Hz", 30.f, 200.f, 80.f, 80.f },
          { "in", "Input", " dB", -12.f, 12.f, 0.f },
          { "orig", "Original bass", " dB", -24.f, 6.f, 0.f },
          { "harm", "Harmonics", " dB", -24.f, 12.f, -6.f },
          { "ratio", "Dyn ratio", ":1", 1.f, 10.f, 3.f },
          { "resp", "Dyn response", " ms", 1.f, 200.f, 40.f, 40.f },
          { "decay", "Even/Odd", "", 0.f, 1.f, 0.5f },
          { "out", "Output", " dB", -12.f, 12.f, 0.f } },
        { { "hfilt", "Harmonic filter", { "Smooth", "Wide", "Tight" }, 0 } }) {}

void MeowBass::prepare(double sr, int bs)
{
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 1 };
    for (auto& s : split) { s.prepare(spec); s.reset(); }
    for (auto& f : hp) f.reset();
    for (auto& f : lp) f.reset();
    env = 0.f; dcState = 0.f;
}

void MeowBass::process(juce::AudioBuffer<float>& buf, const juce::AudioBuffer<float>*)
{
    const int n = buf.getNumSamples(), nch = juce::jmin(2, buf.getNumChannels());
    const float f0 = get("freq"), inG = juce::Decibels::decibelsToGain(get("in")), outG = juce::Decibels::decibelsToGain(get("out"));
    const float origG = juce::Decibels::decibelsToGain(get("orig")), harmG = juce::Decibels::decibelsToGain(get("harm"));
    const float ratio = get("ratio"), resp = get("resp"), odd = get("decay"); const int mode = (int) get("hfilt");
    const float lpHz = mode == 0 ? 4.f * f0 : mode == 1 ? 8.f * f0 : 3.f * f0;
    const float hpHz = mode == 2 ? 2.f * f0 : 1.4f * f0;
    for (int c = 0; c < nch; ++c)
    {
        split[c].setCutoffFrequency(f0);
        hp[c].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(fs, hpHz, 0.7071f);
        lp[c].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(fs, juce::jmin(lpHz, 0.45f * (float) fs), 0.7071f);
    }
    const float aA = std::exp(-1.f / (0.001f * resp * 0.25f * (float) fs)), aR = std::exp(-1.f / (0.001f * resp * (float) fs));
    float lowPk = 0.f, harmPk = 0.f;
    for (int i = 0; i < n; ++i)
    {
        float lo[2] {}, hi[2] {};
        for (int c = 0; c < nch; ++c) split[c].processSample(0, buf.getReadPointer(c)[i] * inG, lo[c], hi[c]);
        const float m = nch > 1 ? 0.5f * (lo[0] + lo[1]) : lo[0];      // harmonics are made from the mono low band
        const float a = std::abs(m) > env ? aA : aR; env = a * env + (1.f - a) * std::abs(m);
        const float norm = juce::jlimit(0.f, 8.f, std::pow(std::max(env, 0.003f) / 0.25f, 1.f / ratio - 1.f)); // evens out quiet and loud notes
        float even = std::abs(m); dcState += 0.002f * (even - dcState); even -= dcState;
        const float oddS = std::tanh(4.f * m) * 0.5f;
        float h = ((1.f - odd) * even * 2.f + odd * oddS) * norm * 0.25f;
        h = hp[0].processSample(h);
        h = lp[0].processSample(h);
        h *= harmG * 4.f;
        lowPk = std::max(lowPk, std::abs(m)); harmPk = std::max(harmPk, std::abs(h));
        for (int c = 0; c < nch; ++c) buf.getWritePointer(c)[i] = (hi[c] + lo[c] * origG + h) * outG;
    }
    lowLevel = lowPk; harmLevel = harmPk;
}

void MeowBass::paintViz(juce::Graphics& g, juce::Rectangle<float> r)
{
    r = r.reduced(4.f, 20.f); r.removeFromRight(24.f);
    const float f0 = get("freq"), origDb = get("orig"), harmDb = get("harm"); const int mode = (int) get("hfilt");
    const float lpK = mode == 0 ? 4.f : mode == 1 ? 8.f : 3.f, hpK = mode == 2 ? 2.f : 1.4f;
    auto X = [&](float hz) { return r.getX() + r.getWidth() * std::log(hz / 16.f) / std::log(1000.f / 16.f); };
    auto Y = [&](float db) { return r.getBottom() - juce::jlimit(0.f, 1.f, (db + 36.f) / 42.f) * r.getHeight(); };
    g.setColour(juce::Colour(0xff223030)); for (float hz : { 32.f, 64.f, 128.f, 256.f, 512.f }) g.drawVerticalLine((int) X(hz), r.getY(), r.getBottom());
    g.setColour(juce::Colour(0xff6b8279)); g.setFont(juce::FontOptions(10.f));
    for (float hz : { 32.f, 64.f, 128.f, 256.f, 512.f }) g.drawText(juce::String((int) hz), (int) X(hz) + 2, (int) r.getBottom() - 12, 30, 12, juce::Justification::left);
    juce::Path blue, orange;
    for (int i = 0; i <= 240; ++i)
    {
        const float hz = 16.f * std::pow(1000.f / 16.f, (float) i / 240.f), rr = hz / f0;
        const float lowMag = 1.f / (1.f + std::pow(rr, 4.f));                                      // LR4 low-pass
        const float hpMag = 1.f / std::sqrt(1.f + std::pow(hpK * f0 / hz, 4.f));
        const float lpMag = 1.f / std::sqrt(1.f + std::pow(hz / (lpK * f0), 4.f));
        const float bdb = 20.f * std::log10(std::max(lowMag, 1e-4f)) + origDb;
        const float odb = 20.f * std::log10(std::max(hpMag * lpMag, 1e-4f)) + harmDb;
        const auto pb = juce::Point<float>(X(hz), Y(bdb)), po = juce::Point<float>(X(hz), Y(odb));
        i ? blue.lineTo(pb) : blue.startNewSubPath(pb); i ? orange.lineTo(po) : orange.startNewSubPath(po);
    }
    g.setColour(juce::Colour(0xff5aa8ff)); g.strokePath(blue, juce::PathStrokeType(2.f));
    g.setColour(juce::Colour(0xfff2a23c)); g.strokePath(orange, juce::PathStrokeType(2.f));
    // live level bars
    const float lv = juce::jlimit(0.f, 1.f, juce::Decibels::gainToDecibels(lowLevel.load(), -60.f) / 60.f + 1.f);
    const float hv = juce::jlimit(0.f, 1.f, juce::Decibels::gainToDecibels(harmLevel.load(), -60.f) / 60.f + 1.f);
    g.setColour(juce::Colour(0xff5aa8ff)); g.fillRect(r.getRight() + 4.f, r.getBottom() - lv * r.getHeight(), 8.f, lv * r.getHeight());
    g.setColour(juce::Colour(0xfff2a23c)); g.fillRect(r.getRight() + 14.f, r.getBottom() - hv * r.getHeight(), 8.f, hv * r.getHeight());
}
