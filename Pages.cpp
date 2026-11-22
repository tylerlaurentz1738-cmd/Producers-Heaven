#include "Pages.h"

namespace
{
std::mt19937 makeRng() { return std::mt19937 ((unsigned) juce::Time::currentTimeMillis() ^ (unsigned) juce::Random::getSystemRandom().nextInt()); }

void drawLabel (juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r, float size = 12.0f,
                juce::Justification j = juce::Justification::centredLeft, juce::Colour c = theme::textDim)
{
    g.setColour (c);
    g.setFont (theme::font (size, true).withExtraKerningFactor (0.08f));
    g.drawText (t, r, j);
}
} // namespace

// =====================================================================================
Page::Page (const juce::String& t, const juce::String& s, theme::Accent ac, ui::Icon i)
    : title (t), subtitle (s), accent (ac), icon (i), back (ui::Icon::Back, ac)
{
    addAndMakeVisible (back);
    back.setTooltip ("Back to home");
    back.onClick = [this] { if (onBack) onBack(); };
}

void Page::resized()
{
    back.setBounds (24, 24, 40, 40);
    layout (contentArea());
}

void Page::paint (juce::Graphics& g)
{
    const juce::Point<float> ic (100.0f, 44.0f);
    theme::drawGlow (g, ic, 48.0f, accent.b, 0.35f);
    juce::ColourGradient sphere (accent.a, ic.x - 18, ic.y - 18, accent.b, ic.x + 18, ic.y + 18, false);
    g.setGradientFill (sphere);
    g.fillEllipse (ic.x - 20, ic.y - 20, 40, 40);
    ui::drawIcon (g, icon, { ic.x - 11, ic.y - 11, 22, 22 }, juce::Colours::white, 1.8f);

    g.setColour (theme::text);
    g.setFont (theme::font (28.0f, true));
    g.drawText (title, 134, 18, 500, 34, juce::Justification::centredLeft);
    g.setColour (theme::textDim);
    g.setFont (theme::font (14.0f));
    g.drawText (subtitle, 134, 50, getWidth() - 160, 20, juce::Justification::centredLeft);

    juce::ColourGradient line (accent.a.withAlpha (0.5f), 28.0f, 0, juce::Colours::transparentWhite, (float) getWidth() - 28.0f, 0, false);
    g.setGradientFill (line);
    g.fillRect (28.0f, (float) headerH - 4.0f, (float) getWidth() - 56.0f, 1.0f);
}

void Page::drawHint (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text)
{
    g.setColour (theme::textDim.withAlpha (0.8f));
    g.setFont (theme::font (12.5f));
    g.drawFittedText (text, r, juce::Justification::centred, 2);
}

// =====================================================================================
// DRUM FORGE
// =====================================================================================
DrumForgePage::DrumForgePage (ProducersHeavenProcessor& p)
    : Page ("Drum Forge", "Fresh drums generated on the spot. Click a pad to hear it, drag it straight into FL Studio.",
            theme::forge, ui::Icon::DrumForge),
      proc (p), rng (makeRng())
{
    for (int s = 0; s < ph::kNumStyles; ++s)
    {
        auto* b = styleButtons.add (new theme::PillButton (ph::DrumSynth::styleName ((ph::Style) s), theme::forge, false));
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1001);
        b->setToggleState (s == 0, juce::dontSendNotification);
        b->onClick = [this, s, b] { if (b->getToggleState() && style != (ph::Style) s) { style = (ph::Style) s; generateAll(); } };
        addAndMakeVisible (b);
    }
    generate.onClick = [this] { generateAll(); };
    generate.setTooltip ("Roll a whole new kit in the selected vibe");
    save.onClick = [this] { saveKit(); };
    save.setTooltip ("Save all 8 sounds as WAVs into Documents/Producers Heaven/Kits");
    addAndMakeVisible (generate);
    addAndMakeVisible (save);

    for (int t = 0; t < ph::kNumDrumTypes; ++t)
    {
        auto* tile = tiles.add (new ui::SoundTile (proc, theme::forge));
        tile->onReroll = [this, t] { generateOne ((ph::DrumType) t); };
        addAndMakeVisible (tile);
    }
    generateAll();
}

void DrumForgePage::generateOne (ph::DrumType t)
{
    auto snd = ph::DrumSynth::generate (t, style, rng, proc.getRenderSampleRate());
    auto buf = ProducersHeavenProcessor::toBuffer (snd.samples);
    tiles[(int) t]->setSound (buf, snd.name, ph::DrumSynth::typeName (t), "Drum Forge");
    proc.setPad (t, buf);
}

void DrumForgePage::generateAll()
{
    for (int t = 0; t < ph::kNumDrumTypes; ++t) generateOne ((ph::DrumType) t);
    status = juce::String (ph::DrumSynth::styleName (style)) + " kit ready. Hit the dice on any pad to re-roll just that sound.";
    repaint();
}

void DrumForgePage::saveKit()
{
    auto dir = ProducersHeavenProcessor::exportFolder ("Kits").getNonexistentChildFile (
        juce::String ("Heaven ") + ph::DrumSynth::styleName (style) + " Kit", "", false);
    dir.createDirectory();
    int i = 1;
    for (auto* tile : tiles)
    {
        if (auto buf = tile->getBuffer())
            ProducersHeavenProcessor::writeWav (*buf, proc.getRenderSampleRate(),
                                                dir.getChildFile (juce::String (i).paddedLeft ('0', 2) + " " +
                                                                  juce::File::createLegalFileName (tile->getSoundName()) + ".wav"));
        ++i;
    }
    status = "Saved to " + dir.getFullPathName();
    dir.revealToUser();
    repaint();
}

void DrumForgePage::layout (juce::Rectangle<int> c)
{
    auto row = c.removeFromTop (40);
    row.removeFromLeft (52); // "VIBE" label
    for (auto* b : styleButtons) { b->setBounds (row.removeFromLeft (100).reduced (0, 2)); row.removeFromLeft (8); }
    generate.setBounds (row.removeFromRight (170));
    row.removeFromRight (10);
    save.setBounds (row.removeFromRight (110).reduced (0, 2));

    c.removeFromTop (20);
    hintArea = c.removeFromBottom (40);
    c.removeFromBottom (8);
    const int gap = 14;
    const int w = (c.getWidth() - gap * 3) / 4, h = (c.getHeight() - gap) / 2;
    for (int i = 0; i < tiles.size(); ++i)
        tiles[i]->setBounds (c.getX() + (i % 4) * (w + gap), c.getY() + (i / 4) * (h + gap), w, h);
}

void DrumForgePage::paint (juce::Graphics& g)
{
    Page::paint (g);
    auto c = contentArea();
    drawLabel (g, "VIBE", c.removeFromTop (40).removeFromLeft (52));
    drawHint (g, hintArea, status + "\nAlso playable from MIDI: notes 35-47 (standard drum map) trigger this kit.");
}

// =====================================================================================
// BEAT DOCTOR
// =====================================================================================
void ListenOrb::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const auto c = r.getCentre();
    const float base = juce::jmin (r.getWidth(), r.getHeight()) * 0.33f;
    const auto ac = theme::doctor;
    const bool listeningNow = state == State::Listening;
    const float pulse = listeningNow ? 1.0f + juce::jmin (0.18f, level * 2.5f) : 1.0f + 0.03f * std::sin (time * 2.0f);
    const float rad = base * pulse;

    if (listeningNow)
        for (int k = 0; k < 3; ++k)
        {
            const float wave = std::fmod (time * 0.6f + (float) k / 3.0f, 1.0f);
            g.setColour (ac.a.withAlpha (0.35f * (1.0f - wave)));
            const float rr = rad * (1.0f + wave * 0.6f);
            g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 2.0f);
        }

    theme::drawGlow (g, c, juce::jmin (rad * 1.9f, juce::jmin (r.getWidth(), r.getHeight()) * 0.5f), ac.b, 0.35f);
    juce::ColourGradient sphere (ac.a, c.x - rad * 0.6f, c.y - rad * 0.7f, ac.b.darker (0.3f), c.x + rad * 0.7f, c.y + rad * 0.8f, false);
    g.setGradientFill (sphere);
    g.fillEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2);
    juce::ColourGradient shine (juce::Colours::white.withAlpha (0.5f), c.x, c.y - rad, juce::Colours::transparentWhite, c.x, c.y, false);
    g.setGradientFill (shine);
    g.fillEllipse (c.x - rad * 0.7f, c.y - rad * 0.92f, rad * 1.4f, rad * 0.9f);

    // progress ring
    const float ringR = base * 1.32f;
    juce::Path bg;
    bg.addCentredArc (c.x, c.y, ringR, ringR, 0, 0, juce::MathConstants<float>::twoPi, true);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.strokePath (bg, juce::PathStrokeType (4.0f));
    if (state != State::Idle)
    {
        juce::Path arc;
        const float a0 = state == State::Analyzing ? time * 4.0f : 0.0f;
        const float a1 = state == State::Analyzing ? a0 + 1.2f : progress * juce::MathConstants<float>::twoPi;
        arc.addCentredArc (c.x, c.y, ringR, ringR, 0, a0, a1, true);
        g.setColour (ac.a);
        g.strokePath (arc, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setColour (juce::Colours::white);
    g.setFont (theme::font (22.0f, true).withExtraKerningFactor (0.12f));
    const juce::String label = state == State::Idle ? "LISTEN" : state == State::Listening ? "STOP" : "...";
    g.drawText (label, r, juce::Justification::centred);
}

TipCard::TipCard (ProducersHeavenProcessor& p, const ph::Tip& t, ph::Style s) : proc (p), tip (t), style (s), rng (makeRng())
{
    if (tip.hasReplacement)
    {
        fixButton = std::make_unique<theme::PillButton> ("Generate: " + juce::String (tip.replaceLabel), theme::doctor, false);
        fixButton->onClick = [this] { makeSound(); };
        addAndMakeVisible (*fixButton);
    }
}

void TipCard::makeSound()
{
    auto snd = ph::BeatAnalyzer::makeReplacement (tip, style, rng, proc.getRenderSampleRate());
    auto buf = ProducersHeavenProcessor::toBuffer (snd.samples);
    if (tile == nullptr)
    {
        tile = std::make_unique<ui::SoundTile> (proc, theme::doctor);
        tile->onReroll = [this] { makeSound(); };
        addAndMakeVisible (*tile);
        fixButton->setVisible (false);
    }
    tile->setSound (buf, snd.name, tip.replaceLabel, "Beat Doctor");
    proc.preview (buf);
    resized();
}

static juce::AttributedString tipText (const ph::Tip& t)
{
    juce::AttributedString s;
    s.append (juce::String (t.title) + "\n", theme::font (16.0f, true), theme::text);
    s.append (juce::String (t.detail), theme::font (13.5f), theme::textDim);
    s.setWordWrap (juce::AttributedString::byWord);
    s.setLineSpacing (2.0f);
    return s;
}

int TipCard::heightForWidth (int w)
{
    const int textW = w - 52 - (tip.hasReplacement ? 236 : 0);
    juce::TextLayout tl;
    tl.createLayout (tipText (tip), (float) juce::jmax (100, textW));
    textH = (int) std::ceil (tl.getHeight());
    return juce::jmax (textH + 30, tip.hasReplacement ? 124 : 0);
}

void TipCard::resized()
{
    if (! tip.hasReplacement) return;
    auto right = getLocalBounds().removeFromRight (236).reduced (12, 10);
    if (tile) tile->setBounds (right);
    else if (fixButton) fixButton->setBounds (right.withSizeKeepingCentre (right.getWidth(), 38));
}

void TipCard::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    theme::drawGlass (g, r, 16.0f);
    const juce::Colour sev = tip.severity == ph::Tip::Important ? juce::Colour (0xffff6b8b)
                           : tip.severity == ph::Tip::Suggestion ? juce::Colour (0xffffd36e) : juce::Colour (0xff5cffc8);
    const juce::Point<float> dot (24.0f, 25.0f);
    theme::drawGlow (g, dot, 16.0f, sev, 0.5f);
    g.setColour (sev);
    g.fillEllipse (dot.x - 5, dot.y - 5, 10, 10);

    juce::TextLayout tl;
    const float textW = r.getWidth() - 52 - (tip.hasReplacement ? 236 : 0);
    tl.createLayout (tipText (tip), juce::jmax (100.0f, textW));
    tl.draw (g, { 42.0f, 15.0f, textW, (float) textH + 4 });
}

BeatDoctorPage::BeatDoctorPage (ProducersHeavenProcessor& p)
    : Page ("Beat Doctor", "I listen to your beat, then tell you what to fix and hand you better drum sounds.",
            theme::doctor, ui::Icon::BeatDoctor),
      proc (p)
{
    addAndMakeVisible (orb);
    orb.onClick = [this] { toggleListen(); };
    orb.setMouseCursor (juce::MouseCursor::PointingHandCursor);

    for (auto* n : { "PUNCH", "LOW END", "BRIGHTNESS", "DYNAMICS", "WIDTH" })
        addAndMakeVisible (rings.add (new ui::ScoreRing (n, theme::doctor)));

    tipsView.setViewedComponent (&tipsContent, false);
    tipsView.setScrollBarsShown (true, false);
    tipsView.setScrollBarThickness (8);
    addAndMakeVisible (tipsView);

    status = "1. Put Producers Heaven on your Master track.\n2. Press play in FL.\n3. Hit LISTEN (I'll listen for up to 20 seconds).";
}

BeatDoctorPage::~BeatDoctorPage() { alive->store (false); }

void BeatDoctorPage::toggleListen()
{
    if (orb.state == ListenOrb::State::Analyzing) return;
    if (orb.state == ListenOrb::State::Listening) { finishListening(); return; }

    proc.startListening();
    if (! proc.isListening())
    {
        status = "I'm not running yet - load me on a mixer track in FL and press play.";
        repaint();
        return;
    }
    orb.state = ListenOrb::State::Listening;
    listenStart = juce::Time::getMillisecondCounterHiRes();
    cards.clear();
    for (auto* r : rings) r->reset();
    bpmText = {};
    layoutTips();
}

void BeatDoctorPage::finishListening()
{
    std::vector<float> l, r;
    proc.takeRecording (l, r);
    orb.state = ListenOrb::State::Analyzing;
    status = "Analyzing your beat...";
    repaint();

    const double sr = proc.getRenderSampleRate();
    juce::Component::SafePointer<BeatDoctorPage> safe (this);
    juce::Thread::launch ([safe, sr, left = std::move (l), right = std::move (r)]
    {
        auto report = ph::BeatAnalyzer::analyze (left, right, sr);
        juce::MessageManager::callAsync ([safe, report] { if (safe) safe->showReport (report); });
    });
}

void BeatDoctorPage::showReport (const ph::BeatReport& rep)
{
    orb.state = ListenOrb::State::Idle;
    orb.progress = 0;
    if (! rep.valid)
    {
        status = rep.error;
        repaint();
        return;
    }
    const auto& m = rep.metrics;
    bpmText = juce::String (juce::roundToInt (m.bpm));
    const float scores[] = { m.punch, m.lowEnd, m.brightness, m.dynamics, m.width };
    for (int i = 0; i < rings.size(); ++i) rings[i]->setScore (scores[i]);

    int important = 0;
    for (auto& t : rep.tips) important += t.severity == ph::Tip::Important;
    status = "Heard " + juce::String (m.seconds, 1) + "s of your beat: " + juce::String (m.kicks) + " kicks, "
           + juce::String (m.snares) + " snares/claps, " + juce::String (m.hats) + " hats.\n"
           + (important > 0 ? juce::String (important) + " thing(s) worth fixing first." : juce::String ("Nothing urgent - just polish."))
           + "\nHit LISTEN again after you make changes.";

    const ph::Style style = m.bpm >= 125 ? ph::Style::Trap : ph::Style::BoomBap;
    cards.clear();
    for (auto& t : rep.tips) tipsContent.addAndMakeVisible (cards.add (new TipCard (proc, t, style)));
    layoutTips();
    repaint();
}

void BeatDoctorPage::layoutTips()
{
    const int w = juce::jmax (200, tipsView.getWidth() - 12);
    int y = 0;
    for (auto* c : cards)
    {
        const int h = c->heightForWidth (w);
        c->setBounds (0, y, w, h);
        y += h + 12;
    }
    tipsContent.setSize (w, juce::jmax (1, y));
}

void BeatDoctorPage::tick()
{
    orb.time += 1.0f / 60.0f;
    if (orb.state == ListenOrb::State::Listening)
    {
        orb.progress = proc.getListenProgress();
        orb.level += (proc.getListenLevel() - orb.level) * 0.3f;
        const double secs = (juce::Time::getMillisecondCounterHiRes() - listenStart) / 1000.0;
        status = (secs > 2.5 && orb.level < 0.0005f)
                     ? juce::String ("I don't hear anything yet - is FL playing, and am I on the track your beat goes through?")
                     : "Listening... " + juce::String (secs, 1) + "s\nLet it play for at least 8 seconds, then hit STOP.";
        if (! proc.isListening()) finishListening();
        repaint (statusArea);
    }
    for (auto* r : rings) r->tick();
    orb.repaint();
}

void BeatDoctorPage::layout (juce::Rectangle<int> c)
{
    auto left = c.removeFromLeft (300);
    orb.setBounds (left.removeFromTop (290));
    statusArea = left.reduced (6, 0);
    c.removeFromLeft (24);

    metricsArea = c.removeFromTop (112);
    auto m = metricsArea.reduced (12, 8);
    m.removeFromLeft (110);
    const int rw = m.getWidth() / juce::jmax (1, rings.size());
    for (auto* r : rings) r->setBounds (m.removeFromLeft (rw).reduced (6, 0));

    c.removeFromTop (16);
    listArea = c;
    tipsView.setBounds (c);
    layoutTips();
}

void BeatDoctorPage::paint (juce::Graphics& g)
{
    Page::paint (g);
    g.setColour (theme::text.withAlpha (0.9f));
    g.setFont (theme::font (14.0f));
    g.drawFittedText (status, statusArea, juce::Justification::centredTop, 6);

    theme::drawGlass (g, metricsArea.toFloat(), 18.0f);
    auto bpm = metricsArea.reduced (12, 8).removeFromLeft (110);
    g.setColour (theme::text);
    g.setFont (theme::font (36.0f, true));
    g.drawText (bpmText.isEmpty() ? "--" : bpmText, bpm.withTrimmedBottom (24), juce::Justification::centred);
    drawLabel (g, "BPM", bpm.removeFromBottom (24), 11.5f, juce::Justification::centred);

    if (cards.isEmpty())
    {
        theme::drawGlass (g, listArea.toFloat(), 18.0f, 0.6f);
        g.setColour (theme::textDim);
        g.setFont (theme::font (15.0f));
        g.drawFittedText ("Your tips and replacement sounds will show up here.", listArea.reduced (20), juce::Justification::centred, 2);
    }
}

// =====================================================================================
// VIBE MATCH
// =====================================================================================
VibeMatchPage::VibeMatchPage (ProducersHeavenProcessor& p)
    : Page ("Vibe Match", "Drop in a beat. I catch its tempo and groove, then cook up a fresh drum loop in the same lane.",
            theme::vibe, ui::Icon::VibeMatch),
      proc (p), rng (makeRng()), loopTile (p, theme::vibe), midiTile (p, theme::vibe)
{
    const int barOpts[] = { 1, 2, 4 };
    for (int b : barOpts)
    {
        auto* btn = barButtons.add (new theme::PillButton (juce::String (b) + (b == 1 ? " Bar" : " Bars"), theme::vibe, false));
        btn->setClickingTogglesState (true);
        btn->setRadioGroupId (2002);
        btn->setToggleState (b == bars, juce::dontSendNotification);
        btn->onClick = [this, b, btn] { if (btn->getToggleState()) bars = b; };
        addAndMakeVisible (btn);
    }

    variation.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    variation.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    variation.setRange (0.0, 1.0);
    variation.setValue (0.35);
    variation.setTooltip ("How far the new loop can wander from your beat");
    ui::setKnobAccent (variation, theme::vibe);
    addAndMakeVisible (variation);

    generate.onClick = [this] { generateLoop (false); };
    newKit.onClick = [this] { generateLoop (true); };
    newKit.setTooltip ("Keep the pattern idea, roll brand-new drum sounds");
    generate.setEnabled (false);
    newKit.setEnabled (false);
    addAndMakeVisible (generate);
    addAndMakeVisible (newKit);
    addAndMakeVisible (grid);

    loopTile.setEmptyText ("Your loop lands here");
    midiTile.setEmptyText ("MIDI version lands here");
    loopTile.onPreview = [this] { loopStartMs = juce::Time::getMillisecondCounterHiRes(); };
    addAndMakeVisible (loopTile);
    addAndMakeVisible (midiTile);
    status = "Works with WAV, MP3, FLAC, AIFF and OGG - full songs or short loops.";
}

VibeMatchPage::~VibeMatchPage() { alive->store (false); }

bool VibeMatchPage::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;mp3;flac;aif;aiff;ogg")) return true;
    return false;
}

void VibeMatchPage::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;mp3;flac;aif;aiff;ogg")) { loadFile (juce::File (f)); break; }
    repaint();
}

void VibeMatchPage::mouseUp (const juce::MouseEvent& e)
{
    if (dropArea.contains (e.getPosition()) && e.mouseWasClicked()) browse();
}

void VibeMatchPage::browse()
{
    chooser = std::make_unique<juce::FileChooser> ("Pick a beat", juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   "*.wav;*.mp3;*.flac;*.aif;*.aiff;*.ogg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f.existsAsFile()) loadFile (f);
                          });
}

void VibeMatchPage::loadFile (const juce::File& f)
{
    if (loading) return;
    loading = true;
    fileName = f.getFileName();
    status = "Listening to your beat...";
    generate.setEnabled (false);
    newKit.setEnabled (false);
    repaint();

    juce::Component::SafePointer<VibeMatchPage> safe (this);
    juce::Thread::launch ([safe, f]
    {
        ph::GrooveProfile prof;
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
        if (reader == nullptr)
            prof.error = "I couldn't open that file. Try exporting it as a WAV.";
        else
        {
            const auto len = (int) std::min (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 360.0));
            const int ch = (int) juce::jlimit (1u, 2u, reader->numChannels);
            juce::AudioBuffer<float> tmp (ch, juce::jmax (1, len));
            reader->read (&tmp, 0, len, 0, true, ch > 1);
            std::vector<float> mono ((size_t) len);
            for (int i = 0; i < len; ++i)
                mono[(size_t) i] = ch > 1 ? 0.5f * (tmp.getSample (0, i) + tmp.getSample (1, i)) : tmp.getSample (0, i);
            prof = ph::PatternEngine::analyzeReference (mono, reader->sampleRate);
        }
        juce::MessageManager::callAsync ([safe, prof]
        {
            if (safe == nullptr) return;
            safe->loading = false;
            safe->profile = prof;
            if (! prof.valid)
            {
                safe->status = prof.error;
                safe->repaint();
                return;
            }
            safe->status = "Detected " + juce::String (juce::roundToInt (prof.bpm)) + " BPM  -  "
                         + ph::DrumSynth::styleName (prof.style) + " feel"
                         + (prof.swing > 0.05f ? "  -  " + juce::String (juce::roundToInt (prof.swing * 200.0f)) + "% swing" : juce::String());
            safe->generate.setEnabled (true);
            safe->newKit.setEnabled (true);
            safe->generateLoop (true);
        });
    });
}

void VibeMatchPage::generateLoop (bool freshKit)
{
    if (! profile.valid) return;
    const double sr = proc.getRenderSampleRate();
    if (freshKit || ! hasKit || std::abs (kit.sounds[0].sampleRate - sr) > 1.0)
    {
        kit = ph::PatternEngine::makeKit (profile, rng, sr);
        hasKit = true;
    }
    const auto pattern = ph::PatternEngine::generate (profile, bars, (float) variation.getValue(), rng);
    const auto audio = ph::PatternEngine::render (pattern, kit, sr);
    auto buf = ProducersHeavenProcessor::toBuffer (audio);

    const juce::String name = "Heaven " + juce::String (ph::DrumSynth::styleName (profile.style)) + " Loop "
                            + juce::String (juce::roundToInt (profile.bpm)) + " BPM";
    loopTile.onReroll = [this] { generateLoop (false); };
    loopTile.setSound (buf, name, juce::String (bars) + (bars == 1 ? " bar loop" : " bar loop") + " - WAV", "Vibe Match", true);

    const auto midiFile = ProducersHeavenProcessor::uniqueFile ("Vibe Match", name, ".mid");
    if (ProducersHeavenProcessor::writeMidi (pattern, midiFile))
        midiTile.setMidiFile (midiFile, name);

    grid.setPattern (pattern);
    loopLengthMs = bars * 4.0 * 60000.0 / profile.bpm;
    if (proc.isLoopPlaying())
    {
        proc.preview (buf, true);
        loopStartMs = juce::Time::getMillisecondCounterHiRes();
    }
    repaint();
}

void VibeMatchPage::tick()
{
    if (proc.isLoopPlaying() && loopStartMs >= 0 && loopLengthMs > 0)
        grid.setPlayhead ((float) (std::fmod (juce::Time::getMillisecondCounterHiRes() - loopStartMs, loopLengthMs) / loopLengthMs));
    else if (loopStartMs >= 0)
    {
        grid.setPlayhead (-1.0f);
        loopStartMs = -1;
        loopTile.repaint();
    }
}

void VibeMatchPage::layout (juce::Rectangle<int> c)
{
    auto left = c.removeFromLeft (320);
    dropArea = left.removeFromTop (150);
    left.removeFromTop (78); // status text
    auto barsRow = left.removeFromTop (58);
    barsRow.removeFromTop (20);
    for (auto* b : barButtons) { b->setBounds (barsRow.removeFromLeft (98).reduced (0, 2)); barsRow.removeFromLeft (8); }
    left.removeFromTop (22);
    auto knobRow = left.removeFromTop (100);
    variation.setBounds (knobRow.withSizeKeepingCentre (96, 96).translated (0, 6));
    left.removeFromTop (10);
    generate.setBounds (left.removeFromTop (44));
    left.removeFromTop (10);
    newKit.setBounds (left.removeFromTop (38));

    c.removeFromLeft (26);
    auto tilesRow = c.removeFromBottom (128);
    c.removeFromBottom (16);
    grid.setBounds (c);
    midiTile.setBounds (tilesRow.removeFromRight (tilesRow.getWidth() / 3));
    tilesRow.removeFromRight (14);
    loopTile.setBounds (tilesRow);
}

void VibeMatchPage::paint (juce::Graphics& g)
{
    Page::paint (g);
    auto d = dropArea.toFloat();
    theme::drawGlass (g, d, 18.0f, dragOver ? 1.8f : 1.0f);
    if (dragOver) theme::drawGlow (g, d.getCentre(), d.getWidth() * 0.6f, theme::vibe.b, 0.25f);
    juce::Path outline, dashed;
    outline.addRoundedRectangle (d.reduced (8.0f), 12.0f);
    const float dashes[] = { 6.0f, 6.0f };
    juce::PathStrokeType (1.2f).createDashedStroke (dashed, outline, dashes, 2);
    g.setColour (theme::vibe.a.withAlpha (dragOver ? 0.9f : 0.45f));
    g.fillPath (dashed);

    ui::drawIcon (g, ui::Icon::Folder, d.withSizeKeepingCentre (40, 40).translated (0, -28), theme::text, 2.0f);
    g.setColour (theme::text);
    g.setFont (theme::font (16.0f, true));
    g.drawText (loading ? "Analyzing..." : fileName.isEmpty() ? "Drop a beat here" : fileName,
                d.reduced (16, 0).withTrimmedTop (d.getHeight() * 0.5f).withHeight (24), juce::Justification::centred, true);
    g.setColour (theme::textDim);
    g.setFont (theme::font (12.5f));
    g.drawText (fileName.isEmpty() ? "or click to browse" : "drop another or click to swap",
                d.withTrimmedTop (d.getHeight() * 0.5f + 24).withHeight (20), juce::Justification::centred);

    auto statusR = juce::Rectangle<int> (dropArea.getX(), dropArea.getBottom() + 8, dropArea.getWidth(), 66);
    g.setColour (theme::text.withAlpha (0.9f));
    g.setFont (theme::font (14.0f));
    g.drawFittedText (status, statusR, juce::Justification::centredLeft, 3);

    if (! barButtons.isEmpty())
        drawLabel (g, "LENGTH", barButtons[0]->getBounds().translated (0, -24).withWidth (200));
    auto kb = variation.getBounds();
    drawLabel (g, "VARIATION", kb.withY (kb.getY() - 18).withHeight (16).withSizeKeepingCentre (160, 16), 12.0f, juce::Justification::centred);
    drawLabel (g, "SIMILAR", juce::Rectangle<int> (kb.getX() - 76, kb.getBottom() - 22, 70, 16), 10.5f, juce::Justification::centredRight);
    drawLabel (g, "WILD", juce::Rectangle<int> (kb.getRight() + 6, kb.getBottom() - 22, 70, 16), 10.5f, juce::Justification::centredLeft);
}

// =====================================================================================
// FX HEAVEN
// =====================================================================================
FXHeavenPage::FXHeavenPage (ProducersHeavenProcessor& p)
    : Page ("FX Heaven", "Stack creative effects on your whole beat. Switch one on, turn the knob, vibe out.",
            theme::fxh, ui::Icon::FXHeaven),
      proc (p)
{
    const theme::Accent accents[] = { theme::forge, theme::doctor, theme::fxh, theme::vibe, theme::forge, theme::doctor, theme::vibe };
    cards.resize (FXRack::NumEffects);
    for (int e = 0; e < FXRack::NumEffects; ++e)
    {
        auto& c = cards[(size_t) e];
        c.name = FXRack::name (e);
        c.blurb = FXRack::blurb (e);
        c.accent = accents[e];
        c.toggle = std::make_unique<ui::PowerToggle> (c.accent);
        c.knob = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
        ui::setKnobAccent (*c.knob, c.accent);
        c.knob->setPopupDisplayEnabled (true, true, this);
        addAndMakeVisible (*c.toggle);
        addAndMakeVisible (*c.knob);
        const juce::String id (FXRack::id (e));
        c.toggleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id + "_on", *c.toggle);
        c.knobAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id + "_amt", *c.knob);
        c.toggle->onStateChange = [this] { repaint(); };
        c.knob->onValueChange = [this] { repaint(); };
    }

    for (auto* s : { &mix, &out, &previewVol })
    {
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setPopupDisplayEnabled (true, true, this);
        ui::setKnobAccent (*s, theme::fxh);
        addAndMakeVisible (s);
    }
    mixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "fx_mix", mix);
    outAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "out_gain", out);
    prevAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "preview_vol", previewVol);
    out.setTextValueSuffix (" dB");
}

void FXHeavenPage::layout (juce::Rectangle<int> c)
{
    hintArea = c.removeFromBottom (30);
    c.removeFromBottom (6);
    const int gap = 14;
    const int w = (c.getWidth() - gap * 3) / 4, h = (c.getHeight() - gap) / 2;
    for (int i = 0; i < 8; ++i)
    {
        const juce::Rectangle<int> b (c.getX() + (i % 4) * (w + gap), c.getY() + (i / 4) * (h + gap), w, h);
        if (i < FXRack::NumEffects)
        {
            auto& card = cards[(size_t) i];
            card.bounds = b;
            card.toggle->setBounds (b.getRight() - 58, b.getY() + 14, 46, 26);
            const int ks = juce::jmin (b.getWidth() - 60, b.getHeight() - 84);
            card.knob->setBounds (b.withSizeKeepingCentre (ks, ks).withY (b.getY() + 62));
        }
        else
        {
            masterArea = b;
            auto k = b.reduced (10).withTrimmedTop (44).withTrimmedBottom (22);
            const int kw = k.getWidth() / 3;
            for (auto* s : { &mix, &out, &previewVol })
                s->setBounds (k.removeFromLeft (kw).withSizeKeepingCentre (juce::jmin (kw, k.getHeight()), juce::jmin (kw, k.getHeight())));
        }
    }
}

void FXHeavenPage::paint (juce::Graphics& g)
{
    Page::paint (g);
    for (auto& c : cards)
    {
        const bool on = c.toggle->getToggleState();
        auto r = c.bounds.toFloat();
        if (on)
        {
            theme::drawGlow (g, r.getCentre(), r.getWidth() * 0.7f, c.accent.b, 0.14f);
            juce::ColourGradient tint (c.accent.a.withAlpha (0.16f), r.getX(), r.getY(), c.accent.b.withAlpha (0.08f), r.getRight(), r.getBottom(), false);
            g.setGradientFill (tint);
            g.fillRoundedRectangle (r, 18.0f);
        }
        theme::drawGlass (g, r, 18.0f);
        g.setColour (on ? theme::text : theme::text.withAlpha (0.75f));
        g.setFont (theme::font (18.0f, true));
        g.drawText (c.name, r.reduced (16, 0).withY (r.getY() + 14).withHeight (24), juce::Justification::centredLeft);
        g.setColour (theme::textDim);
        g.setFont (theme::font (12.5f));
        g.drawText (c.blurb, r.reduced (16, 0).withY (r.getY() + 38).withHeight (18), juce::Justification::centredLeft);
        g.setColour (on ? theme::text : theme::textDim.withAlpha (0.6f));
        g.setFont (theme::font (13.0f, true));
        g.drawText (juce::String (juce::roundToInt (c.knob->getValue() * 100.0)) + "%",
                    r.withTrimmedTop (r.getHeight() - 30).withHeight (20), juce::Justification::centred);
    }

    auto m = masterArea.toFloat();
    theme::drawGlass (g, m, 18.0f);
    g.setColour (theme::text);
    g.setFont (theme::font (18.0f, true));
    g.drawText ("Master", m.reduced (16, 0).withY (m.getY() + 14).withHeight (24), juce::Justification::centredLeft);
    const juce::String labels[] = { "FX MIX", "OUTPUT", "PREVIEW" };
    const juce::Slider* sl[] = { &mix, &out, &previewVol };
    for (int i = 0; i < 3; ++i)
        drawLabel (g, labels[i], sl[i]->getBounds().withY (masterArea.getBottom() - 30).withHeight (16).expanded (20, 0), 10.5f,
                   juce::Justification::centred);

    drawHint (g, hintArea, "Put me on your Master (or any mixer track) so I can hear the beat. Every knob is automatable from FL's plugin menu.");
}
