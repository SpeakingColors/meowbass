#include "MeowBass.h"

using namespace catfx;

MeowBass::MeowBass()
    : FxBase("Meowbass",
        { { "freq", "Bass range", " Hz", 30.f, 200.f, 80.f, 80.f, "Bass range: the frequency below which the bass is picked up to make harmonics. Set it near the lowest note you want to feel (kick or bass fundamental, often 50 to 100 Hz)." },
          { "in", "Input", " dB", -12.f, 12.f, 0.f, 0.f, "Input: level into the effect. Raise it for more harmonics from a quiet source." },
          { "orig", "Original bass", " dB", -48.f, 6.f, 0.f, 0.f, "Original bass: level of the untouched low end. Pull it down to free headroom and let the harmonics carry the bass." },
          { "harm", "Harmonics", " dB", -36.f, 18.f, -12.f, 0.f, "Harmonics: level of the added overtones that make small speakers suggest deep bass. Start low and raise until the bass feels bigger. Range -36 to +18 dB: from subtle to obvious." },
          { "dyn", "Dynamics", " %", -100.f, 100.f, -40.f, 0.f, "Dynamics: how the harmonics follow the bass level. 0 = off (neutral). Negative = evens out quiet and loud notes (-100 = most even, quiet notes are boosted). Positive = the harmonics follow the level more and more, so loud notes get far more harmonics than quiet ones (+100 = drastic). Tailor it from subtle to extreme either way." },
          { "resp", "Dyn response", " ms", 1.f, 1000.f, 40.f, 40.f, "Dynamics response: how fast the harmonics follow the bass level (works together with Dynamics; at Dynamics 0 it does nothing). Short = every hit snaps the harmonics (punchy, tight); long = the harmonics glide over whole notes (smooth, slow swell). With Dynamics up, the difference is big." },
          { "decay", "Even/Odd", "", 0.f, 1.f, 0.5f, 0.f, "Even/Odd: character of the harmonics. Even = warm and round (rectifier style), Odd = harder and more edgy (clipper style)." },
          { "hdrive", "Drive", " %", 0.f, 100.f, 30.f, 0.f, "Harmonic drive: how hard the bass is pushed into the harmonic generator. Low = clean and subtle, high = thick, obvious and colourful. Works together with Even/Odd." },
          { "out", "Output", " dB", -12.f, 12.f, 0.f, 0.f, "Output: final level. Match it with the bypassed level when comparing." } },
        { { "hfilt", "Harmonic filter", { "Smooth", "Wide", "Tight" }, 0, "Harmonic filter: Smooth is balanced, Wide lets more upper harmonics through (brighter), Tight keeps only the lowest ones (cleaner)." } }) {}

void MeowBass::prepare(double sr, int bs)
{
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 1 };
    for (auto& s : split) { s.prepare(spec); s.reset(); }
    hpE.reset(); lpE.reset(); hpO.reset(); lpO.reset();
    env = 0.f; dcState = 0.f;
    auto ini = [&](juce::SmoothedValue<float>& v, float val, double sec) { v.reset(sr, sec); v.setCurrentAndTargetValue(val); };
    ini(gIn, juce::Decibels::decibelsToGain(get("in")), 0.02); ini(gOrig, juce::Decibels::decibelsToGain(get("orig")), 0.02);
    ini(gHarm, juce::Decibels::decibelsToGain(get("harm")), 0.02); ini(gOut, juce::Decibels::decibelsToGain(get("out")), 0.02);
    ini(sDyn, get("dyn"), 0.02); ini(sDrive, get("hdrive"), 0.01);
}

void MeowBass::process(juce::AudioBuffer<float>& buf, const juce::AudioBuffer<float>*)
{
    const int n = buf.getNumSamples(), nch = juce::jmin(2, buf.getNumChannels());
    const float f0 = get("freq");
    gIn.setTargetValue(juce::Decibels::decibelsToGain(get("in"))); gOut.setTargetValue(juce::Decibels::decibelsToGain(get("out")));
    gOrig.setTargetValue(juce::Decibels::decibelsToGain(get("orig"))); gHarm.setTargetValue(juce::Decibels::decibelsToGain(get("harm")));
    sDyn.setTargetValue(get("dyn")); sDrive.setTargetValue(get("hdrive"));
    const float resp = get("resp"), odd = get("decay"); const int mode = (int) get("hfilt");
    const float lpHz = mode == 0 ? 4.f * f0 : mode == 1 ? 8.f * f0 : 3.f * f0;
    const float hpOHz = mode == 2 ? 2.f * f0 : 1.4f * f0, hpEHz = (mode == 2 ? 1.0f : mode == 1 ? 0.7f : 0.8f) * f0;
    for (int c = 0; c < nch; ++c) split[c].setCutoffFrequency(f0);
    hpO.set(true, fs, hpOHz); hpE.set(true, fs, hpEHz); lpO.set(false, fs, lpHz); lpE.set(false, fs, lpHz);
    const float aA = std::exp(-1.f / (0.001f * std::max(0.2f, resp * 0.04f) * (float) fs)), aR = std::exp(-1.f / (0.001f * resp * (float) fs));
    float lowPk = 0.f, harmPk = 0.f;
    for (int i = 0; i < n; ++i)
    {
        const float inG = gIn.getNextValue(), origG = gOrig.getNextValue(), harmG = gHarm.getNextValue(), outG = gOut.getNextValue();
        const float dyn = sDyn.getNextValue(), gD = 1.f + sDrive.getNextValue() * 0.01f * 8.f;
        float lo[2] {}, hi[2] {};
        for (int c = 0; c < nch; ++c) split[c].processSample(0, buf.getReadPointer(c)[i] * inG, lo[c], hi[c]);
        const float m = nch > 1 ? 0.5f * (lo[0] + lo[1]) : lo[0];      // harmonics are made from the mono low band
        const float a = std::abs(m) > env ? aA : aR; env = a * env + (1.f - a) * std::abs(m);
        // Dynamics law (-100..+100 %): below 0 evens out, 0 is off (neutral), above 0 expands. Applied before the shaper (exponent 1) and after it (0.5 expand, 1 even out).
        const float dn = juce::jlimit(-100.f, 100.f, dyn) * 0.01f, ex = dn >= 0.f ? dn * 3.f : dn * 1.5f;
        const float lg = std::log(std::max(env, 0.003f) / (ex > 0.f ? 0.3f : 0.25f));
        const float pre = juce::jlimit(0.02f, 6.f, std::exp(ex * lg)), post = juce::jlimit(0.f, ex > 0.f ? 4.f : 5.f, std::exp(ex * (ex < 0.f ? 1.f : 0.5f) * lg));
        float ev = std::abs(m * gD * pre); dcState += 0.002f * (ev - dcState); ev = std::tanh((ev - dcState) * 1.2f);
        const float od = std::tanh(4.f * gD * m * pre) * 0.5f;
        float h = (1.f - odd) * lpE.tick(hpE.tick(ev * 2.f * post * 0.375f)) + odd * lpO.tick(hpO.tick(od * post * 1.5f));
        h *= harmG * 4.f * 0.5f;
        lowPk = std::max(lowPk, std::abs(m)); harmPk = std::max(harmPk, std::abs(h));
        for (int c = 0; c < nch; ++c) buf.getWritePointer(c)[i] = (hi[c] + lo[c] * origG + h) * outG;
    }
    lowLevel = lowPk; harmLevel = harmPk;
}

std::vector<catfx::PanelSpec> MeowBass::panels() const
{
    return { { "Frequency", { "freq", "orig", "harm" }, { "hfilt" }, 3.4f },
             { "Character", { "decay", "hdrive" }, {}, 1.3f },
             { "Dynamics", { "dyn", "resp" }, {}, 1.7f },
             { "Level", { "in", "out" }, {}, 1.5f } };
}

std::vector<FxBase::Preset> MeowBass::presets() const
{
    return {
        { "Init", {} },
        { "Small Speakers", { { "freq", 90.f }, { "orig", -6.f }, { "harm", -3.f }, { "hfilt", 1.f }, { "hdrive", 45.f } } },
        { "Kick Weight", { { "freq", 60.f }, { "harm", -9.f }, { "decay", 0.3f }, { "dyn", -37.f }, { "resp", 20.f } } },
        { "Sub Clarity", { { "freq", 50.f }, { "orig", -3.f }, { "harm", -8.f }, { "decay", 0.2f }, { "hfilt", 2.f } } },
        { "Reese Body", { { "freq", 110.f }, { "harm", -6.f }, { "decay", 0.7f }, { "dyn", -42.f }, { "hfilt", 1.f }, { "hdrive", 50.f } } },
        { "Gentle Warmth", { { "freq", 80.f }, { "harm", -16.f }, { "decay", 0.2f }, { "dyn", -27.f }, { "hdrive", 15.f } } },
        { "Hardstyle Rumble", { { "freq", 70.f }, { "orig", -2.f }, { "harm", -4.f }, { "decay", 0.8f }, { "dyn", -53.f }, { "resp", 15.f }, { "hdrive", 70.f } } } };
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
        dbAxis(g, p, -48.f, 18.f, 12.f); freqAxis(g, p, 16.f, 1000.f);
        auto X = [&](float hz) { return freqToX(hz, p, 16.f, 1000.f); };
        auto Y = [&](float db) { return p.getBottom() - juce::jlimit(0.f, 1.f, (db + 48.f) / 66.f) * p.getHeight(); };
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
        { auto line = p.toNearestInt().withTrimmedTop(2).withHeight(12);
          g.setColour(blue()); g.drawText("live bass " + juce::String(lv, 1) + " dB", line.removeFromLeft(line.getWidth() / 2), juce::Justification::right);
          g.setColour(accent()); g.drawText("  harmonics " + juce::String(hv, 1) + " dB", line, juce::Justification::left); }
    }
    else if (idx == 1)  // shape of the generated harmonic over one bass cycle
    {
        const float odd = get("decay"), drv = get("hdrive");
        auto p = r.withTrimmedLeft(8.f).withTrimmedRight(8.f).withTrimmedTop(26.f).withTrimmedBottom(14.f);
        g.setColour(bg().brighter(0.04f)); g.fillRect(p);
        g.setColour(grid()); g.drawHorizontalLine((int) p.getCentreY(), p.getX(), p.getRight());
        juce::Path in, out; float mx = 1e-3f; std::vector<float> hs;
        for (int i = 0; i <= 120; ++i)
        {
            const float m = 0.8f * std::sin(juce::MathConstants<float>::twoPi * (float) i / 120.f);
            const float h = (1.f - odd) * (std::abs(m) - 0.509f) * 2.f + odd * std::tanh((1.f + drv * 0.08f) * m) * 0.5f;
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
        g.setFont(juce::FontOptions(11.f));
        g.setColour(blue()); g.drawText("bass", (int) r.getX() + 4, (int) r.getY() + 1, 40, 12, juce::Justification::left);
        g.setColour(accent()); g.drawText("harmonics", (int) r.getX() + 4, (int) r.getY() + 1, (int) r.getWidth() - 8, 12, juce::Justification::right);
        g.setColour(accent()); g.drawText("even " + juce::String((int) std::round((1.f - odd) * 100.f)) + "%", p.toNearestInt().withY((int) p.getBottom() + 1).withHeight(12), juce::Justification::left);
        g.drawText("odd " + juce::String((int) std::round(odd * 100.f)) + "%", p.toNearestInt().withY((int) p.getBottom() + 1).withHeight(12), juce::Justification::right);
    }
    else if (idx == 2)  // dynamics: harmonic gain against bass level
    {
        const float dyn = get("dyn"), resp = get("resp");
        auto p = r.withTrimmedLeft(34.f).withTrimmedRight(8.f).withTrimmedTop(26.f).withTrimmedBottom(14.f);
        g.setColour(bg().brighter(0.04f)); g.fillRect(p);
        dbAxis(g, p, -48.f, 0.f, 12.f);
        for (float v : { -48.f, -36.f, -24.f, -12.f, 0.f })
        { const float x = p.getX() + (v + 48.f) / 48.f * p.getWidth(); g.setColour(grid()); g.drawVerticalLine((int) x, p.getY(), p.getBottom()); g.setColour(green()); g.setFont(juce::FontOptions(10.f)); g.drawText(juce::String((int) v), (int) x - 14, (int) p.getBottom() + 1, 28, 11, juce::Justification::centred); }
        const float dn = juce::jlimit(-100.f, 100.f, dyn) * 0.01f, ex = dn >= 0.f ? dn * 3.f : dn * 1.5f;
        auto gainDb = [&](float levelDb) { const float lin = juce::Decibels::decibelsToGain(levelDb); return 20.f * std::log10(juce::jlimit(1e-3f, ex > 0.f ? 24.f : 30.f, std::pow(std::max(lin, 0.003f) / (ex > 0.f ? 0.3f : 0.25f), ex * (ex >= 0.f ? 1.5f : 2.f)))); };
        auto Y = [&](float db) { return p.getBottom() - (juce::jlimit(-48.f, 0.f, db) + 48.f) / 48.f * p.getHeight(); };   // output level axis
        auto outDb = [&](float levelDb) { return levelDb + gainDb(levelDb); };   // transfer curve: harmonic level out against bass level in
        g.setColour(dim().withAlpha(0.6f)); g.drawLine(p.getX(), p.getBottom(), p.getRight(), p.getY(), 1.f);   // 1:1 line
        juce::Path c;
        for (int i = 0; i <= 96; ++i) { const float lv = -48.f + (float) i / 2.f, x = p.getX() + (float) i / 96.f * p.getWidth(); i ? c.lineTo(x, Y(outDb(lv))) : c.startNewSubPath(x, Y(outDb(lv))); }
        g.setColour(accent()); g.strokePath(c, juce::PathStrokeType(2.f));
        const float lv = juce::jlimit(-48.f, 0.f, juce::Decibels::gainToDecibels(lowLevel.load(), -60.f));
        g.setColour(juce::Colours::white); g.fillEllipse(p.getX() + (lv + 48.f) / 48.f * p.getWidth() - 3.f, Y(outDb(lv)) - 3.f, 6.f, 6.f);
        g.setColour(green()); g.setFont(juce::FontOptions(11.f));
        g.drawText("harmonics out vs bass in", (int) r.getX() + 4, (int) r.getY() + 1, (int) r.getWidth(), 12, juce::Justification::left);
        g.setColour(accent()); g.drawText(juce::String("dyn ") + (dyn > 0.f ? "+" : "") + juce::String((int) std::round(dyn)) + (dyn == 0.f ? " (off)" : "") + "  " + juce::String((int) resp) + " ms", p.toNearestInt().withTrimmedTop(2).withHeight(12).withTrimmedRight(4), juce::Justification::right);
    }
    else                // level: IN and OUT meters
    {
        auto area = r.reduced(6.f, 6.f);
        auto left = area.removeFromLeft(area.getWidth() * 0.5f);
        meter(g, left, inLevel.load(), "IN dB");
        meter(g, area, outLevel.load(), "OUT dB");
    }
}
