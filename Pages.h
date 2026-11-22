#pragma once
#include "Widgets.h"
#include "../Core/BeatAnalyzer.h"
#include <random>

// ---------- shared page frame (header + back button) ----------
class Page : public juce::Component
{
public:
    Page (const juce::String& title, const juce::String& subtitle, theme::Accent accent, ui::Icon icon);
    std::function<void()> onBack;
    virtual void tick() {}

    void paint (juce::Graphics&) override;
    void resized() override;

protected:
    virtual void layout (juce::Rectangle<int> content) = 0;
    juce::Rectangle<int> contentArea() const { return getLocalBounds().withTrimmedTop (headerH).reduced (28, 8).withTrimmedBottom (12); }
    static void drawHint (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text);

    static constexpr int headerH = 84;
    juce::String title, subtitle;
    theme::Accent accent;
    ui::Icon icon;
    ui::IconButton back;
};

// ---------- Drum Forge ----------
class DrumForgePage : public Page
{
public:
    explicit DrumForgePage (ProducersHeavenProcessor& p);
    void paint (juce::Graphics&) override;

private:
    void layout (juce::Rectangle<int>) override;
    void generateAll();
    void generateOne (ph::DrumType t);
    void saveKit();

    ProducersHeavenProcessor& proc;
    std::mt19937 rng;
    ph::Style style = ph::Style::Trap;
    juce::OwnedArray<theme::PillButton> styleButtons;
    theme::PillButton generate { "Generate Kit", theme::forge };
    theme::PillButton save { "Save Kit", theme::forge, false };
    juce::OwnedArray<ui::SoundTile> tiles;
    juce::Rectangle<int> hintArea;
    juce::String status;
};

// ---------- Beat Doctor ----------
class ListenOrb : public juce::Component
{
public:
    enum class State { Idle, Listening, Analyzing };
    State state = State::Idle;
    float progress = 0, level = 0, time = 0;
    std::function<void()> onClick;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override { if (e.mouseWasClicked() && onClick) onClick(); }
};

class TipCard : public juce::Component
{
public:
    TipCard (ProducersHeavenProcessor& p, const ph::Tip& t, ph::Style style);
    int heightForWidth (int w);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void makeSound();
    ProducersHeavenProcessor& proc;
    ph::Tip tip;
    ph::Style style;
    std::mt19937 rng;
    std::unique_ptr<theme::PillButton> fixButton;
    std::unique_ptr<ui::SoundTile> tile;
    int textH = 0;
};

class BeatDoctorPage : public Page
{
public:
    explicit BeatDoctorPage (ProducersHeavenProcessor& p);
    ~BeatDoctorPage() override;
    void tick() override;
    void paint (juce::Graphics&) override;

private:
    void layout (juce::Rectangle<int>) override;
    void toggleListen();
    void finishListening();
    void showReport (const ph::BeatReport& r);
    void layoutTips();

    ProducersHeavenProcessor& proc;
    ListenOrb orb;
    juce::OwnedArray<ui::ScoreRing> rings;
    juce::Viewport tipsView;
    juce::Component tipsContent;
    juce::OwnedArray<TipCard> cards;
    juce::String status, bpmText;
    juce::Rectangle<int> statusArea, metricsArea, listArea;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    double listenStart = 0;
};

// ---------- Vibe Match ----------
class VibeMatchPage : public Page, public juce::FileDragAndDropTarget
{
public:
    explicit VibeMatchPage (ProducersHeavenProcessor& p);
    ~VibeMatchPage() override;
    void tick() override;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void layout (juce::Rectangle<int>) override;
    void browse();
    void loadFile (const juce::File& f);
    void generateLoop (bool newKit);

    ProducersHeavenProcessor& proc;
    std::mt19937 rng;
    std::unique_ptr<juce::FileChooser> chooser;
    ph::GrooveProfile profile;
    ph::Kit kit;
    bool hasKit = false, dragOver = false, loading = false;
    int bars = 2;
    juce::String fileName, status;

    juce::Rectangle<int> dropArea;
    juce::OwnedArray<theme::PillButton> barButtons;
    juce::Slider variation;
    theme::PillButton generate { "Generate Loop", theme::vibe };
    theme::PillButton newKit { "New Sounds", theme::vibe, false };
    ui::StepGrid grid;
    ui::SoundTile loopTile, midiTile;
    double loopStartMs = -1, loopLengthMs = 0;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
};

// ---------- FX Heaven ----------
class FXHeavenPage : public Page
{
public:
    explicit FXHeavenPage (ProducersHeavenProcessor& p);
    void paint (juce::Graphics&) override;

private:
    void layout (juce::Rectangle<int>) override;

    struct Card
    {
        juce::String name, blurb;
        theme::Accent accent;
        juce::Rectangle<int> bounds;
        std::unique_ptr<ui::PowerToggle> toggle;
        std::unique_ptr<juce::Slider> knob;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> toggleAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> knobAtt;
    };

    ProducersHeavenProcessor& proc;
    std::vector<Card> cards;
    juce::Rectangle<int> masterArea, hintArea;
    juce::Slider mix, out, previewVol;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAtt, outAtt, prevAtt;
};
