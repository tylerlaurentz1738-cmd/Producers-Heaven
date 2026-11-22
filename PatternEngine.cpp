#include "PatternEngine.h"
#include "BeatAnalyzer.h"
#include "Biquad.h"
#include <algorithm>
#include <cmath>

namespace ph
{
namespace
{
using Row = std::array<float, kStepsPerBar>;

struct StyleTemplate
{
    Row kick, snare, hat, openHat, clap, perc, rim;
    float use808;
    float rollChance;
};

const StyleTemplate& templateFor (Style s)
{
    static const StyleTemplate trap {
        { 1,0,0,0, 0,0,0,.5f, 0,0,.6f,0, 0,0,.3f,0 },
        { 0,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,.1f },
        { 1,.5f,1,.5f, 1,.5f,1,.5f, 1,.5f,1,.5f, 1,.5f,1,.5f },
        { 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,.2f,0 },
        { 0,0,0,0, 0,0,0,0, .6f,0,0,0, 0,0,0,0 },
        { 0,0,0,.15f, 0,0,.15f,0, 0,0,0,.2f, 0,.15f,0,0 },
        { 0 },
        0.9f, 0.35f };
    static const StyleTemplate boomBap {
        { 1,0,0,0, 0,0,0,.45f, 0,0,1,0, 0,0,0,.2f },
        { 0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,.15f },
        { 1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,.3f },
        { 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,.25f,0 },
        { 0 },
        { 0 },
        { 0,0,0,.2f, 0,0,0,0, 0,.2f,0,0, 0,0,0,0 },
        0.15f, 0.0f };
    static const StyleTemplate drill {
        { 1,0,0,0, 0,0,.6f,0, 0,0,0,.5f, 0,0,.3f,0 },
        { 0,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0 },
        { 1,0,.7f,.4f, 1,0,.7f,0, 1,0,.7f,.4f, 1,0,.7f,.3f },
        { 0 },
        { 0 },
        { 0,0,0,.2f, 0,0,0,0, 0,0,.15f,0, 0,0,0,.2f },
        { 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,.2f,0,0 },
        1.0f, 0.3f };
    static const StyleTemplate lofi {
        { 1,0,0,0, 0,0,0,.3f, 0,0,.8f,0, 0,0,0,0 },
        { 0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0 },
        { 1,0,.8f,0, 1,0,.8f,0, 1,0,.8f,0, 1,0,.8f,.2f },
        { 0 },
        { 0 },
        { 0 },
        { 0,0,0,0, 0,0,.2f,0, 0,0,0,0, 0,0,0,.2f },
        0.0f, 0.0f };
    static const StyleTemplate house {
        { 1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0 },
        { 0,0,0,0, .3f,0,0,0, 0,0,0,0, .3f,0,0,0 },
        { .6f,0,0,.5f, .6f,0,0,.5f, .6f,0,0,.5f, .6f,0,0,.5f },
        { 0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,0 },
        { 0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0 },
        { 0,0,0,.2f, 0,0,0,0, 0,.25f,0,0, 0,0,0,.2f },
        { 0 },
        0.0f, 0.0f };
    switch (s)
    {
        case Style::BoomBap: return boomBap;
        case Style::Drill: return drill;
        case Style::LoFi: return lofi;
        case Style::House: return house;
        default: return trap;
    }
}

float sharpen (float p, float variation)
{
    if (p <= 0.001f) return 0.0f;
    if (p >= 0.999f) return 1.0f;
    const float k = 4.0f + 14.0f * (1.0f - variation);
    return 1.0f / (1.0f + std::exp (-k * (p - 0.42f)));
}

float sumRow (const Row& r)
{
    float s = 0;
    for (auto v : r) s += v;
    return s;
}
} // namespace

GrooveProfile PatternEngine::analyzeReference (const std::vector<float>& monoIn, double sr)
{
    GrooveProfile g;
    if (monoIn.size() < (size_t) (sr * 1.0))
    {
        g.error = "That file is too short - give me at least a one-bar loop.";
        return g;
    }

    // Long songs: analyse a 45 s window from the middle (usually where the full drums are).
    // Short loops: repeat them so there's enough to lock onto the tempo.
    std::vector<float> mono;
    const size_t maxLen = (size_t) (sr * 45.0);
    if (monoIn.size() < (size_t) (sr * 12.0))
        while (mono.size() < (size_t) (sr * 12.0)) mono.insert (mono.end(), monoIn.begin(), monoIn.end());
    else if (monoIn.size() > maxLen)
    {
        const size_t start = (monoIn.size() - maxLen) / 2;
        mono.assign (monoIn.begin() + (long) start, monoIn.begin() + (long) (start + maxLen));
    }
    else
        mono = monoIn;

    g.seconds = (double) monoIn.size() / sr;
    const auto od = computeOnsets (mono, sr);
    g.bpm = estimateTempo (od.totalFlux, od.frameRate);
    if (g.bpm <= 0)
    {
        g.error = "I couldn't find a steady rhythm in that file.";
        return g;
    }

    const double stepSec = 60.0 / g.bpm / 4.0;
    auto timeOf = [&] (int frame) { return (double) frame * od.hop / sr; };

    // grid phase: circular mean of all onsets on the 16th-note grid
    double cs = 0, sn = 0;
    auto accumulatePhase = [&] (const std::vector<int>& list, double w)
    {
        for (int f : list)
        {
            const double a = 2.0 * 3.14159265358979 * timeOf (f) / stepSec;
            cs += w * std::cos (a); sn += w * std::sin (a);
        }
    };
    accumulatePhase (od.kicks, 2.0);
    accumulatePhase (od.snares, 1.5);
    accumulatePhase (od.hats, 1.0);
    double phase = std::atan2 (sn, cs);
    if (phase < 0) phase += 2.0 * 3.14159265358979;
    const double offset = phase / (2.0 * 3.14159265358979) * stepSec;

    const double barSec = stepSec * kStepsPerBar;
    const double numBars = std::max (1.0, (double) mono.size() / sr / barSec);

    Row kc {}, scnt {}, hcnt {};
    double oddDev = 0, evenDev = 0; int oddN = 0, evenN = 0;
    std::vector<std::pair<int, double>> hatPos; // grid index, deviation
    auto bin = [&] (const std::vector<int>& list, Row& row, bool isHat)
    {
        for (int f : list)
        {
            const double pos = (timeOf (f) - offset) / stepSec;
            const long k = std::lround (pos);
            const int idx = (int) (((k % kStepsPerBar) + kStepsPerBar) % kStepsPerBar);
            row[(size_t) idx] += 1.0f;
            if (isHat) hatPos.emplace_back (idx, pos - (double) k);
        }
    };
    bin (od.kicks, kc, false);
    bin (od.snares, scnt, false);
    bin (od.hats, hcnt, true);

    // choose which step is the downbeat
    int bestRot = 0; float bestScore = -1e9f;
    for (int r = 0; r < kStepsPerBar; ++r)
    {
        auto at = [&] (const Row& row, int s) { return row[(size_t) ((r + s) % kStepsPerBar)]; };
        const float backbeat = at (scnt, 4) + at (scnt, 12);
        const float half = 1.6f * at (scnt, 8);
        const float score = 2.0f * at (kc, 0) + std::max (backbeat, half) - 1.5f * at (scnt, 0)
                          + 0.3f * (at (hcnt, 0) + at (hcnt, 4) + at (hcnt, 8) + at (hcnt, 12));
        if (score > bestScore) { bestScore = score; bestRot = r; }
    }
    for (int s = 0; s < kStepsPerBar; ++s)
    {
        const size_t src = (size_t) ((bestRot + s) % kStepsPerBar);
        g.kick[(size_t) s] = std::min (1.0f, (float) (kc[src] / numBars));
        g.snare[(size_t) s] = std::min (1.0f, (float) (scnt[src] / numBars));
        g.hat[(size_t) s] = std::min (1.0f, (float) (hcnt[src] / numBars));
    }
    for (auto& hp : hatPos)
    {
        const int s = ((hp.first - bestRot) % kStepsPerBar + kStepsPerBar) % kStepsPerBar;
        if (s % 2 == 1) { oddDev += hp.second; ++oddN; } else { evenDev += hp.second; ++evenN; }
    }
    if (oddN > 4 && evenN > 4)
        g.swing = (float) std::min (0.45, std::max (0.0, oddDev / oddN - evenDev / evenN));

    const double rms = std::max (1e-9, [&] { double s = 0; for (auto v : mono) s += (double) v * v; return std::sqrt (s / mono.size()); }());
    auto bandRel = [&] (int type, double f)
    {
        auto b = filtered (mono, sr, type, f);
        double s = 0; for (auto v : b) s += (double) v * v;
        return dbOf (std::sqrt (s / b.size())) - dbOf (rms);
    };
    g.brightness = std::min (1.0f, std::max (0.0f, (bandRel (1, 9000.0) + 40.0f) / 26.0f));
    g.lowEnd = std::min (1.0f, std::max (0.0f, (bandRel (0, 70.0) + 20.0f) / 17.0f));
    g.density = std::min (1.0f, (float) (od.hats.size() / (numBars * kStepsPerBar)));

    const bool fourOnFloor = g.kick[0] > .6f && g.kick[4] > .6f && g.kick[8] > .6f && g.kick[12] > .6f;
    const bool backbeat = g.snare[4] > .35f && g.snare[12] > .35f;
    const bool halftime = g.snare[8] > .35f && ! backbeat;
    if (fourOnFloor && g.bpm >= 110) g.style = Style::House;
    else if (g.brightness < 0.3f && g.bpm < 100) g.style = Style::LoFi;
    else if (halftime && g.bpm >= 136 && g.bpm <= 148 && sumRow (g.kick) < 3.0f && g.swing < 0.1f) g.style = Style::Drill;
    else if (halftime || g.bpm >= 125) g.style = Style::Trap;
    else g.style = Style::BoomBap;

    if (od.kicks.size() + od.snares.size() + od.hats.size() < 8)
    {
        g.error = "I couldn't hear clear drums in that file. Try a beat with drums rather than an acapella or ambient loop.";
        return g;
    }
    g.valid = true;
    return g;
}

Pattern PatternEngine::generate (const GrooveProfile& g, int bars, float v, std::mt19937& rng)
{
    auto rnd = [&rng] { return std::uniform_real_distribution<float> (0.0f, 1.0f) (rng); };
    const auto& t = templateFor (g.style);
    v = std::min (1.0f, std::max (0.0f, v));

    Pattern p;
    p.bars = std::max (1, std::min (8, bars));
    p.bpm = g.bpm;
    p.swing = g.swing;
    const int total = p.totalSteps();
    for (auto& h : p.hits) h.assign ((size_t) total, 0.0f);
    p.hatDivision.assign ((size_t) total, 1);

    auto blend = [&] (const Row& prof, const Row& tmpl)
    {
        Row r {};
        const bool empty = sumRow (prof) < 0.5f;
        for (int s = 0; s < kStepsPerBar; ++s)
            r[(size_t) s] = empty ? tmpl[(size_t) s] : (1.0f - v) * prof[(size_t) s] + v * tmpl[(size_t) s];
        return r;
    };
    const Row kick = blend (g.kick, t.kick);
    const Row snare = blend (g.snare, t.snare);
    const Row hat = blend (g.hat, t.hat);

    const bool use808 = rnd() < std::max (t.use808, g.lowEnd > 0.65f ? 0.6f : 0.0f);
    const bool layerClap = sumRow (t.clap) > 0 ? true : rnd() < 0.3f + 0.3f * v;
    const float hum = 0.08f + 0.1f * v;
    auto vel = [&] (float base) { return std::min (1.0f, std::max (0.25f, base + (rnd() - 0.5f) * 2.0f * hum)); };

    auto& K = p.hits[(size_t) DrumType::Kick];
    auto& B = p.hits[(size_t) DrumType::Bass808];
    auto& S = p.hits[(size_t) DrumType::Snare];
    auto& C = p.hits[(size_t) DrumType::Clap];
    auto& H = p.hits[(size_t) DrumType::HiHat];
    auto& O = p.hits[(size_t) DrumType::OpenHat];
    auto& P = p.hits[(size_t) DrumType::Perc];
    auto& R = p.hits[(size_t) DrumType::Rim];

    // Bar 1 pattern is the "theme"; following bars are variations of it.
    for (int bar = 0; bar < p.bars; ++bar)
    {
        const bool isFillBar = p.bars >= 2 && bar == p.bars - 1 && rnd() < 0.45f + 0.4f * v;
        const float barVar = bar == 0 ? v : std::min (1.0f, v * 0.6f + 0.1f);
        for (int s = 0; s < kStepsPerBar; ++s)
        {
            const size_t i = (size_t) (bar * kStepsPerBar + s);
            const size_t i0 = (size_t) s; // same step in bar 1

            // kick
            if (bar > 0 && K[i0] > 0 && rnd() > barVar * 0.35f) K[i] = vel (K[i0]);
            else if (rnd() < sharpen (kick[(size_t) s], barVar)) K[i] = vel (s % 4 == 0 ? 0.95f : 0.82f);

            // snare - strong backbeats are always kept so the groove stays recognisable
            if (snare[(size_t) s] >= 0.5f) S[i] = vel (0.95f);
            else if (snare[(size_t) s] > 0.2f && rnd() < sharpen (snare[(size_t) s], barVar) * 0.5f) S[i] = vel (0.45f); // ghost notes

            // hats
            if (rnd() < sharpen (hat[(size_t) s], barVar))
            {
                H[i] = vel (s % 4 == 0 ? 0.9f : (s % 2 == 0 ? 0.75f : 0.55f));
                if (s % 2 == 1 && rnd() < t.rollChance * (0.5f + v) * 0.35f)
                    p.hatDivision[i] = rnd() < 0.5f ? 2 : 3;
            }
            if (rnd() < sharpen (t.openHat[(size_t) s], barVar) * (0.6f + 0.4f * v)) { O[i] = vel (0.7f); H[i] = 0; }

            if (layerClap && S[i] >= 0.7f) C[i] = vel (0.8f);
            else if (rnd() < t.clap[(size_t) s] * 0.9f) C[i] = vel (0.85f);

            if (rnd() < t.perc[(size_t) s] * (0.6f + v)) P[i] = vel (0.6f);
            if (rnd() < t.rim[(size_t) s] * (0.6f + v)) R[i] = vel (0.6f);
        }

        if (isFillBar)
        {
            const int kind = (int) (rnd() * 3.0f);
            for (int s = 12; s < 16; ++s)
            {
                const size_t i = (size_t) (bar * kStepsPerBar + s);
                if (kind == 0) S[i] = std::max (S[i], 0.45f + 0.15f * (float) (s - 12));
                else if (kind == 1) { H[i] = 0.7f; p.hatDivision[i] = s >= 14 ? 3 : 2; }
                else if (s % 2 == 1) P[i] = vel (0.7f);
            }
        }
    }

    if (use808)
        for (int i = 0; i < total; ++i)
        {
            if (K[(size_t) i] > 0) B[(size_t) i] = vel (0.9f);
            else if (i % 4 == 3 && rnd() < 0.12f + 0.2f * v) B[(size_t) i] = vel (0.75f);
        }

    // guarantee there is a downbeat
    if (K[0] <= 0) K[0] = 0.95f;
    return p;
}

Kit PatternEngine::makeKit (const GrooveProfile& g, std::mt19937& rng, double sr)
{
    Kit kit;
    for (int i = 0; i < kNumDrumTypes; ++i)
    {
        const auto type = (DrumType) i;
        auto params = DrumSynth::randomParams (type, g.style, rng);
        params.tone = std::min (1.0f, std::max (0.0f, params.tone + (g.brightness - 0.5f) * 0.4f));
        kit.sounds[(size_t) i] = DrumSynth::fromParams (type, params, sr, DrumSynth::randomName (type, rng));
    }
    return kit;
}

std::vector<DrumEvent> PatternEngine::events (const Pattern& p)
{
    std::vector<DrumEvent> ev;
    for (int t = 0; t < kNumDrumTypes; ++t)
    {
        const auto& row = p.hits[(size_t) t];
        for (int s = 0; s < (int) row.size(); ++s)
        {
            const float v = row[(size_t) s];
            if (v <= 0) continue;
            const double beat = s / 4.0 + ((s % 2 == 1) ? p.swing / 4.0 : 0.0);
            const int div = (DrumType) t == DrumType::HiHat ? p.hatDivision[(size_t) s] : 1;
            for (int d = 0; d < div; ++d)
                ev.push_back ({ (DrumType) t, beat + d / (4.0 * div), v * (d == 0 ? 1.0f : 0.8f - 0.08f * (float) d) });
        }
    }
    std::sort (ev.begin(), ev.end(), [] (const DrumEvent& a, const DrumEvent& b) { return a.beat < b.beat; });
    return ev;
}

StereoAudio PatternEngine::render (const Pattern& p, const Kit& kit, double sr)
{
    StereoAudio out;
    out.sampleRate = sr;
    const double beatSec = 60.0 / p.bpm;
    const size_t total = (size_t) std::llround (p.bars * 4 * beatSec * sr);
    out.left.assign (total, 0.0f);
    out.right.assign (total, 0.0f);
    if (total == 0) return out;

    static const float gain[kNumDrumTypes] = { 0.95f, 0.75f, 0.8f, 0.6f, 0.42f, 0.38f, 0.45f, 0.45f };
    static const float pan[kNumDrumTypes]  = { 0.0f, 0.0f, 0.0f, 0.05f, 0.18f, 0.22f, -0.3f, -0.15f };

    const auto ev = events (p);
    auto chokeGroup = [] (DrumType t) { return t == DrumType::Bass808 ? 1 : (t == DrumType::HiHat || t == DrumType::OpenHat) ? 2 : 0; };

    for (size_t e = 0; e < ev.size(); ++e)
    {
        const auto& event = ev[e];
        const auto& snd = kit.sounds[(size_t) event.type].samples;
        if (snd.empty()) continue;
        const size_t start = (size_t) std::llround (event.beat * beatSec * sr) % total;
        size_t len = snd.size();

        // choke: 808s cut previous 808s, closed hats cut open hats
        const int grp = chokeGroup (event.type);
        if (grp != 0 && (grp == 1 || event.type == DrumType::OpenHat))
        {
            for (size_t k = 1; k <= ev.size(); ++k)
            {
                const auto& nx = ev[(e + k) % ev.size()];
                if (chokeGroup (nx.type) != grp) continue;
                double nb = nx.beat + (e + k >= ev.size() ? p.bars * 4.0 : 0.0);
                const size_t ns = (size_t) std::llround (nb * beatSec * sr);
                const size_t startAbs = (size_t) std::llround (event.beat * beatSec * sr);
                if (ns > startAbs) len = std::min (len, ns - startAbs);
                break;
            }
        }
        len = std::min (len, total); // never overlap itself across the loop
        const size_t fade = std::min (len, (size_t) (0.004 * sr));
        const float g = gain[(size_t) event.type] * event.velocity;
        const float pn = pan[(size_t) event.type];
        const float gl = g * std::cos ((pn + 1.0f) * 0.25f * kPi) * 1.41421f;
        const float gr = g * std::sin ((pn + 1.0f) * 0.25f * kPi) * 1.41421f;
        for (size_t i = 0; i < len; ++i)
        {
            float x = snd[i];
            if (len < snd.size() && i >= len - fade) x *= (float) (len - i) / (float) fade;
            const size_t idx = (start + i) % total; // wrap tails so the loop is seamless
            out.left[idx] += x * gl;
            out.right[idx] += x * gr;
        }
    }

    float peak = 0;
    for (size_t i = 0; i < total; ++i) peak = std::max ({ peak, std::abs (out.left[i]), std::abs (out.right[i]) });
    if (peak > 1e-6f)
    {
        const float k = 0.89f / peak;
        for (size_t i = 0; i < total; ++i) { out.left[i] *= k; out.right[i] *= k; }
    }
    return out;
}
} // namespace ph
