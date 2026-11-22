#include "FXRack.h"

using SVF = juce::dsp::StateVariableTPTFilterType;

const char* FXRack::name (int e)
{
    static const char* n[] = { "Distortion", "Underwater", "Lo-Fi", "Telephone", "Vinyl", "Echo", "Heaven Verb" };
    return e >= 0 && e < NumEffects ? n[e] : "";
}

const char* FXRack::id (int e)
{
    static const char* n[] = { "dist", "water", "lofi", "phone", "vinyl", "echo", "verb" };
    return e >= 0 && e < NumEffects ? n[e] : "";
}

const char* FXRack::blurb (int e)
{
    static const char* n[] = { "Grit & crunch", "Muffled + wobbly", "Crushed bits", "Thin radio voice",
                               "Crackle & dust", "Synced dotted 8ths", "Huge angelic space" };
    return e >= 0 && e < NumEffects ? n[e] : "";
}

void FXRack::prepare (double sampleRate, int maxBlock, int numChannels)
{
    sr = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, maxBlock), (juce::uint32) channels };

    for (auto* f : { &distTone, &phoneLp, &waterLp, &vinylLp }) { f->prepare (spec); f->setType (SVF::lowpass); }
    phoneHp.prepare (spec);
    phoneHp.setType (SVF::highpass);

    waterChorus.prepare (spec);
    waterChorus.setRate (0.35f);
    waterChorus.setCentreDelay (8.0f);
    waterChorus.setFeedback (0.15f);

    echo.prepare (spec);
    echo.setMaximumDelayInSamples ((int) (sampleRate * 2.5));

    verb.setSampleRate (sampleRate);
    scratch.setSize (channels, juce::jmax (1, maxBlock));
    reset();
}

void FXRack::reset()
{
    for (auto* f : { &distTone, &phoneHp, &phoneLp, &waterLp, &vinylLp }) f->reset();
    waterChorus.reset();
    echo.reset();
    verb.reset();
    lofiHold[0] = lofiHold[1] = 0.0f;
    lofiCounter[0] = lofiCounter[1] = 0;
    echoFb[0] = echoFb[1] = 0.0f;
    crackleEnv = 0.0f;
}

void FXRack::process (juce::AudioBuffer<float>& buffer, const Settings& s)
{
    if (s.on[Distortion]) processDistortion (buffer, s.amount[Distortion]);
    if (s.on[LoFi])       processLoFi (buffer, s.amount[LoFi]);
    if (s.on[Telephone])  processTelephone (buffer, s.amount[Telephone]);
    if (s.on[Underwater]) processUnderwater (buffer, s.amount[Underwater]);
    if (s.on[Vinyl])      processVinyl (buffer, s.amount[Vinyl]);
    if (s.on[Echo])       processEcho (buffer, s.amount[Echo], s.bpm);
    if (s.on[HeavenVerb]) processVerb (buffer, s.amount[HeavenVerb]);
}

void FXRack::processDistortion (juce::AudioBuffer<float>& b, float amt)
{
    const float drive = 1.0f + amt * 24.0f;
    const float comp = 0.5f / std::tanh (drive * 0.5f); // keeps loudness roughly steady as drive goes up
    const float wet = juce::jmin (1.0f, 0.4f + amt);
    distTone.setCutoffFrequency (12000.0f - amt * 6500.0f);
    const int nCh = juce::jmin (channels, b.getNumChannels());
    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* d = b.getWritePointer (ch);
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float x = d[i];
            const float y = distTone.processSample (ch, std::tanh (drive * x) * comp);
            d[i] = x * (1.0f - wet) + y * wet;
        }
    }
}

void FXRack::processLoFi (juce::AudioBuffer<float>& b, float amt)
{
    const float levels = std::pow (2.0f, 16.0f - amt * 12.0f) * 0.5f; // 16 bit -> 4 bit
    const int hold = 1 + (int) (amt * 14.0f);                          // sample-rate reduction
    const int nCh = juce::jmin (channels, b.getNumChannels());
    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* d = b.getWritePointer (ch);
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            if (++lofiCounter[ch] >= hold)
            {
                lofiCounter[ch] = 0;
                lofiHold[ch] = std::round (d[i] * levels) / levels;
            }
            d[i] = lofiHold[ch];
        }
    }
}

void FXRack::processTelephone (juce::AudioBuffer<float>& b, float amt)
{
    phoneHp.setCutoffFrequency (300.0f + amt * 500.0f);
    phoneLp.setCutoffFrequency (3400.0f - amt * 1400.0f);
    phoneHp.setResonance (1.1f);
    phoneLp.setResonance (1.1f);
    const float wet = 0.5f + 0.5f * amt;
    const float drive = 1.0f + amt * 3.0f;
    const int nCh = juce::jmin (channels, b.getNumChannels());
    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* d = b.getWritePointer (ch);
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float x = d[i];
            const float y = std::tanh (phoneLp.processSample (ch, phoneHp.processSample (ch, x)) * drive) * 1.2f;
            d[i] = x * (1.0f - wet) + y * wet;
        }
    }
}

void FXRack::processUnderwater (juce::AudioBuffer<float>& b, float amt)
{
    const int n = b.getNumSamples();
    waterLfo += 2.0 * juce::MathConstants<double>::pi * 0.22 * n / sr;
    if (waterLfo > juce::MathConstants<double>::twoPi) waterLfo -= juce::MathConstants<double>::twoPi;
    const float base = 2600.0f * std::pow (1.0f - amt, 1.5f) + 220.0f;
    waterLp.setCutoffFrequency (base * (1.0f + 0.22f * (float) std::sin (waterLfo)));
    waterLp.setResonance (0.9f + amt * 1.4f);

    const int nCh = juce::jmin (channels, b.getNumChannels());
    const float makeup = 1.0f + amt * 0.25f;
    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* d = b.getWritePointer (ch);
        for (int i = 0; i < n; ++i) d[i] = waterLp.processSample (ch, d[i]) * makeup;
    }

    waterChorus.setDepth (0.3f + 0.5f * amt);
    waterChorus.setMix (0.25f + 0.35f * amt);
    juce::dsp::AudioBlock<float> block (b.getArrayOfWritePointers(), (size_t) nCh, (size_t) n);
    waterChorus.process (juce::dsp::ProcessContextReplacing<float> (block));
}

void FXRack::processVinyl (juce::AudioBuffer<float>& b, float amt)
{
    vinylLp.setCutoffFrequency (15000.0f - amt * 9000.0f);
    const int n = b.getNumSamples();
    const int nCh = juce::jmin (channels, b.getNumChannels());
    for (int i = 0; i < n; ++i)
    {
        if (random.nextFloat() < amt * 0.0005f)
        {
            crackleEnv = 0.15f + random.nextFloat() * 0.5f * amt;
            crackleSign = random.nextBool() ? 1.0f : -1.0f;
        }
        const float crackle = crackleSign * crackleEnv;
        crackleEnv *= 0.55f;
        crackleSign = -crackleSign * 0.9f;
        const float hiss = (random.nextFloat() * 2.0f - 1.0f) * amt * 0.005f;
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* d = b.getWritePointer (ch);
            d[i] = vinylLp.processSample (ch, d[i]) + crackle + hiss;
        }
    }
}

void FXRack::processEcho (juce::AudioBuffer<float>& b, float amt, double bpm)
{
    const double beat = 60.0 / juce::jlimit (40.0, 240.0, bpm);
    const float delayL = (float) juce::jlimit (1.0, sr * 2.4, beat * 0.75 * sr); // dotted 8th
    const float delayR = (float) juce::jlimit (1.0, sr * 2.4, beat * 0.75 * sr * 1.06);
    const float fb = 0.2f + amt * 0.55f;
    const float wet = amt * 0.6f;
    const int nCh = juce::jmin (channels, b.getNumChannels());
    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* d = b.getWritePointer (ch);
        const float dl = ch == 0 ? delayL : delayR;
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float delayed = echo.popSample (ch, dl, true);
            echoFb[ch] = echoFb[ch] * 0.6f + delayed * 0.4f; // darken repeats
            echo.pushSample (ch, d[i] + echoFb[ch] * fb);
            d[i] += delayed * wet;
        }
    }
}

void FXRack::processVerb (juce::AudioBuffer<float>& b, float amt)
{
    juce::Reverb::Parameters p;
    p.roomSize = 0.55f + 0.43f * amt;
    p.damping = 0.3f;
    p.wetLevel = 0.05f + 0.17f * amt; // juce::Reverb's wet path is hot, so keep this modest
    p.dryLevel = 0.5f * (1.0f - 0.35f * amt); // juce::Reverb applies 2x to the dry signal
    p.width = 1.0f;
    p.freezeMode = 0.0f;
    verb.setParameters (p);
    if (channels >= 2 && b.getNumChannels() >= 2)
        verb.processStereo (b.getWritePointer (0), b.getWritePointer (1), b.getNumSamples());
    else
        verb.processMono (b.getWritePointer (0), b.getNumSamples());
}
