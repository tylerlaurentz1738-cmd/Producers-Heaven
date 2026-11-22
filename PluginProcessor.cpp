#include "PluginProcessor.h"
#include "PluginEditor.h"

ProducersHeavenProcessor::ProducersHeavenProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (int e = 0; e < FXRack::NumEffects; ++e)
    {
        onParams[(size_t) e] = apvts.getRawParameterValue (juce::String (FXRack::id (e)) + "_on");
        amtParams[(size_t) e] = apvts.getRawParameterValue (juce::String (FXRack::id (e)) + "_amt");
    }
    mixParam = apvts.getRawParameterValue ("fx_mix");
    outParam = apvts.getRawParameterValue ("out_gain");
    previewParam = apvts.getRawParameterValue ("preview_vol");
}

juce::AudioProcessorValueTreeState::ParameterLayout ProducersHeavenProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    static const float defaults[] = { 0.45f, 0.6f, 0.5f, 0.6f, 0.5f, 0.35f, 0.45f };
    for (int e = 0; e < FXRack::NumEffects; ++e)
    {
        const juce::String id (FXRack::id (e));
        const juce::String nm (FXRack::name (e));
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id + "_on", 1 }, nm, false));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id + "_amt", 1 }, nm + " Amount",
                                                                 juce::NormalisableRange<float> (0.0f, 1.0f), defaults[e]));
    }
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "fx_mix", 1 }, "FX Mix",
                                                             juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "out_gain", 1 }, "Output",
                                                             juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "preview_vol", 1 }, "Preview Volume",
                                                             juce::NormalisableRange<float> (0.0f, 1.0f), 0.8f));
    return layout;
}

bool ProducersHeavenProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void ProducersHeavenProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentRate = sampleRate;
    fx.prepare (sampleRate, samplesPerBlock, juce::jmax (1, getTotalNumOutputChannels()));
    dryBuffer.setSize (2, samplesPerBlock);
    mixSmoothed.reset (sampleRate, 0.05);
    outSmoothed.reset (sampleRate, 0.05);

    listening = false;
    listenPos = 0;
    listenBuffer.setSize (2, (int) (maxListenSeconds * sampleRate));
    listenBuffer.clear();
}

void ProducersHeavenProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numCh = getTotalNumOutputChannels();
    for (int ch = getTotalNumInputChannels(); ch < numCh; ++ch) buffer.clear (ch, 0, n);

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                hostBpm = *bpm;

    // ---- Beat Doctor capture (what's playing into the plugin, before FX) ----
    if (listening.load())
    {
        const int cap = listenBuffer.getNumSamples();
        const int pos = listenPos.load();
        const int toCopy = juce::jmin (n, cap - pos);
        if (toCopy > 0 && numCh > 0)
        {
            for (int ch = 0; ch < 2; ++ch)
                listenBuffer.copyFrom (ch, pos, buffer, juce::jmin (ch, numCh - 1), 0, toCopy);
            listenLevel = buffer.getRMSLevel (0, 0, n);
            listenPos = pos + toCopy;
        }
        if (listenPos.load() >= cap) listening = false;
    }

    // ---- MIDI notes trigger the generated kit ----
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (! msg.isNoteOn()) continue;
        for (int t = 0; t < ph::kNumDrumTypes; ++t)
            if (ph::DrumSynth::midiNote ((ph::DrumType) t) == msg.getNoteNumber())
            {
                SharedBuffer pad;
                {
                    const juce::SpinLock::ScopedTryLockType lock (voiceLock);
                    if (lock.isLocked()) pad = pads[(size_t) t];
                }
                if (pad) startVoice (pad, msg.getFloatVelocity(), false);
            }
    }

    // ---- FX Heaven ----
    FXRack::Settings s;
    bool anyOn = false;
    for (int e = 0; e < FXRack::NumEffects; ++e)
    {
        s.on[e] = onParams[(size_t) e]->load() > 0.5f;
        s.amount[e] = amtParams[(size_t) e]->load();
        anyOn |= s.on[e];
    }
    s.bpm = hostBpm.load();

    mixSmoothed.setTargetValue (mixParam->load());
    outSmoothed.setTargetValue (juce::Decibels::decibelsToGain (outParam->load()));

    if (anyOn && numCh > 0)
    {
        for (int ch = 0; ch < juce::jmin (numCh, 2); ++ch) dryBuffer.copyFrom (ch, 0, buffer, ch, 0, n);
        fx.process (buffer, s);
        for (int i = 0; i < n; ++i)
        {
            const float m = mixSmoothed.getNextValue();
            for (int ch = 0; ch < juce::jmin (numCh, 2); ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                d[i] = dryBuffer.getSample (ch, i) + (d[i] - dryBuffer.getSample (ch, i)) * m;
            }
        }
    }
    else
        mixSmoothed.skip (n);

    for (int i = 0; i < n; ++i)
    {
        const float g = outSmoothed.getNextValue();
        for (int ch = 0; ch < numCh; ++ch) buffer.getWritePointer (ch)[i] *= g;
    }

    // ---- previews are added clean, after the FX ----
    renderVoices (buffer, n);
}

void ProducersHeavenProcessor::startVoice (SharedBuffer buf, float gain, bool loop)
{
    if (! buf || buf->getNumSamples() == 0) return;
    const juce::SpinLock::ScopedLockType lock (voiceLock);
    Voice* target = nullptr;
    for (auto& v : voices)
    {
        if (loop && v.active && v.loop) v.active = false; // only one loop at a time
        if (! v.active && target == nullptr) target = &v;
    }
    if (target == nullptr)
    {
        target = &voices[0];
        for (auto& v : voices) if (v.position > target->position) target = &v;
    }
    target->buffer = std::move (buf);
    target->position = 0;
    target->gain = gain;
    target->loop = loop;
    target->active = true;
}

void ProducersHeavenProcessor::renderVoices (juce::AudioBuffer<float>& out, int n)
{
    const juce::SpinLock::ScopedTryLockType lock (voiceLock);
    if (! lock.isLocked()) return;
    const float vol = previewParam->load();
    const int outCh = out.getNumChannels();
    bool anyLoop = false;

    for (auto& v : voices)
    {
        if (! v.active || ! v.buffer) continue;
        const auto& src = *v.buffer;
        const int len = src.getNumSamples();
        int written = 0;
        while (written < n && v.active)
        {
            const int chunk = juce::jmin (n - written, len - v.position);
            for (int ch = 0; ch < outCh; ++ch)
                out.addFrom (ch, written, src, juce::jmin (ch, src.getNumChannels() - 1), v.position, chunk, v.gain * vol);
            written += chunk;
            v.position += chunk;
            if (v.position >= len)
            {
                if (v.loop) v.position = 0;
                else v.active = false; // buffer is released later on the message thread
            }
        }
        anyLoop |= v.active && v.loop;
    }
    loopPlaying = anyLoop;
}

void ProducersHeavenProcessor::preview (SharedBuffer buffer, bool loop)
{
    startVoice (std::move (buffer), 1.0f, loop);
    if (loop) loopPlaying = true;
}

void ProducersHeavenProcessor::stopPreview()
{
    const juce::SpinLock::ScopedLockType lock (voiceLock);
    for (auto& v : voices) { v.active = false; v.buffer.reset(); }
    loopPlaying = false;
}

void ProducersHeavenProcessor::setPad (ph::DrumType type, SharedBuffer buffer)
{
    const juce::SpinLock::ScopedLockType lock (voiceLock);
    pads[(size_t) type] = std::move (buffer);
}

void ProducersHeavenProcessor::startListening()
{
    if (listenBuffer.getNumSamples() == 0) return;
    listenPos = 0;
    listenLevel = 0.0f;
    listening = true;
}

void ProducersHeavenProcessor::stopListening() { listening = false; }

float ProducersHeavenProcessor::getListenProgress() const
{
    const int cap = listenBuffer.getNumSamples();
    return cap > 0 ? (float) listenPos.load() / (float) cap : 0.0f;
}

void ProducersHeavenProcessor::takeRecording (std::vector<float>& left, std::vector<float>& right)
{
    listening = false;
    const int len = juce::jmin (listenPos.load(), listenBuffer.getNumSamples());
    left.assign (listenBuffer.getReadPointer (0), listenBuffer.getReadPointer (0) + len);
    right.assign (listenBuffer.getReadPointer (1), listenBuffer.getReadPointer (1) + len);
}

// ---------------- helpers ----------------
SharedBuffer ProducersHeavenProcessor::toBuffer (const std::vector<float>& mono)
{
    auto b = std::make_shared<juce::AudioBuffer<float>> (1, (int) mono.size());
    if (! mono.empty()) b->copyFrom (0, 0, mono.data(), (int) mono.size());
    return b;
}

SharedBuffer ProducersHeavenProcessor::toBuffer (const ph::StereoAudio& s)
{
    const int n = (int) juce::jmin (s.left.size(), s.right.size());
    auto b = std::make_shared<juce::AudioBuffer<float>> (2, n);
    if (n > 0)
    {
        b->copyFrom (0, 0, s.left.data(), n);
        b->copyFrom (1, 0, s.right.data(), n);
    }
    return b;
}

juce::File ProducersHeavenProcessor::exportFolder (const juce::String& sub)
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("Producers Heaven").getChildFile (sub);
    dir.createDirectory();
    return dir;
}

juce::File ProducersHeavenProcessor::uniqueFile (const juce::String& sub, const juce::String& name, const juce::String& ext)
{
    return exportFolder (sub).getNonexistentChildFile (juce::File::createLegalFileName (name), ext, false);
}

bool ProducersHeavenProcessor::writeWav (const juce::AudioBuffer<float>& buffer, double sampleRate, const juce::File& file)
{
    file.deleteFile();
    auto stream = file.createOutputStream();
    if (stream == nullptr) return false;
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), sampleRate,
                                                                          (unsigned int) buffer.getNumChannels(), 24, {}, 0));
    if (writer == nullptr) return false;
    stream.release(); // the writer owns it now
    return writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
}

bool ProducersHeavenProcessor::writeMidi (const ph::Pattern& pattern, const juce::File& file)
{
    constexpr int ppq = 96;
    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / pattern.bpm)), 0.0);
    seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
    for (const auto& e : ph::PatternEngine::events (pattern))
    {
        const double tick = e.beat * ppq;
        const int note = ph::DrumSynth::midiNote (e.type);
        const auto vel = (juce::uint8) juce::jlimit (1, 127, (int) (e.velocity * 127.0f));
        seq.addEvent (juce::MidiMessage::noteOn (1, note, vel), tick);
        seq.addEvent (juce::MidiMessage::noteOff (1, note), tick + ppq / 8.0);
    }
    seq.updateMatchedPairs();
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (ppq);
    mf.addTrack (seq);
    file.deleteFile();
    juce::FileOutputStream os (file);
    return os.openedOk() && mf.writeTo (os);
}

// ---------------- state ----------------
void ProducersHeavenProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ProducersHeavenProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* ProducersHeavenProcessor::createEditor() { return new ProducersHeavenEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ProducersHeavenProcessor(); }
