#include "MeowBass.h"

using namespace catfx;

MeowBass::MeowBass()
    : FxBase("Meowbass",
        { { "freq", "Bass range", " Hz", 30.f, 200.f, 80.f, 80.f, "Bass range: the frequency below which the bass is picked up to make harmonics. Set it near the lowest note you want to feel (kick or bass fundamental, often 50 to 100 Hz)." },
          { "in", "Input", " dB", -12.f, 12.f, 0.f, 0.f, "Input: level into the effect. Raise it for more harmonics from a quiet source." },
          { "orig", "Original bass", " dB", -24.f, 6.f, 0.f, 0.f, "Original bass: level of the untouched low end. Pull it down to free headroom and let the harmonics carry the bass." },
          { "harm", "Harmonics", " dB", -24.f, 12.f, -6.f, 0.f, "Harmonics: level of the added overtones that make small speakers suggest deep bass. Start low and raise until the bass feels bigger." },
          { "ratio", "Dyn ratio", ":1", 1.f, 10.f, 3.f, 0.f, "Dynamics ratio: evens out quiet and loud bass notes so the harmonics stay steady. 1:1 follows the level exactly, higher = more even." },
          { "resp", "Dyn response", " ms", 1.f, 200.f, 40.f, 40.f, "Dynamics response: how fast the evening-out reacts. Short = tight and punchy, long = smooth." },
          { "decay", "Even/Odd", "", 0.f, 1.f, 0.5f, 0.f, "Even/Odd: character of the harmonics. Even = warm and round (rectifier style), Odd = harder and more edgy (clipper style)." },
          { "out", "Output", " dB", -12.f, 12.f, 0.f, 0.f, "Output: final level. Match it with the bypassed level when comparing." } },
        { { "hfilt", "Harmonic filter", { "Smooth", "Wide", "Tight" }, 0, "Harmonic filter: Smooth is balanced, Wide lets more upper harmonics through (brighter), Tight keeps only the lowest ones (cleaner)." } }) {}

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

std::vector<catfx::PanelSpec> MeowBass::panels() const
{
    return { { "Frequency", { "freq", "orig", "harm" }, { "hfilt" }, 3.4f },
             { "Character", { "decay" }, {}, 1.3f },
             { "Dynamics", { "ratio", "resp" }, {}, 1.7f },
             { "Level", { "in", "out" }, {}, 1.5f } };
}

std::vector<FxBase::Preset> MeowBass::presets() const
{
    return {
        { "Init", {} },
        { "Small Speakers", { { "freq", 90.f }, { "orig", -6.f }, { "harm", 3.f }, { "hfilt", 1.f } } },
        { "Kick Weight", { { "freq", 60.f }, { "harm", -3.f }, { "decay", 0.3f }, { "ratio", 4.f }, { "resp", 20.f } } },
        { "Sub Clarity", { { "freq", 50.f }, { "orig", -3.f }, { "harm", -2.f }, { "decay", 0.2f }, { "hfilt", 2.f } } },
        { "Reese Body", { { "freq", 110.f }, { "harm", 0.f }, { "decay", 0.7f }, { "ratio", 3.f }, { "hfilt", 1.f } } },
        { "Gentle Warmth", { { "freq", 80.f }, { "harm", -10.f }, { "decay", 0.2f }, { "ratio", 2.f } } },
        { "Hardstyle Rumble", { { "freq", 70.f }, { "orig", -2.f }, { "harm", 2.f }, { "decay", 0.8f }, { "ratio", 5.f }, { "resp", 15.f } } } };
}

void MeowBass::paintPanel(int idx, juce::Graphics& g, juce::Rectangle<float> r)
{
    using namespace catfx::viz;
    if (idx == 0)       // frequency view: original bass (blue) and added harmonics (orange)
    {
        const float f0 = get("freq"), origDb = get("orig"), harmDb = get("harm"); const int mode = (int) get("hfilt");
        const float lpK = mode == 0 ? 4.f : mode == 1 ? 8.f : 3.f, hpK = mode == 2 ? 2.f : 1.4f;
        auto p = r.withTrimmedLeft(34.f).withTrimmedRight(8.f).withTrimmedTop(26.f).withTrimmedBottom(14.f);
        g.setColour(bg().brighter(0.04f)); g.fillRect(p);
        dbAxis(g, p, -36.f, 6.f, 6.f); freqAxis(g, p, 16.f, 1000.f);
        auto X = [&](float hz) { return freqToX(hz, p, 16.f, 1000.f); };
        auto Y = [&](float db) { return p.getBottom() - juce::jlimit(0.f, 1.f, (db + 36.f) / 42.f) * p.getHeight(); };
        juce::Path blue_, orange_;
        for (int i = 0; i <= 240; ++i)
        {
            const float hz = 16.f * std::pow(1000.f / 16.f, (float) i / 240.f), rr = hz / f0;
            const float lowMag = 1.f / (1.f + std::pow(rr, 4.f));                                      // LR4 low-pass
            const float hpMag = 1.f / std::sqrt(1.f + std::pow(hpK * f0 / hz, 4.f));
            const float lpMag = 1.f / std::sqrt(1.f + std::pow(hz / (lpK * f0), 4.f));
            const float bdb = 20.f * std::log10(std::max(lowMag, 1e-4f)) + origDb;
            const float odb = 20.f * std::log10(std::max(hpMag * lpMag, 1e-4f)) + harmDb;
            const auto pb = juce::Point<float>(X(hz), Y(bdb)), po = juce::Point<float>(X(hz), Y(odb));
            i ? blue_.lineTo(pb) : blue_.startNewSubPath(pb); i ? orange_.lineTo(po) : orange_.startNewSubPath(po);
        }
        g.setColour(blue()); g.strokePath(blue_, juce::PathStrokeType(2.f));
        g.setColour(accent()); g.strokePath(orange_, juce::PathStrokeType(2.f));
        g.setFont(juce::FontOptions(11.f));
        g.setColour(blue()); g.drawText("original bass " + juce::String(origDb, 1) + " dB", (int) r.getX() + 4, (int) r.getY() + 1, 150, 12, juce::Justification::left);
        g.setColour(accent()); g.drawText("harmonics " + juce::String(harmDb, 1) + " dB", (int) r.getX() + 4, (int) r.getY() + 1, (int) r.getWidth() - 8, 12, juce::Justification::right);
        const float lv = juce::Decibels::gainToDecibels(lowLevel.load(), -60.f), hv = juce::Decibels::gainToDecibels(harmLevel.load(), -60.f);
        g.setColour(dim()); g.drawText("live: bass " + juce::String(lv, 1) + " dB, harmonics " + juce::String(hv, 1) + " dB", p.toNearestInt().withTrimmedTop(2).withHeight(12), juce::Justification::centred);
    }
    else if (idx == 1)  // shape of the generated harmonic over one bass cycle
    {
        const float odd = get("decay");
        auto p = r.withTrimmedLeft(8.f).withTrimmedRight(8.f).withTrimmedTop(26.f).withTrimmedBottom(14.f);
        g.setColour(bg().brighter(0.04f)); g.fillRect(p);
        g.setColour(grid()); g.drawHorizontalLine((int) p.getCentreY(), p.getX(), p.getRight());
        juce::Path in, out; float mx = 1e-3f; std::vector<float> hs;
        for (int i = 0; i <= 120; ++i)
        {
            const float m = 0.8f * std::sin(juce::MathConstants<float>::twoPi * (float) i / 120.f);
            const float h = (1.f - odd) * (std::abs(m) - 0.509f) * 2.f + odd * std::tanh(4.f * m) * 0.5f;
            hs.push_back(h); mx = std::max(mx, std::abs(h));
        }
        for (int i = 0; i <= 120; ++i)
        {
            const float x = p.getX() + p.getWidth() * (float) i / 120.f;
            const float yi = p.getCentreY() - 0.8f * std::sin(juce::MathConstants<float>::twoPi * (float) i / 120.f) * p.getHeight() * 0.45f;
            const float yo = p.getCentreY() - hs[(size_t) i] / mx * p.getHeight() * 0.42f;
            i ? in.lineTo(x, yi) : in.startNewSubPath(x, yi); i ? out.lineTo(x, yo) : out.startNewSubPath(x, yo);
        }
        g.setColour(blue().withAlpha(0.8f)); g.strokePath(in, juce::PathStrokeType(1.5f));
        g.setColour(accent()); g.strokePath(out, juce::PathStrokeType(2.5f));
        g.setColour(dim()); g.setFont(juce::FontOptions(11.f));
        g.drawText("one bass cycle", (int) r.getX() + 4, (int) r.getY() + 1, (int) r.getWidth(), 12, juce::Justification::left);
        g.drawText("even " + juce::String((int) std::round((1.f - odd) * 100.f)) + "%", p.toNearestInt().withY((int) p.getBottom() + 1).withHeight(12), juce::Justification::left);
        g.drawText("odd " + juce::String((int) std::round(odd * 100.f)) + "%", p.toNearestInt().withY((int) p.getBottom() + 1).withHeight(12), juce::Justification::right);
    }
    else if (idx == 2)  // dynamics: harmonic gain against bass level
    {
        const float ratio = get("ratio"), resp = get("resp");
        auto p = r.withTrimmedLeft(34.f).withTrimmedRight(8.f).withTrimmedTop(26.f).withTrimmedBottom(14.f);
        g.setColour(bg().brighter(0.04f)); g.fillRect(p);
        dbAxis(g, p, -12.f, 18.f, 6.f);
        for (float v : { -48.f, -36.f, -24.f, -12.f, 0.f })
        { const float x = p.getX() + (v + 48.f) / 48.f * p.getWidth(); g.setColour(grid()); g.drawVerticalLine((int) x, p.getY(), p.getBottom()); g.setColour(dim()); g.setFont(juce::FontOptions(10.f)); g.drawText(juce::String((int) v), (int) x - 14, (int) p.getBottom() + 1, 28, 11, juce::Justification::centred); }
        auto gainDb = [&](float levelDb) { const float lin = juce::Decibels::decibelsToGain(levelDb); return 20.f * std::log10(juce::jlimit(0.f, 8.f, std::pow(std::max(lin, 0.003f) / 0.25f, 1.f / ratio - 1.f))); };
        auto Y = [&](float db) { return p.getBottom() - (juce::jlimit(-12.f, 18.f, db) + 12.f) / 30.f * p.getHeight(); };
        juce::Path c;
        for (int i = 0; i <= 96; ++i) { const float lv = -48.f + (float) i / 2.f, x = p.getX() + (float) i / 96.f * p.getWidth(); i ? c.lineTo(x, Y(gainDb(lv))) : c.startNewSubPath(x, Y(gainDb(lv))); }
        g.setColour(accent()); g.strokePath(c, juce::PathStrokeType(2.f));
        const float lv = juce::jlimit(-48.f, 0.f, juce::Decibels::gainToDecibels(lowLevel.load(), -60.f));
        g.setColour(juce::Colours::white); g.fillEllipse(p.getX() + (lv + 48.f) / 48.f * p.getWidth() - 3.f, Y(gainDb(lv)) - 3.f, 6.f, 6.f);
        g.setColour(dim()); g.setFont(juce::FontOptions(11.f));
        g.drawText("harmonic gain vs bass level", (int) r.getX() + 4, (int) r.getY() + 1, (int) r.getWidth(), 12, juce::Justification::left);
        g.drawText(juce::String(ratio, 1) + ":1  " + juce::String((int) resp) + " ms", p.toNearestInt().withTrimmedTop(2).withHeight(12).withTrimmedRight(4), juce::Justification::right);
    }
    else                // level: IN and OUT meters
    {
        auto area = r.reduced(6.f, 6.f);
        auto left = area.removeFromLeft(area.getWidth() * 0.5f);
        meter(g, left, inLevel.load(), "IN dB");
        meter(g, area, outLevel.load(), "OUT dB");
    }
}
