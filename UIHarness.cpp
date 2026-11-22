// Integration harness: loads the real plugin processor + editor, drives every page and
// renders screenshots. Build with -DPH_BUILD_HARNESS=ON. Usage: PHHarness <outputDir>
#include "PluginProcessor.h"
#include "PluginEditor.h"

template <typename T>
static T* findChild (juce::Component& root)
{
    for (auto* c : root.getChildren())
    {
        if (auto* t = dynamic_cast<T*> (c)) return t;
        if (auto* t = findChild<T> (*c)) return t;
    }
    return nullptr;
}

static void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

static void snap (juce::Component& c, const juce::File& f)
{
    auto img = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s\n", f.getFullPathName().toRawUTF8());
}

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("%s %s\n", ok ? "ok:  " : "FAIL:", what);
    if (! ok) ++failures;
}

static float processSeconds (ProducersHeavenProcessor& p, const juce::AudioBuffer<float>& src, double seconds, bool& finite)
{
    const int block = 512;
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    const int total = (int) (seconds * 44100.0);
    float peak = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < block; ++i)
                buf.setSample (ch, i, src.getSample (ch, (pos + i) % src.getNumSamples()));
        p.processBlock (buf, midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < block; ++i)
            {
                const float v = buf.getSample (ch, i);
                finite &= std::isfinite (v);
                peak = std::max (peak, std::abs (v));
            }
    }
    return peak;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out (argc > 1 ? argv[1] : "/tmp/ph_shots");
    out.createDirectory();

    ProducersHeavenProcessor proc;
    proc.setPlayConfigDetails (2, 2, 44100.0, 512);
    proc.prepareToPlay (44100.0, 512);

    // a test beat to feed the plugin
    std::mt19937 rng (11);
    ph::GrooveProfile g; g.style = ph::Style::Trap; g.bpm = 140; g.valid = true;
    auto pattern = ph::PatternEngine::generate (g, 2, 0.2f, rng);
    auto kit = ph::PatternEngine::makeKit (g, rng, 44100.0);
    auto beat = ProducersHeavenProcessor::toBuffer (ph::PatternEngine::render (pattern, kit, 44100.0));

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto* editor = dynamic_cast<ProducersHeavenEditor*> (ed.get());
    editor->setVisible (true);
    pump (900);
    snap (*editor, out.getChildFile ("1_home.png"));

    // ---- Drum Forge ----
    editor->navigate (0);
    pump (600);
    snap (*editor, out.getChildFile ("2_drum_forge.png"));

    // ---- Beat Doctor: listen to the beat, then stop ----
    editor->navigate (1);
    pump (500);
    auto* orb = findChild<ListenOrb> (*editor);
    check (orb != nullptr, "found listen button");
    orb->onClick();
    bool finite = true;
    processSeconds (proc, *beat, 10.0, finite);
    pump (300);
    snap (*editor, out.getChildFile ("3a_beat_doctor_listening.png"));
    orb->onClick();
    for (int i = 0; i < 40 && findChild<TipCard> (*editor) == nullptr; ++i) pump (100);
    check (findChild<TipCard> (*editor) != nullptr, "beat doctor produced tips");
    if (auto* card = findChild<TipCard> (*editor))
        if (auto* btn = findChild<theme::PillButton> (*card)) btn->triggerClick();
    pump (900);
    snap (*editor, out.getChildFile ("3b_beat_doctor_results.png"));

    // ---- Vibe Match: drop a WAV file on it ----
    const auto ref = out.getChildFile ("reference_beat.wav");
    ProducersHeavenProcessor::writeWav (*beat, 44100.0, ref);
    editor->navigate (2);
    pump (500);
    auto* vibe = findChild<VibeMatchPage> (*editor);
    vibe->filesDropped ({ ref.getFullPathName() }, 0, 0);
    for (int i = 0; i < 60; ++i) pump (100);
    snap (*editor, out.getChildFile ("4_vibe_match.png"));
    const auto vibeDir = ProducersHeavenProcessor::exportFolder ("Vibe Match");
    check (vibeDir.getNumberOfChildFiles (juce::File::findFiles, "*.mid") > 0, "vibe match wrote a MIDI file");

    // ---- FX Heaven: switch on a few, then torture-test every effect ----
    for (auto* id : { "dist_on", "water_on", "verb_on" })
        proc.apvts.getParameter (id)->setValueNotifyingHost (1.0f);
    editor->navigate (3);
    pump (700);
    snap (*editor, out.getChildFile ("5_fx_heaven.png"));
    proc.stopPreview();
    bool dryOk = true;
    for (int k = 0; k < FXRack::NumEffects; ++k)
        proc.apvts.getParameter (juce::String (FXRack::id (k)) + "_on")->setValueNotifyingHost (0.0f);
    const float dryPeak = processSeconds (proc, *beat, 4.0, dryOk);
    check (dryOk && std::abs (dryPeak - 0.89f) < 0.05f, ("bypass is clean: peak " + juce::String (dryPeak, 2)).toRawUTF8());

    for (int e = 0; e < FXRack::NumEffects; ++e)
    {
        for (int k = 0; k < FXRack::NumEffects; ++k)
            proc.apvts.getParameter (juce::String (FXRack::id (k)) + "_on")->setValueNotifyingHost (k == e ? 1.0f : 0.0f);
        for (float amt : { 0.0f, 0.5f, 1.0f })
        {
            proc.apvts.getParameter (juce::String (FXRack::id (e)) + "_amt")->setValueNotifyingHost (amt);
            bool ok = true;
            const float peak = processSeconds (proc, *beat, 2.0, ok);
            char msg[128];
            std::snprintf (msg, sizeof msg, "%s @ %.0f%%: peak %.2f", FXRack::name (e), amt * 100, peak);
            check (ok && peak < 4.0f && peak > 0.01f, msg);
        }
    }
    for (int k = 0; k < FXRack::NumEffects; ++k)
        proc.apvts.getParameter (juce::String (FXRack::id (k)) + "_on")->setValueNotifyingHost (1.0f);
    bool ok = true;
    const float peakAll = processSeconds (proc, *beat, 4.0, ok);
    check (ok && peakAll < 4.0f, ("all 7 effects stacked: peak " + juce::String (peakAll, 2)).toRawUTF8());

    // MIDI triggers the kit
    for (int k = 0; k < FXRack::NumEffects; ++k)
        proc.apvts.getParameter (juce::String (FXRack::id (k)) + "_on")->setValueNotifyingHost (0.0f);
    juce::AudioBuffer<float> silent (2, 512);
    silent.clear();
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 120), 0);
    proc.processBlock (silent, midi);
    check (silent.getMagnitude (0, 512) > 0.01f, "MIDI note 36 plays the generated kick");

    // state save/restore
    juce::MemoryBlock state;
    proc.getStateInformation (state);
    ProducersHeavenProcessor proc2;
    proc2.setStateInformation (state.getData(), (int) state.getSize());
    check (proc2.apvts.getParameter ("dist_on")->getValue() < 0.5f, "state restores");

    ed.reset();
    std::printf (failures ? "\n%d FAILURES\n" : "\nALL PASSED\n", failures);
    return failures ? 1 : 0;
}
