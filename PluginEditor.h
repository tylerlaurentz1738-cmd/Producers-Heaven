#pragma once
#include "PluginProcessor.h"
#include "UI/HomeScreen.h"
#include "UI/Pages.h"

class ProducersHeavenEditor : public juce::AudioProcessorEditor,
                              public juce::DragAndDropContainer,
                              private juce::Timer
{
public:
    explicit ProducersHeavenEditor (ProducersHeavenProcessor&);
    ~ProducersHeavenEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void navigate (int page); // -1 = home

private:
    void timerCallback() override;
    juce::Component* viewFor (int page);

    ProducersHeavenProcessor& proc;
    theme::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    HomeScreen home;
    std::unique_ptr<DrumForgePage> forge;
    std::unique_ptr<BeatDoctorPage> doctor;
    std::unique_ptr<VibeMatchPage> vibe;
    std::unique_ptr<FXHeavenPage> fx;

    int current = -1, target = -1;
    float fade = 1.0f; // 0..1 progress of the current transition
    double time = 0.0;

    struct Star { float x, y, size, speed, phase; };
    std::vector<Star> stars;
    struct Cloud { juce::Colour colour; float x, y, r, speed, phase; };
    std::vector<Cloud> clouds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProducersHeavenEditor)
};
