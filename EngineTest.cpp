// Standalone test for the Producers Heaven engine (no JUCE needed).
// Build: g++ -O2 -std=c++17 -I../Source/Core EngineTest.cpp ../Source/Core/*.cpp -o engine_test
#include "BeatAnalyzer.h"
#include "DrumSynth.h"
#include "PatternEngine.h"
#include <cmath>
#include <cstdio>

using namespace ph;

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf ("FAIL: %s\n", msg); ++failures; } else std::printf ("ok:   %s\n", msg); } while (0)

static void printRow (const char* name, const std::vector<float>& row)
{
    std::printf ("  %-8s ", name);
    for (size_t i = 0; i < row.size() && i < 32; ++i) std::printf ("%c", row[i] > 0 ? 'X' : '.');
    std::printf ("\n");
}

int main()
{
    const double sr = 44100.0;
    std::mt19937 rng (42);

    // 1. every drum renders, is finite and normalised
    for (int s = 0; s < kNumStyles; ++s)
        for (int t = 0; t < kNumDrumTypes; ++t)
        {
            auto d = DrumSynth::generate ((DrumType) t, (Style) s, rng, sr);
            float peak = 0; bool finite = true;
            for (auto v : d.samples) { peak = std::max (peak, std::abs (v)); finite &= std::isfinite (v); }
            if (! finite || peak < 0.5f || peak > 0.95f || d.samples.size() < 100)
            {
                std::printf ("FAIL: %s/%s peak %.2f len %zu\n", DrumSynth::styleName ((Style) s), DrumSynth::typeName ((DrumType) t), peak, d.samples.size());
                ++failures;
            }
        }
    std::printf ("ok:   rendered %d drum sounds\n", kNumStyles * kNumDrumTypes);

    // 2. render loops at known tempos & styles, then re-detect them
    struct Case { Style style; double bpm; };
    const Case cases[] = { { Style::Trap, 140 }, { Style::BoomBap, 90 }, { Style::House, 124 }, { Style::LoFi, 85 }, { Style::Drill, 142 } };
    for (auto c : cases)
    {
        GrooveProfile g; g.style = c.style; g.bpm = c.bpm; g.valid = true; // empty rows => template
        auto pat = PatternEngine::generate (g, 4, 0.3f, rng);
        auto kit = PatternEngine::makeKit (g, rng, sr);
        auto loop = PatternEngine::render (pat, kit, sr);

        std::vector<float> mono, L, R;
        for (int rep = 0; rep < 3; ++rep)
            for (size_t i = 0; i < loop.left.size(); ++i)
            {
                mono.push_back (0.5f * (loop.left[i] + loop.right[i]));
                L.push_back (loop.left[i]); R.push_back (loop.right[i]);
            }

        auto prof = PatternEngine::analyzeReference (mono, sr);
        char msg[256];
        std::snprintf (msg, sizeof msg, "%s %.0f BPM -> detected %.1f BPM, style %s, swing %.2f (%s)",
                       DrumSynth::styleName (c.style), c.bpm, prof.bpm, DrumSynth::styleName (prof.style), prof.swing,
                       prof.valid ? "valid" : prof.error.c_str());
        const bool tempoOk = std::abs (prof.bpm - c.bpm) < 1.5 || std::abs (prof.bpm - c.bpm * 2) < 2 || std::abs (prof.bpm * 2 - c.bpm) < 2;
        CHECK (prof.valid && tempoOk, msg);
        printRow ("kick", pat.hits[0]);
        printRow ("snare", pat.hits[2]);
        printRow ("hat", pat.hits[4]);

        // regenerate from the detected profile
        auto pat2 = PatternEngine::generate (prof, 2, 0.4f, rng);
        auto loop2 = PatternEngine::render (pat2, kit, sr);
        std::snprintf (msg, sizeof msg, "  generated similar loop: %zu samples (%.2f s)", loop2.left.size(), loop2.left.size() / sr);
        CHECK (loop2.left.size() > 0, msg);
        printRow ("kick'", pat2.hits[0]);
        printRow ("snare'", pat2.hits[2]);
        printRow ("hat'", pat2.hits[4]);

        auto rep = BeatAnalyzer::analyze (L, R, sr);
        std::snprintf (msg, sizeof msg, "  beat doctor: %.0f BPM, %d kicks %d snares %d hats, %zu tips (punch %.0f low %.0f bright %.0f)",
                       rep.metrics.bpm, rep.metrics.kicks, rep.metrics.snares, rep.metrics.hats, rep.tips.size(),
                       rep.metrics.punch, rep.metrics.lowEnd, rep.metrics.brightness);
        CHECK (rep.valid && ! rep.tips.empty(), msg);
        for (auto& t : rep.tips) std::printf ("      [%d] %s%s\n", (int) t.severity, t.title.c_str(), t.hasReplacement ? " (+sound)" : "");
    }

    // 3. silence and too-short input give friendly errors
    std::vector<float> silence ((size_t) (sr * 5), 0.0f);
    CHECK (! BeatAnalyzer::analyze (silence, silence, sr).valid, "silence is rejected");
    std::vector<float> shortv ((size_t) (sr * 1), 0.1f);
    CHECK (! PatternEngine::analyzeReference (shortv, sr).valid, "short file is rejected");

    std::printf (failures ? "\n%d FAILURES\n" : "\nALL PASSED\n", failures);
    return failures ? 1 : 0;
}
