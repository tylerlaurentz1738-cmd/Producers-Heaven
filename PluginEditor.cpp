#include "PluginEditor.h"

ProducersHeavenEditor::ProducersHeavenEditor (ProducersHeavenProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setOpaque (true);

    forge = std::make_unique<DrumForgePage> (proc);
    doctor = std::make_unique<BeatDoctorPage> (proc);
    vibe = std::make_unique<VibeMatchPage> (proc);
    fx = std::make_unique<FXHeavenPage> (proc);

    addAndMakeVisible (home);
    home.onSelect = [this] (int i) { navigate (i); };
    for (auto* page : std::initializer_list<Page*> { forge.get(), doctor.get(), vibe.get(), fx.get() })
    {
        addChildComponent (page);
        page->onBack = [this] { navigate (-1); };
    }

    juce::Random rnd (1234);
    for (int i = 0; i < 90; ++i)
        stars.push_back ({ rnd.nextFloat(), rnd.nextFloat(), 0.6f + rnd.nextFloat() * 1.6f, 0.004f + rnd.nextFloat() * 0.012f, rnd.nextFloat() * 6.28f });
    clouds = {
        { juce::Colour (0xffff4fd8), 0.15f, 0.85f, 0.45f, 0.05f, 0.0f },
        { juce::Colour (0xff3ee6ff), 0.85f, 0.80f, 0.40f, 0.04f, 2.0f },
        { juce::Colour (0xff8f6bff), 0.50f, 0.20f, 0.55f, 0.03f, 4.0f },
        { juce::Colour (0xffffd36e), 0.70f, 1.05f, 0.35f, 0.035f, 1.0f },
    };

    setSize (1040, 680);
    startTimerHz (60);
}

ProducersHeavenEditor::~ProducersHeavenEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

juce::Component* ProducersHeavenEditor::viewFor (int page)
{
    switch (page)
    {
        case 0: return forge.get();
        case 1: return doctor.get();
        case 2: return vibe.get();
        case 3: return fx.get();
        default: return &home;
    }
}

void ProducersHeavenEditor::navigate (int page)
{
    if (page == current && fade >= 1.0f) return;
    if (fade < 1.0f) // finish any running transition instantly
    {
        viewFor (current)->setVisible (false);
        current = target;
    }
    target = page;
    auto* in = viewFor (target);
    in->setAlpha (0.0f);
    in->setVisible (true);
    in->toFront (false);
    fade = 0.0f;
}

void ProducersHeavenEditor::timerCallback()
{
    time += 1.0 / 60.0;

    if (fade < 1.0f)
    {
        fade = juce::jmin (1.0f, fade + 0.09f);
        const float e = 1.0f - std::pow (1.0f - fade, 3.0f); // ease-out
        auto* in = viewFor (target);
        auto* out = viewFor (current);
        in->setAlpha (e);
        if (out != in) out->setAlpha (1.0f - e);
        const int dy = juce::roundToInt ((1.0f - e) * 18.0f);
        in->setTopLeftPosition (0, dy);
        if (fade >= 1.0f)
        {
            if (out != in) { out->setVisible (false); out->setAlpha (1.0f); }
            in->setTopLeftPosition (0, 0);
            current = target;
        }
    }

    if (home.isVisible()) home.tick (time);
    if (auto* page = dynamic_cast<Page*> (viewFor (current))) page->tick();
    if (target != current)
        if (auto* page = dynamic_cast<Page*> (viewFor (target))) page->tick();

    // drift the sky
    for (auto& s : stars)
    {
        s.y -= s.speed / 60.0f;
        if (s.y < -0.02f) { s.y = 1.02f; }
    }
    repaint();
}

void ProducersHeavenEditor::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    juce::ColourGradient sky (theme::bgTop, 0, 0, theme::bgBottom, 0, r.getHeight(), false);
    sky.addColour (0.55, theme::bgMid);
    g.setGradientFill (sky);
    g.fillAll();

    for (auto& c : clouds)
    {
        const float cx = (c.x + 0.06f * std::sin ((float) time * c.speed * 6.0f + c.phase)) * r.getWidth();
        const float cy = (c.y + 0.04f * std::cos ((float) time * c.speed * 5.0f + c.phase)) * r.getHeight();
        theme::drawGlow (g, { cx, cy }, c.r * r.getWidth(), c.colour, 0.16f);
    }

    for (auto& s : stars)
    {
        const float tw = 0.35f + 0.65f * (0.5f + 0.5f * std::sin ((float) time * 1.7f + s.phase));
        g.setColour (juce::Colours::white.withAlpha (0.55f * tw));
        g.fillEllipse (s.x * r.getWidth(), s.y * r.getHeight(), s.size, s.size);
    }

    // soft cloud bank along the bottom
    juce::ColourGradient mist (juce::Colours::white.withAlpha (0.0f), 0, r.getHeight() * 0.72f,
                               juce::Colour (0xffd9c8ff).withAlpha (0.1f), 0, r.getHeight(), false);
    g.setGradientFill (mist);
    g.fillRect (r.withTrimmedTop (r.getHeight() * 0.72f));
}

void ProducersHeavenEditor::resized()
{
    const auto b = getLocalBounds();
    home.setBounds (b);
    for (auto* page : std::initializer_list<juce::Component*> { forge.get(), doctor.get(), vibe.get(), fx.get() })
        page->setBounds (b);
}
