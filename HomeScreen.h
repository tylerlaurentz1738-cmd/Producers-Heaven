#pragma once
#include "Widgets.h"

// Home screen: the four features float around as glowing orbs. Click one to open it.
class HomeScreen : public juce::Component
{
public:
    HomeScreen();
    std::function<void (int)> onSelect;

    void tick (double timeSeconds);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    struct Orb
    {
        juce::String name, subtitle;
        theme::Accent accent;
        ui::Icon icon;
        juce::Point<float> home; // relative 0..1 position
        float phase;
        float hover = 0.0f;
        juce::Point<float> centre; // current pixel position
        float radius = 0.0f;
    };

    int hitTestOrb (juce::Point<float> p) const;

    std::vector<Orb> orbs;
    int hovered = -1;
    double time = 0.0;
};
