#pragma once
#include "Theme.h"
#include "../PluginProcessor.h"

namespace ui
{
enum class Icon { DrumForge = 0, BeatDoctor, VibeMatch, FXHeaven, Back, Play, Stop, Dice, Folder, Midi };

void drawIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> r, juce::Colour colour, float stroke = 2.2f);
void setKnobAccent (juce::Slider& s, theme::Accent ac);

// Small round icon button (dice / play / folder ...)
class IconButton : public juce::Button
{
public:
    IconButton (Icon i, theme::Accent ac) : juce::Button ("icon"), icon (i), accent (ac)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
    void setIcon (Icon i) { icon = i; repaint(); }
    void paintButton (juce::Graphics& g, bool over, bool down) override;

private:
    Icon icon;
    theme::Accent accent;
};

// A generated sound: click to hear it, drag it straight into FL Studio.
class SoundTile : public juce::Component, public juce::SettableTooltipClient
{
public:
    SoundTile (ProducersHeavenProcessor& p, theme::Accent ac);

    void setSound (SharedBuffer buffer, const juce::String& name, const juce::String& label,
                   const juce::String& exportSubfolder, bool loopPreview = false);
    void setMidiFile (const juce::File& midi, const juce::String& name);
    void clear();
    bool hasSound() const { return buffer != nullptr || midiFile.existsAsFile(); }
    SharedBuffer getBuffer() const { return buffer; }
    juce::String getSoundName() const { return soundName; }
    juce::File ensureFile(); // writes the WAV the first time it's needed

    std::function<void()> onReroll; // shows a dice button when set
    std::function<void()> onPreview; // called when playback starts
    void setEmptyText (const juce::String& t) { emptyText = t; repaint(); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseDown (const juce::MouseEvent&) override { dragStarted = false; }
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    ProducersHeavenProcessor& proc;
    theme::Accent accent;
    SharedBuffer buffer;
    juce::String soundName, labelText, subfolder, emptyText { "Nothing here yet" };
    juce::File file, midiFile;
    bool loop = false, hover = false, dragStarted = false;
    std::vector<float> peaks;
    IconButton dice { Icon::Dice, theme::forge };
    float flash = 0.0f;
};

// Pattern display for Vibe Match
class StepGrid : public juce::Component
{
public:
    void setPattern (const ph::Pattern& p) { pattern = p; has = true; repaint(); }
    void clear() { has = false; repaint(); }
    void setPlayhead (float beatFraction) { playhead = beatFraction; repaint(); }
    void paint (juce::Graphics&) override;

private:
    ph::Pattern pattern;
    bool has = false;
    float playhead = -1.0f;
};

// 0..100 score ring
class ScoreRing : public juce::Component
{
public:
    ScoreRing (const juce::String& label, theme::Accent ac) : name (label), accent (ac) {}
    void setScore (float s) { target = s; known = true; }
    void reset() { target = 0; known = false; }
    void tick() { if (std::abs (shown - target) > 0.1f) { shown += (target - shown) * 0.12f; repaint(); } }
    void paint (juce::Graphics&) override;

private:
    juce::String name;
    theme::Accent accent;
    float shown = 0, target = 0;
    bool known = false;
};

// Glowing on/off switch
class PowerToggle : public juce::ToggleButton
{
public:
    explicit PowerToggle (theme::Accent ac) : accent (ac) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paintButton (juce::Graphics& g, bool over, bool down) override;

private:
    theme::Accent accent;
};
} // namespace ui
