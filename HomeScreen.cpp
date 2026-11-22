#include "HomeScreen.h"

HomeScreen::HomeScreen()
{
    orbs = {
        { "Drum Forge",  "Generate drums off the rip", theme::forge,  ui::Icon::DrumForge,  { 0.17f, 0.60f }, 0.0f, 0.0f, {}, 0.0f },
        { "Beat Doctor", "Listens & gives tips",       theme::doctor, ui::Icon::BeatDoctor, { 0.39f, 0.70f }, 1.7f, 0.0f, {}, 0.0f },
        { "Vibe Match",  "Upload a beat, get a loop",  theme::vibe,   ui::Icon::VibeMatch,  { 0.61f, 0.70f }, 3.1f, 0.0f, {}, 0.0f },
        { "FX Heaven",   "Distortion, underwater +",   theme::fxh,    ui::Icon::FXHeaven,   { 0.83f, 0.60f }, 4.6f, 0.0f, {}, 0.0f },
    };
}

void HomeScreen::tick (double t)
{
    time = t;
    const float w = (float) getWidth(), h = (float) getHeight();
    const float base = juce::jmin (w, h) * 0.105f;
    for (size_t i = 0; i < orbs.size(); ++i)
    {
        auto& o = orbs[i];
        const float target = (int) i == hovered ? 1.0f : 0.0f;
        o.hover += (target - o.hover) * 0.18f;
        const float bobY = std::sin ((float) t * 1.1f + o.phase) * 9.0f;
        const float bobX = std::cos ((float) t * 0.7f + o.phase * 1.3f) * 5.0f;
        o.centre = { o.home.x * w + bobX, o.home.y * h + bobY - o.hover * 6.0f };
        o.radius = base * (1.0f + 0.12f * o.hover);
    }
    repaint();
}

int HomeScreen::hitTestOrb (juce::Point<float> p) const
{
    for (size_t i = 0; i < orbs.size(); ++i)
        if (p.getDistanceFrom (orbs[i].centre) <= orbs[i].radius * 1.08f) return (int) i;
    return -1;
}

void HomeScreen::mouseMove (const juce::MouseEvent& e)
{
    hovered = hitTestOrb (e.position);
    setMouseCursor (hovered >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void HomeScreen::mouseExit (const juce::MouseEvent&) { hovered = -1; }

void HomeScreen::mouseUp (const juce::MouseEvent& e)
{
    const int i = hitTestOrb (e.position);
    if (i >= 0 && e.mouseWasClicked() && onSelect) onSelect (i);
}

void HomeScreen::paint (juce::Graphics& g)
{
    const float w = (float) getWidth(), h = (float) getHeight();

    // ---- title with halo ----
    const juce::Point<float> haloC (w * 0.5f, h * 0.15f);
    const float pulse = 0.5f + 0.5f * std::sin ((float) time * 1.4f);
    theme::drawGlow (g, haloC.translated (0, 34), 260.0f, juce::Colour (0xff8f6bff), 0.18f + 0.05f * pulse);
    g.setColour (juce::Colour (0xffffe6a6).withAlpha (0.75f));
    g.drawEllipse (haloC.x - 46, haloC.y - 9, 92, 18, 2.5f);
    theme::drawGlow (g, haloC, 60.0f, juce::Colour (0xffffe6a6), 0.25f + 0.1f * pulse);

    auto titleArea = juce::Rectangle<float> (0, h * 0.15f + 14, w, 60);
    juce::ColourGradient tg (juce::Colour (0xffffffff), 0, titleArea.getY(), juce::Colour (0xffc9b8ff), 0, titleArea.getBottom(), false);
    g.setGradientFill (tg);
    g.setFont (theme::font (50.0f, true).withExtraKerningFactor (0.04f));
    g.drawText ("Producers Heaven", titleArea, juce::Justification::centred);
    g.setColour (theme::textDim);
    g.setFont (theme::font (15.0f).withExtraKerningFactor (0.2f));
    g.drawText ("PICK A CLOUD", titleArea.translated (0, 54).withHeight (22), juce::Justification::centred);

    // faint constellation lines between orbs
    juce::Path links;
    for (size_t i = 0; i + 1 < orbs.size(); ++i)
    {
        links.startNewSubPath (orbs[i].centre);
        links.lineTo (orbs[i + 1].centre);
    }
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.strokePath (links, juce::PathStrokeType (1.0f));

    // ---- orbs ----
    for (size_t i = 0; i < orbs.size(); ++i)
    {
        const auto& o = orbs[i];
        const auto c = o.centre;
        const float r = o.radius;

        theme::drawGlow (g, c, r * 2.1f, o.accent.b, 0.22f + 0.25f * o.hover);
        theme::drawGlow (g, c, r * 1.5f, o.accent.a, 0.15f + 0.2f * o.hover);

        juce::ColourGradient sphere (o.accent.a.brighter (0.15f), c.x - r * 0.6f, c.y - r * 0.7f,
                                     o.accent.b.darker (0.35f), c.x + r * 0.7f, c.y + r * 0.8f, false);
        g.setGradientFill (sphere);
        g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);

        // glassy highlight + rim light
        juce::ColourGradient shine (juce::Colours::white.withAlpha (0.55f), c.x - r * 0.35f, c.y - r * 0.55f,
                                    juce::Colours::white.withAlpha (0.0f), c.x - r * 0.35f, c.y + r * 0.1f, false);
        g.setGradientFill (shine);
        g.fillEllipse (c.x - r * 0.72f, c.y - r * 0.9f, r * 1.2f, r * 0.85f);
        g.setColour (juce::Colours::white.withAlpha (0.3f + 0.3f * o.hover));
        g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 1.2f);

        // orbiting sparkles
        for (int k = 0; k < 3; ++k)
        {
            const float a = (float) time * (0.6f + 0.25f * k) + o.phase + k * 2.1f;
            const float orbitR = r * (1.22f + 0.1f * k);
            const juce::Point<float> sp (c.x + std::cos (a) * orbitR, c.y + std::sin (a) * orbitR * 0.45f);
            const float ss = 2.0f + 1.5f * o.hover;
            g.setColour (juce::Colours::white.withAlpha (0.5f + 0.4f * o.hover));
            g.fillEllipse (sp.x - ss, sp.y - ss, ss * 2, ss * 2);
        }

        ui::drawIcon (g, o.icon, juce::Rectangle<float> (c.x - r * 0.48f, c.y - r * 0.48f, r * 0.96f, r * 0.96f),
                      juce::Colours::white, 2.2f + r * 0.02f);

        auto label = juce::Rectangle<float> (c.x - 110, c.y + r + 16, 220, 24);
        g.setColour (theme::text);
        g.setFont (theme::font (19.0f, true));
        g.drawText (o.name, label, juce::Justification::centred);
        g.setColour (theme::textDim.withAlpha (0.75f + 0.25f * o.hover));
        g.setFont (theme::font (13.0f));
        g.drawText (o.subtitle, label.translated (0, 22), juce::Justification::centred);
    }
}
