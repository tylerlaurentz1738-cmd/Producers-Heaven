#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "DSP/FXRack.h"
#include "Core/DrumSynth.h"
#include "Core/PatternEngine.h"
#include <array>
#include <atomic>
#include <memory>

using SharedBuffer = std::shared_ptr<const juce::AudioBuffer<float>>;

class ProducersHeavenProcessor : public juce::AudioProcessor
{
public:
    ProducersHeavenProcessor();
    ~ProducersHeavenProcessor() override = default;

    // ---- AudioProcessor ----
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Producers Heaven"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // ---- sample rate the UI should render sounds at ----
    double getRenderSampleRate() const { return currentRate > 0 ? currentRate : 44100.0; }
    double getHostBpm() const { return hostBpm.load(); }

    // ---- Beat Doctor: record what's playing through the plugin ----
    static constexpr double maxListenSeconds = 20.0;
    void startListening();
    void stopListening();
    bool isListening() const { return listening.load(); }
    float getListenProgress() const;
    float getListenLevel() const { return listenLevel.load(); }
    void takeRecording (std::vector<float>& left, std::vector<float>& right);

    // ---- previews & MIDI-playable kit ----
    void preview (SharedBuffer buffer, bool loop = false);
    void stopPreview();
    bool isLoopPlaying() const { return loopPlaying.load(); }
    void setPad (ph::DrumType type, SharedBuffer buffer);

    // ---- helpers shared with the UI ----
    static SharedBuffer toBuffer (const std::vector<float>& mono);
    static SharedBuffer toBuffer (const ph::StereoAudio& stereo);
    static juce::File exportFolder (const juce::String& sub);
    static juce::File uniqueFile (const juce::String& sub, const juce::String& name, const juce::String& ext);
    static bool writeWav (const juce::AudioBuffer<float>& buffer, double sampleRate, const juce::File& file);
    static bool writeMidi (const ph::Pattern& pattern, const juce::File& file);

private:
    struct Voice
    {
        SharedBuffer buffer;
        int position = 0;
        float gain = 1.0f;
        bool loop = false;
        bool active = false;
    };

    void startVoice (SharedBuffer buffer, float gain, bool loop);
    void renderVoices (juce::AudioBuffer<float>& out, int numSamples);

    FXRack fx;
    juce::AudioBuffer<float> dryBuffer;
    juce::LinearSmoothedValue<float> mixSmoothed { 1.0f }, outSmoothed { 1.0f };

    std::array<std::atomic<float>*, FXRack::NumEffects> onParams {}, amtParams {};
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* outParam = nullptr;
    std::atomic<float>* previewParam = nullptr;

    double currentRate = 0.0;
    std::atomic<double> hostBpm { 120.0 };

    // listening
    juce::AudioBuffer<float> listenBuffer;
    std::atomic<bool> listening { false };
    std::atomic<int> listenPos { 0 };
    std::atomic<float> listenLevel { 0.0f };

    // voices
    juce::SpinLock voiceLock;
    std::array<Voice, 24> voices;
    std::array<SharedBuffer, ph::kNumDrumTypes> pads;
    std::atomic<bool> loopPlaying { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProducersHeavenProcessor)
};
