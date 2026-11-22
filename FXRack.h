#pragma once
#include <juce_dsp/juce_dsp.h>

// The "FX Heaven" rack: seven creative effects you can stack on the whole beat.
class FXRack
{
public:
    enum Effect { Distortion = 0, Underwater, LoFi, Telephone, Vinyl, Echo, HeavenVerb, NumEffects };

    struct Settings
    {
        bool on[NumEffects] {};
        float amount[NumEffects] {};
        double bpm = 120.0;
    };

    static const char* name (int effect);
    static const char* id (int effect);
    static const char* blurb (int effect);

    void prepare (double sampleRate, int maxBlock, int numChannels);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, const Settings& s);

private:
    void processDistortion (juce::AudioBuffer<float>&, float amt);
    void processLoFi (juce::AudioBuffer<float>&, float amt);
    void processTelephone (juce::AudioBuffer<float>&, float amt);
    void processUnderwater (juce::AudioBuffer<float>&, float amt);
    void processVinyl (juce::AudioBuffer<float>&, float amt);
    void processEcho (juce::AudioBuffer<float>&, float amt, double bpm);
    void processVerb (juce::AudioBuffer<float>&, float amt);

    double sr = 44100.0;
    int channels = 2;

    juce::dsp::StateVariableTPTFilter<float> distTone, phoneHp, phoneLp, waterLp, vinylLp;
    juce::dsp::Chorus<float> waterChorus;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> echo { 192000 };
    juce::Reverb verb;
    juce::AudioBuffer<float> scratch;

    float lofiHold[2] {}; int lofiCounter[2] {};
    double waterLfo = 0.0;
    float crackleEnv = 0.0f, crackleSign = 1.0f;
    float echoFb[2] {};
    juce::Random random;
};
