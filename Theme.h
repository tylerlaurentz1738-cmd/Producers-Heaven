#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace theme
{
// Dreamy night-sky palette
const juce::Colour bgTop    { 0xff0a0920 };
const juce::Colour bgMid    { 0xff181040 };
const juce::Colour bgBottom { 0xff2b1459 };
const juce::Colour text     { 0xfff4f1ff };
const juce::Colour textDim  { 0xffa9a3d1 };
const juce::Colour glass    { 0x14ffffff };
const juce::Colour glassEdge{ 0x26ffffff };

struct Accent { juce::Colour a, b; };
const Accent forge  { juce::Colour (0xffff8a5b), juce::Colour (0xffff4fd8) }; // Drum Forge
const Accent doctor { juce::Colour (0xff3ee6ff), juce::Colour (0xff5a6bff) }; // Beat Doctor
const Accent vibe   { juce::Colour (0xffb06bff), juce::Colour (0xffff5ed1) }; // Vibe Match
const Accent fxh    { juce::Colour (0xffffd36e), juce::Colour (0xff5cffc8) }; // FX Heaven

inline juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

inline juce::ColourGradient gradient (const Accent& ac, juce::Rectangle<float> r)
{
    return juce::ColourGradient (ac.a, r.getX(), r.getY(), ac.b, r.getRight(), r.getBottom(), false);
}

inline void drawGlass (juce::Graphics& g, juce::Rectangle<float> r, float radius = 18.0f, float alpha = 1.0f)
{
    g.setColour (glass.withMultipliedAlpha (alpha));
    g.fillRoundedRectangle (r, radius);
    juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.07f * alpha), r.getX(), r.getY(),
                                juce::Colours::transparentWhite, r.getX(), r.getY() + r.getHeight() * 0.6f, false);
    g.setGradientFill (sheen);
    g.fillRoundedRectangle (r, radius);
    g.setColour (glassEdge.withMultipliedAlpha (alpha));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}

inline void drawGlow (juce::Graphics& g, juce::Point<float> c, float radius, juce::Colour col, float strength = 0.5f)
{
    juce::ColourGradient glow (col.withAlpha (strength), c.x, c.y, col.withAlpha (0.0f), c.x + radius, c.y, true);
    g.setGradientFill (glow);
    g.fillEllipse (c.x - radius, c.y - radius, radius * 2, radius * 2);
}

// Pill-shaped gradient button
class PillButton : public juce::Button
{
public:
    PillButton (const juce::String& label, Accent ac, bool filled = true)
        : juce::Button (label), accent (ac), solid (filled) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    void setAccent (Accent ac) { accent = ac; repaint(); }
    void setFilled (bool f) { solid = f; repaint(); }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const float rad = r.getHeight() * 0.5f;
        const bool on = solid || getToggleState();
        if (on)
        {
            if (isEnabled()) drawGlow (g, r.getCentre(), r.getWidth() * 0.55f, accent.b, over ? 0.25f : 0.12f);
            g.setGradientFill (gradient (accent, r));
            g.setOpacity (isEnabled() ? (down ? 0.8f : 1.0f) : 0.35f);
            g.fillRoundedRectangle (r, rad);
            g.setOpacity (1.0f);
        }
        else
        {
            g.setColour (juce::Colours::white.withAlpha (over ? 0.12f : 0.06f));
            g.fillRoundedRectangle (r, rad);
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.drawRoundedRectangle (r, rad, 1.0f);
        }
        g.setColour (on ? juce::Colour (0xff120b2a) : theme::text.withAlpha (isEnabled() ? 0.9f : 0.4f));
        g.setFont (font (juce::jmin (15.0f, r.getHeight() * 0.42f), true));
        g.drawText (getButtonText(), r, juce::Justification::centred);
    }

private:
    Accent accent;
    bool solid;
};

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, text);
        setColour (juce::ScrollBar::thumbColourId, juce::Colours::white.withAlpha (0.25f));
        setColour (juce::TooltipWindow::backgroundColourId, bgMid);
        setColour (juce::TooltipWindow::textColourId, text);
    }

    Accent knobAccent = doctor;

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s) override
    {
        auto acc = knobAccent;
        if (s.getProperties().contains ("accentA"))
            acc = { juce::Colour ((juce::uint32) (juce::int64) s.getProperties()["accentA"]),
                    juce::Colour ((juce::uint32) (juce::int64) s.getProperties()["accentB"]) };

        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
        const float size = juce::jmin (r.getWidth(), r.getHeight());
        r = r.withSizeKeepingCentre (size, size);
        const auto c = r.getCentre();
        const float rad = size * 0.5f;
        const float angle = start + pos * (end - start);
        const bool active = s.isEnabled() && ! s.getProperties().getWithDefault ("dim", false);

        juce::Path track;
        track.addCentredArc (c.x, c.y, rad - 3, rad - 3, 0, start, end, true);
        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path val;
        val.addCentredArc (c.x, c.y, rad - 3, rad - 3, 0, start, angle, true);
        g.setGradientFill (gradient (acc, r));
        g.setOpacity (active ? 1.0f : 0.3f);
        g.strokePath (val, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setOpacity (1.0f);

        const float inner = rad * 0.62f;
        if (active) drawGlow (g, c, inner * 1.6f, acc.b, 0.18f);
        juce::ColourGradient knob (juce::Colour (0xff2c2160), c.x, c.y - inner, juce::Colour (0xff140d33), c.x, c.y + inner, false);
        g.setGradientFill (knob);
        g.fillEllipse (c.x - inner, c.y - inner, inner * 2, inner * 2);
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawEllipse (c.x - inner, c.y - inner, inner * 2, inner * 2, 1.0f);

        const juce::Point<float> tip (c.x + std::sin (angle) * inner * 0.72f, c.y - std::cos (angle) * inner * 0.72f);
        g.setColour (active ? text : textDim.withAlpha (0.5f));
        g.fillEllipse (tip.x - 3, tip.y - 3, 6, 6);
    }

    juce::Font getLabelFont (juce::Label&) override { return font (13.0f); }
};
} // namespace theme
