#include "Widgets.h"

namespace ui
{
void setKnobAccent (juce::Slider& s, theme::Accent ac)
{
    s.getProperties().set ("accentA", (juce::int64) ac.a.getARGB());
    s.getProperties().set ("accentB", (juce::int64) ac.b.getARGB());
}

static juce::Path sparkle (juce::Point<float> c, float r)
{
    juce::Path p;
    p.startNewSubPath (c.x, c.y - r);
    p.quadraticTo (c, { c.x + r, c.y });
    p.quadraticTo (c, { c.x, c.y + r });
    p.quadraticTo (c, { c.x - r, c.y });
    p.quadraticTo (c, { c.x, c.y - r });
    p.closeSubPath();
    return p;
}

void drawIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> r, juce::Colour col, float stroke)
{
    const float s = juce::jmin (r.getWidth(), r.getHeight());
    r = r.withSizeKeepingCentre (s, s);
    const float x = r.getX(), y = r.getY();
    auto P = [&] (float fx, float fy) { return juce::Point<float> (x + fx * s, y + fy * s); };
    const juce::PathStrokeType st (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (col);
    juce::Path p;

    switch (icon)
    {
        case Icon::DrumForge:
        {
            p.addEllipse (x + 0.14f * s, y + 0.38f * s, 0.72f * s, 0.2f * s);
            p.startNewSubPath (P (0.14f, 0.48f)); p.lineTo (P (0.14f, 0.74f));
            p.startNewSubPath (P (0.86f, 0.48f)); p.lineTo (P (0.86f, 0.74f));
            p.addCentredArc (x + 0.5f * s, y + 0.74f * s, 0.36f * s, 0.1f * s, 0, juce::MathConstants<float>::halfPi,
                             juce::MathConstants<float>::halfPi * 3.0f, true);
            p.startNewSubPath (P (0.3f, 0.58f)); p.lineTo (P (0.3f, 0.82f));
            p.startNewSubPath (P (0.5f, 0.58f)); p.lineTo (P (0.5f, 0.85f));
            p.startNewSubPath (P (0.7f, 0.58f)); p.lineTo (P (0.7f, 0.82f));
            p.startNewSubPath (P (0.22f, 0.12f)); p.lineTo (P (0.48f, 0.44f));
            p.startNewSubPath (P (0.78f, 0.12f)); p.lineTo (P (0.52f, 0.44f));
            g.strokePath (p, st);
            g.fillEllipse (P (0.18f, 0.08f).x, P (0.18f, 0.08f).y, 0.08f * s, 0.08f * s);
            g.fillEllipse (P (0.74f, 0.08f).x, P (0.74f, 0.08f).y, 0.08f * s, 0.08f * s);
            break;
        }
        case Icon::BeatDoctor:
        {
            p.startNewSubPath (P (0.05f, 0.55f));
            p.lineTo (P (0.28f, 0.55f)); p.lineTo (P (0.36f, 0.35f)); p.lineTo (P (0.46f, 0.8f));
            p.lineTo (P (0.56f, 0.18f)); p.lineTo (P (0.66f, 0.62f)); p.lineTo (P (0.72f, 0.55f)); p.lineTo (P (0.95f, 0.55f));
            g.strokePath (p, st);
            juce::Path plus;
            plus.startNewSubPath (P (0.82f, 0.14f)); plus.lineTo (P (0.82f, 0.34f));
            plus.startNewSubPath (P (0.72f, 0.24f)); plus.lineTo (P (0.92f, 0.24f));
            g.strokePath (plus, st);
            break;
        }
        case Icon::VibeMatch:
        {
            const auto c = r.getCentre();
            const float rad = 0.32f * s;
            p.addCentredArc (c.x, c.y, rad, rad, 0, 0.35f, 2.8f, true);
            p.startNewSubPath (c.x + rad * std::sin (3.5f), c.y - rad * std::cos (3.5f));
            p.addCentredArc (c.x, c.y, rad, rad, 0, 3.5f, 5.95f, false);
            g.strokePath (p, st);
            auto arrow = [&] (float a, float dir)
            {
                const juce::Point<float> tip (c.x + rad * std::sin (a), c.y - rad * std::cos (a));
                const float t = a + dir * juce::MathConstants<float>::halfPi;
                juce::Path h;
                h.startNewSubPath (tip.x + 0.1f * s * std::sin (t - 0.9f), tip.y - 0.1f * s * std::cos (t - 0.9f));
                h.lineTo (tip);
                h.lineTo (tip.x + 0.1f * s * std::sin (t + 0.9f), tip.y - 0.1f * s * std::cos (t + 0.9f));
                g.strokePath (h, st);
            };
            arrow (2.8f, -1.0f);
            arrow (5.95f, -1.0f);
            g.fillPath (sparkle (c, 0.1f * s));
            break;
        }
        case Icon::FXHeaven:
            g.fillPath (sparkle (P (0.42f, 0.55f), 0.34f * s));
            g.fillPath (sparkle (P (0.78f, 0.22f), 0.14f * s));
            g.fillPath (sparkle (P (0.8f, 0.8f), 0.08f * s));
            break;
        case Icon::Back:
            p.startNewSubPath (P (0.62f, 0.2f)); p.lineTo (P (0.32f, 0.5f)); p.lineTo (P (0.62f, 0.8f));
            g.strokePath (p, st);
            break;
        case Icon::Play:
            p.addTriangle (P (0.3f, 0.2f), P (0.3f, 0.8f), P (0.82f, 0.5f));
            g.fillPath (p);
            break;
        case Icon::Stop:
            g.fillRoundedRectangle (juce::Rectangle<float> (P (0.25f, 0.25f), P (0.75f, 0.75f)), 0.06f * s);
            break;
        case Icon::Dice:
            g.drawRoundedRectangle (juce::Rectangle<float> (P (0.15f, 0.15f), P (0.85f, 0.85f)), 0.16f * s, stroke);
            for (auto pt : { P (0.34f, 0.34f), P (0.5f, 0.5f), P (0.66f, 0.66f) })
                g.fillEllipse (pt.x - 0.07f * s, pt.y - 0.07f * s, 0.14f * s, 0.14f * s);
            break;
        case Icon::Folder:
            p.startNewSubPath (P (0.1f, 0.3f)); p.lineTo (P (0.38f, 0.3f)); p.lineTo (P (0.46f, 0.38f));
            p.lineTo (P (0.9f, 0.38f)); p.lineTo (P (0.9f, 0.8f)); p.lineTo (P (0.1f, 0.8f)); p.closeSubPath();
            g.strokePath (p, st);
            break;
        case Icon::Midi:
            g.drawRoundedRectangle (juce::Rectangle<float> (P (0.1f, 0.2f), P (0.9f, 0.8f)), 0.06f * s, stroke);
            for (float fx : { 0.3f, 0.5f, 0.7f })
            {
                p.startNewSubPath (P (fx, 0.55f)); p.lineTo (P (fx, 0.8f));
                g.fillRect (juce::Rectangle<float> (P (fx - 0.05f, 0.2f), P (fx + 0.05f, 0.55f)));
            }
            g.strokePath (p, st);
            break;
    }
}

void IconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colours::white.withAlpha (over ? 0.16f : 0.08f));
    g.fillEllipse (r);
    if (over) theme::drawGlow (g, r.getCentre(), r.getWidth() * 0.8f, accent.b, 0.25f);
    drawIcon (g, icon, r.reduced (r.getWidth() * 0.26f), down ? accent.a : theme::text, 1.8f);
}

// ---------------- SoundTile ----------------
SoundTile::SoundTile (ProducersHeavenProcessor& p, theme::Accent ac) : proc (p), accent (ac), dice (Icon::Dice, ac)
{
    addChildComponent (dice);
    dice.setTooltip ("Generate a new one");
    dice.onClick = [this] { if (onReroll) onReroll(); };
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("Click to listen - drag into FL Studio");
}

void SoundTile::setSound (SharedBuffer b, const juce::String& name, const juce::String& label, const juce::String& sub, bool loopPreview)
{
    buffer = std::move (b);
    soundName = name;
    labelText = label;
    subfolder = sub;
    loop = loopPreview;
    file = juce::File();
    midiFile = juce::File();
    peaks.assign (72, 0.0f);
    if (buffer != nullptr && buffer->getNumSamples() > 0)
    {
        const int n = buffer->getNumSamples();
        for (int i = 0; i < (int) peaks.size(); ++i)
        {
            const int s0 = i * n / (int) peaks.size(), s1 = juce::jmax (s0 + 1, (i + 1) * n / (int) peaks.size());
            float pk = 0;
            for (int ch = 0; ch < buffer->getNumChannels(); ++ch)
                pk = juce::jmax (pk, buffer->getMagnitude (ch, s0, s1 - s0));
            peaks[(size_t) i] = pk;
        }
    }
    dice.setVisible (onReroll != nullptr);
    repaint();
}

void SoundTile::setMidiFile (const juce::File& midi, const juce::String& name)
{
    buffer.reset();
    peaks.clear();
    midiFile = midi;
    soundName = name;
    labelText = "MIDI";
    repaint();
}

void SoundTile::clear()
{
    buffer.reset();
    midiFile = juce::File();
    file = juce::File();
    peaks.clear();
    dice.setVisible (false);
    repaint();
}

juce::File SoundTile::ensureFile()
{
    if (midiFile.existsAsFile()) return midiFile;
    if (buffer == nullptr) return {};
    if (! file.existsAsFile())
    {
        file = ProducersHeavenProcessor::uniqueFile (subfolder, soundName, ".wav");
        if (! ProducersHeavenProcessor::writeWav (*buffer, proc.getRenderSampleRate(), file))
            file = juce::File();
    }
    return file;
}

void SoundTile::resized()
{
    dice.setBounds (getWidth() - 34, 8, 26, 26);
}

void SoundTile::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    if (! hasSound())
    {
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.fillRoundedRectangle (r, 14.0f);
        juce::Path outline;
        outline.addRoundedRectangle (r, 14.0f);
        juce::Path dashed;
        const float dashes[] = { 5.0f, 5.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);
        g.setColour (juce::Colours::white.withAlpha (0.2f));
        g.fillPath (dashed);
        g.setColour (theme::textDim);
        g.setFont (theme::font (13.0f));
        g.drawFittedText (emptyText, r.reduced (10).toNearestInt(), juce::Justification::centred, 3);
        return;
    }

    if (hover) theme::drawGlow (g, r.getCentre(), r.getWidth() * 0.6f, accent.b, 0.16f);
    juce::ColourGradient tint (accent.a.withAlpha (hover ? 0.28f : 0.18f), r.getX(), r.getY(),
                               accent.b.withAlpha (hover ? 0.22f : 0.1f), r.getRight(), r.getBottom(), false);
    g.setGradientFill (tint);
    g.fillRoundedRectangle (r, 14.0f);
    g.setColour (juce::Colours::white.withAlpha (hover ? 0.35f : 0.16f));
    g.drawRoundedRectangle (r, 14.0f, 1.0f);

    auto content = r.reduced (12.0f, 10.0f);
    g.setColour (accent.a.brighter (0.3f));
    g.setFont (theme::font (11.0f, true));
    g.drawText (labelText.toUpperCase(), content.removeFromTop (14.0f), juce::Justification::topLeft);
    g.setColour (theme::text);
    g.setFont (theme::font (14.0f, true));
    g.drawFittedText (soundName, content.removeFromTop (20.0f).withTrimmedRight (dice.isVisible() ? 26.0f : 0.0f).toNearestInt(),
                      juce::Justification::topLeft, 1);

    auto wave = content.withTrimmedBottom (14.0f).reduced (0, 4);
    if (midiFile.existsAsFile())
        drawIcon (g, Icon::Midi, wave.withSizeKeepingCentre (wave.getHeight(), wave.getHeight()), theme::text.withAlpha (0.8f), 1.6f);
    else if (! peaks.empty() && wave.getHeight() > 6)
    {
        const float bw = wave.getWidth() / (float) peaks.size();
        g.setGradientFill (theme::gradient (accent, wave));
        for (size_t i = 0; i < peaks.size(); ++i)
        {
            const float h = juce::jmax (1.5f, peaks[i] * wave.getHeight());
            g.fillRoundedRectangle (wave.getX() + (float) i * bw, wave.getCentreY() - h * 0.5f, juce::jmax (1.0f, bw - 1.5f), h, 1.0f);
        }
    }

    g.setColour (theme::textDim.withAlpha (hover ? 0.95f : 0.6f));
    g.setFont (theme::font (10.5f));
    const juce::String hint = midiFile.existsAsFile() ? "drag into the piano roll"
                            : loop ? (proc.isLoopPlaying() ? "click to stop  -  drag to FL" : "click to play  -  drag to FL")
                                   : "click to hear  -  drag to FL";
    g.drawText (hint, content.removeFromBottom (14.0f), juce::Justification::bottomLeft);

    if (flash > 0.0f)
    {
        g.setColour (juce::Colours::white.withAlpha (0.18f * flash));
        g.fillRoundedRectangle (r, 14.0f);
    }
}

void SoundTile::mouseDrag (const juce::MouseEvent& e)
{
    if (dragStarted || ! hasSound() || e.getDistanceFromDragStart() < 6) return;
    dragStarted = true;
    const auto f = ensureFile();
    if (f.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
}

void SoundTile::mouseUp (const juce::MouseEvent& e)
{
    if (dragStarted || ! e.mouseWasClicked() || buffer == nullptr) return;
    if (loop && proc.isLoopPlaying()) proc.stopPreview();
    else { proc.preview (buffer, loop); if (onPreview) onPreview(); }
    flash = 1.0f;
    repaint();
    juce::Component::SafePointer<SoundTile> safe (this);
    juce::Timer::callAfterDelay (140, [safe] { if (safe) { safe->flash = 0.0f; safe->repaint(); } });
}

// ---------------- StepGrid ----------------
void StepGrid::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    theme::drawGlass (g, r, 16.0f);
    r.reduce (14.0f, 12.0f);
    if (! has)
    {
        g.setColour (theme::textDim);
        g.setFont (theme::font (14.0f));
        g.drawText ("Your generated pattern shows up here", r, juce::Justification::centred);
        return;
    }

    static const ph::DrumType order[] = { ph::DrumType::Kick, ph::DrumType::Bass808, ph::DrumType::Snare, ph::DrumType::Clap,
                                          ph::DrumType::HiHat, ph::DrumType::OpenHat, ph::DrumType::Perc, ph::DrumType::Rim };
    const juce::Colour rowCol[] = { juce::Colour (0xffff8a5b), juce::Colour (0xffff5e8a), juce::Colour (0xff3ee6ff), juce::Colour (0xff5a9bff),
                                    juce::Colour (0xffffd36e), juce::Colour (0xfff5b04a), juce::Colour (0xffb06bff), juce::Colour (0xff5cffc8) };
    const float labelW = 64.0f;
    const int steps = pattern.totalSteps();
    const float rowH = r.getHeight() / 8.0f;
    const float colW = (r.getWidth() - labelW) / (float) juce::jmax (1, steps);

    for (int row = 0; row < 8; ++row)
    {
        const auto type = order[row];
        auto rowR = juce::Rectangle<float> (r.getX(), r.getY() + row * rowH, r.getWidth(), rowH);
        g.setColour (theme::textDim);
        g.setFont (theme::font (11.0f, true));
        g.drawText (ph::DrumSynth::typeName (type), rowR.withWidth (labelW - 6), juce::Justification::centredLeft);
        const auto& hits = pattern.hits[(size_t) type];
        for (int s = 0; s < steps; ++s)
        {
            auto cell = juce::Rectangle<float> (r.getX() + labelW + s * colW, rowR.getY(), colW, rowH).reduced (1.2f, 2.0f);
            const float v = s < (int) hits.size() ? hits[(size_t) s] : 0.0f;
            const bool beat = s % 4 == 0;
            if (v > 0)
            {
                g.setColour (rowCol[row].withAlpha (0.35f + 0.65f * v));
                g.fillRoundedRectangle (cell, 3.0f);
                if (type == ph::DrumType::HiHat && pattern.hatDivision[(size_t) s] > 1)
                {
                    g.setColour (juce::Colours::white);
                    const int d = pattern.hatDivision[(size_t) s];
                    for (int k = 0; k < d; ++k)
                        g.fillEllipse (cell.getX() + cell.getWidth() * (k + 0.5f) / d - 1.5f, cell.getBottom() - 5.0f, 3.0f, 3.0f);
                }
            }
            else
            {
                g.setColour (juce::Colours::white.withAlpha (beat ? 0.1f : 0.05f));
                g.fillRoundedRectangle (cell, 3.0f);
            }
        }
    }
    for (int bar = 1; bar < pattern.bars; ++bar)
    {
        const float xb = r.getX() + labelW + bar * ph::kStepsPerBar * colW;
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawVerticalLine ((int) xb, r.getY(), r.getBottom());
    }
    if (playhead >= 0.0f)
    {
        const float xp = r.getX() + labelW + playhead * (r.getWidth() - labelW);
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.fillRect (xp - 1.0f, r.getY(), 2.0f, r.getHeight());
    }
}

// ---------------- ScoreRing ----------------
void ScoreRing::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto ringArea = r.removeFromTop (r.getHeight() - 18.0f);
    const float size = juce::jmin (ringArea.getWidth(), ringArea.getHeight()) - 6.0f;
    auto ring = ringArea.withSizeKeepingCentre (size, size);
    const auto c = ring.getCentre();
    const float rad = size * 0.5f - 3.0f;
    const float start = -juce::MathConstants<float>::pi * 0.75f, end = juce::MathConstants<float>::pi * 0.75f;

    juce::Path bg;
    bg.addCentredArc (c.x, c.y, rad, rad, 0, start, end, true);
    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.strokePath (bg, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (known && shown > 0.5f)
    {
        juce::Path v;
        v.addCentredArc (c.x, c.y, rad, rad, 0, start, start + (end - start) * shown / 100.0f, true);
        g.setGradientFill (theme::gradient (accent, ring));
        g.strokePath (v, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (theme::text);
    g.setFont (theme::font (size * 0.3f, true));
    g.drawText (known ? juce::String (juce::roundToInt (shown)) : "--", ring, juce::Justification::centred);
    g.setColour (theme::textDim);
    g.setFont (theme::font (11.5f, true));
    g.drawText (name, r, juce::Justification::centred);
}

// ---------------- PowerToggle ----------------
void PowerToggle::paintButton (juce::Graphics& g, bool over, bool)
{
    auto r = getLocalBounds().toFloat().withSizeKeepingCentre (42.0f, 22.0f);
    const bool on = getToggleState();
    if (on)
    {
        theme::drawGlow (g, r.getCentre(), 34.0f, accent.b, over ? 0.45f : 0.3f);
        g.setGradientFill (theme::gradient (accent, r));
        g.fillRoundedRectangle (r, 11.0f);
    }
    else
    {
        g.setColour (juce::Colours::white.withAlpha (over ? 0.16f : 0.1f));
        g.fillRoundedRectangle (r, 11.0f);
    }
    const float kx = on ? r.getRight() - 19.0f : r.getX() + 3.0f;
    g.setColour (on ? juce::Colours::white : theme::textDim);
    g.fillEllipse (kx, r.getY() + 3.0f, 16.0f, 16.0f);
}
} // namespace ui
