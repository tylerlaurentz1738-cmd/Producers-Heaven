#include "BeatAnalyzer.h"
#include "Biquad.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>

namespace ph
{
namespace
{
double rmsOf (const std::vector<float>& v, size_t start = 0, size_t end = SIZE_MAX)
{
    end = std::min (end, v.size());
    if (end <= start) return 0.0;
    double s = 0;
    for (size_t i = start; i < end; ++i) s += (double) v[i] * v[i];
    return std::sqrt (s / (double) (end - start));
}

// Smoothed power envelope sampled once per hop. Squaring then low-passing removes the
// ripple you get from bass waves, so a single kick doesn't look like several onsets.
std::vector<float> frameEnv (const std::vector<float>& x, int hop, double sr, double smoothHz)
{
    Biquad lp1, lp2;
    lp1.setLowPass (sr, smoothHz, 0.6);
    lp2.setLowPass (sr, smoothHz, 0.6);
    const size_t frames = x.size() / (size_t) hop;
    std::vector<float> e (frames);
    size_t i = 0;
    for (size_t f = 0; f < frames; ++f)
    {
        float v = 0;
        for (size_t end = (f + 1) * (size_t) hop; i < end; ++i)
            v = lp2.process (lp1.process (x[i] * x[i]));
        e[f] = std::sqrt (std::max (0.0f, v));
    }
    return e;
}

std::vector<float> fluxOf (const std::vector<float>& env)
{
    float mx = 1.0e-9f;
    for (auto v : env) mx = std::max (mx, v);
    std::vector<float> c (env.size()), fl (env.size(), 0.0f);
    for (size_t i = 0; i < env.size(); ++i) c[i] = std::log1p (60.0f * env[i] / mx);
    for (size_t i = 1; i < env.size(); ++i) fl[i] = std::max (0.0f, c[i] - c[i - 1]);
    // the follower starts from silence, so ignore the first few frames, and normalise
    // to a high percentile instead of the max so one huge hit can't hide the rest
    for (size_t i = 0; i < std::min<size_t> (4, fl.size()); ++i) fl[i] = 0.0f;
    std::vector<float> nz;
    for (auto v : fl) if (v > 0.01f) nz.push_back (v);
    float ref = 1.0e-9f;
    if (! nz.empty())
    {
        const size_t k = (size_t) (0.97 * (double) (nz.size() - 1));
        std::nth_element (nz.begin(), nz.begin() + (long) k, nz.end());
        ref = std::max (ref, nz[k]);
    }
    for (auto& v : fl) v = std::min (1.5f, v / ref);
    return fl;
}

// Peak-pick the flux. `mask` suppresses weaker bumps shortly after a strong hit (e.g. a kick
// beating against an 808). Each onset is then walked back to where the energy started rising,
// which removes the delay added by the envelope smoothing.
std::vector<int> pickPeaks (const std::vector<float>& flux, double frameRate, double minGapSec, float globalThresh,
                            double maskSec = 0.0, float maskRatio = 0.0f)
{
    std::vector<int> peaks;
    const int w = std::max (2, (int) (0.1 * frameRate));
    const int gap = std::max (1, (int) (minGapSec * frameRate));
    const int mask = (int) (maskSec * frameRate);
    const int n = (int) flux.size();
    for (int i = 1; i < n - 1; ++i)
    {
        double sum = 0; int cnt = 0;
        for (int k = std::max (0, i - w); k <= std::min (n - 1, i + w); ++k) { sum += flux[(size_t) k]; ++cnt; }
        const float thr = (float) (sum / cnt) * 1.5f + globalThresh;
        const float v = flux[(size_t) i];
        if (! (v > thr && v >= flux[(size_t) i - 1] && v >= flux[(size_t) i + 1])) continue;

        if (! peaks.empty() && i - peaks.back() < gap)
        {
            if (v > flux[(size_t) peaks.back()]) peaks.back() = i;
            continue;
        }
        if (! peaks.empty() && i - peaks.back() < mask && v < maskRatio * flux[(size_t) peaks.back()])
            continue;
        peaks.push_back (i);
    }

    const int maxBack = std::max (1, (int) (0.05 * frameRate));
    std::vector<int> out;
    for (int pk : peaks)
    {
        int j = pk;
        while (j > 0 && pk - j < maxBack && flux[(size_t) j - 1] > 0.08f * flux[(size_t) pk]) --j;
        if (out.empty() || j > out.back()) out.push_back (j);
    }
    return out;
}

float percentile (std::vector<float> v, double q)
{
    if (v.empty()) return 0.0f;
    const size_t k = (size_t) (q * (double) (v.size() - 1));
    std::nth_element (v.begin(), v.begin() + (long) k, v.end());
    return v[k];
}

// Drop onsets whose band is basically silent: either the whole band is far below the mix,
// or this particular hit is far below the band's typical loud level.
std::vector<int> gateByLevel (const std::vector<int>& onsets, const std::vector<float>& env, float mixRef, float bandMin, float hitMin)
{
    std::vector<int> out;
    const float ref = percentile (env, 0.95);
    if (ref < bandMin * mixRef) return out;
    const int n = (int) env.size();
    for (int f : onsets)
    {
        float pk = 0;
        for (int i = f; i < std::min (n, f + 10); ++i) pk = std::max (pk, env[(size_t) i]);
        if (pk >= hitMin * ref) out.push_back (f);
    }
    return out;
}

bool near (const std::vector<int>& list, int frame, int tol)
{
    auto it = std::lower_bound (list.begin(), list.end(), frame - tol);
    return it != list.end() && *it <= frame + tol;
}

float mapScore (float v, float lo, float hi)
{
    return std::min (100.0f, std::max (0.0f, (v - lo) / (hi - lo) * 100.0f));
}

double acAt (const std::vector<double>& ac, double lag)
{
    const int i = (int) lag;
    if (i < 0 || i + 1 >= (int) ac.size()) return 0.0;
    const double fr = lag - i;
    return ac[(size_t) i] * (1.0 - fr) + ac[(size_t) i + 1] * fr;
}

// decay time (s) for envelope to drop 20 dB from its peak after an onset
float decayTime (const std::vector<float>& env, int onset, int limit, double frameRate)
{
    const int n = (int) env.size();
    int pk = onset; float pv = 0;
    for (int i = onset; i < std::min (n, onset + 6); ++i) if (env[(size_t) i] > pv) { pv = env[(size_t) i]; pk = i; }
    if (pv <= 0) return 0;
    int i = pk;
    while (i < std::min (n, limit) && env[(size_t) i] > pv * 0.1f) ++i;
    return (float) ((i - pk) / frameRate);
}
} // namespace

OnsetData computeOnsets (const std::vector<float>& mono, double sr)
{
    OnsetData d;
    d.sampleRate = sr;
    d.hop = sr > 60000 ? 512 : 256;
    d.frameRate = sr / d.hop;

    const auto low = filtered (mono, sr, 0, 130.0);
    // snare "snap" band: above 808 harmonics and kick body, below most hat energy
    const auto mid = filtered (filtered (filtered (mono, sr, 1, 900.0), sr, 1, 900.0), sr, 0, 5000.0);
    const auto high = filtered (filtered (mono, sr, 1, 7000.0), sr, 1, 7000.0);

    const float mixRef = percentile (frameEnv (mono, d.hop, sr, 35.0), 0.95);
    d.lowEnv = frameEnv (low, d.hop, sr, 18.0);
    d.midEnv = frameEnv (mid, d.hop, sr, 35.0);
    d.highEnv = frameEnv (high, d.hop, sr, 60.0);
    d.lowFlux = fluxOf (d.lowEnv);
    d.midFlux = fluxOf (d.midEnv);
    d.highFlux = fluxOf (d.highEnv);


    // Kick + 808 overlapping makes the low band pulse (beating). Those pulses keep getting
    // quieter, whereas a real new hit gets back up near the recent peak level.
    {
        const auto cand = gateByLevel (pickPeaks (d.lowFlux, d.frameRate, 0.09, 0.15f, 0.3, 0.45f), d.lowEnv, mixRef, 0.05f, 0.3f);
        const int look = (int) (0.35 * d.frameRate);
        const int n = (int) d.lowEnv.size();
        for (int f : cand)
        {
            float after = 0, before = 0;
            const int rise = (int) (0.15 * d.frameRate);
            for (int i = f; i < std::min (n, f + rise); ++i) after = std::max (after, d.lowEnv[(size_t) i]);
            for (int i = std::max (0, f - look); i < f; ++i) before = std::max (before, d.lowEnv[(size_t) i]);
            if (after >= 0.9f * before) d.kicks.push_back (f);
        }
    }
    auto snareCand = pickPeaks (d.midFlux, d.frameRate, 0.09, 0.15f, 0.2, 0.35f);
    for (int f : gateByLevel (snareCand, d.midEnv, mixRef, 0.03f, 0.35f))
    {
        float fl = 0, mPk = 0, hPk = 0;
        for (int i = f; i < std::min ((int) d.midFlux.size(), f + 8); ++i)
        {
            fl = std::max (fl, d.midFlux[(size_t) i]);
            mPk = std::max (mPk, d.midEnv[(size_t) i]);
            hPk = std::max (hPk, d.highEnv[(size_t) i]);
        }
        if (near (d.kicks, f, 2) && fl < 0.9f) continue; // kick click, not a snare
        if (dbOf (mPk) - dbOf (hPk) < -3.0f) continue;    // mostly >7 kHz energy: that's a hat
        d.snares.push_back (f);
    }
    auto hatCand = gateByLevel (pickPeaks (d.highFlux, d.frameRate, 0.04, 0.1f), d.highEnv, mixRef, 0.01f, 0.2f);
    for (int f : hatCand)
        if (! near (d.snares, f, 1)) d.hats.push_back (f);

    // Tempo is estimated from a cleaned-up onset train rather than raw flux
    std::vector<float> train (d.lowFlux.size(), 0.0f);
    auto addTrain = [&train] (const std::vector<int>& list, float w)
    {
        static const float kern[] = { 0.25f, 0.6f, 1.0f, 0.6f, 0.25f };
        for (int f : list)
            for (int k = -2; k <= 2; ++k)
            {
                const int i = f + k;
                if (i >= 0 && i < (int) train.size()) train[(size_t) i] += w * kern[k + 2];
            }
    };
    addTrain (d.kicks, 1.0f);
    addTrain (d.snares, 1.0f);
    addTrain (d.hats, 0.35f);
    d.totalFlux = train;
    return d;
}

double estimateTempo (const std::vector<float>& fluxIn, double frameRate)
{
    if (fluxIn.size() < 64) return 0.0;
    std::vector<double> flux (fluxIn.begin(), fluxIn.end());
    const double mean = std::accumulate (flux.begin(), flux.end(), 0.0) / (double) flux.size();
    for (auto& v : flux) v -= mean;

    const int maxLag = std::min ((int) flux.size() - 2, (int) (8.0 * 60.0 * frameRate / 80.0) + 4);
    std::vector<double> ac ((size_t) maxLag + 2, 0.0);
    for (int lag = 1; lag <= maxLag; ++lag)
    {
        double s = 0;
        for (size_t i = 0; i + (size_t) lag < flux.size(); ++i) s += flux[i] * flux[i + (size_t) lag];
        ac[(size_t) lag] = s / (double) (flux.size() - (size_t) lag);
    }

    double best = 0, bestScore = -1e30;
    for (double bpm = 80.0; bpm < 160.0; bpm += 0.25)
    {
        const double lag = 60.0 * frameRate / bpm;
        // a real tempo lines up at the 16th, 8th, beat, half-bar and bar levels at the same time;
        // dotted rhythms (very common in trap) only line up at one of them
        double score = acAt (ac, lag) + 0.7 * acAt (ac, lag * 2.0) + 1.0 * acAt (ac, lag * 4.0)
                     + 0.8 * acAt (ac, lag * 8.0) + 0.4 * acAt (ac, lag / 2.0) + 0.6 * acAt (ac, lag / 4.0);
        if (score > bestScore) { bestScore = score; best = bpm; }
    }
    const double rounded = std::round (best);
    if (std::abs (rounded - best) <= 0.5) best = rounded;
    return best;
}

DrumParams ParamOverride::apply (DrumParams p) const
{
    if (pitch >= 0) p.pitch = pitch;
    if (decay >= 0) p.decay = decay;
    if (punch >= 0) p.punch = punch;
    if (tone >= 0) p.tone = tone;
    if (noise >= 0) p.noise = noise;
    if (drive >= 0) p.drive = drive;
    if (click >= 0) p.click = click;
    return p;
}

DrumSound BeatAnalyzer::makeReplacement (const Tip& tip, Style style, std::mt19937& rng, double sr)
{
    auto p = tip.replaceWith.apply (DrumSynth::randomParams (tip.replaceType, style, rng));
    return DrumSynth::fromParams (tip.replaceType, p, sr, DrumSynth::randomName (tip.replaceType, rng));
}

BeatReport BeatAnalyzer::analyze (const std::vector<float>& L, const std::vector<float>& R, double sr)
{
    BeatReport rep;
    const size_t n = std::min (L.size(), R.size());
    if (n < (size_t) (sr * 3.0))
    {
        rep.error = "I didn't hear enough. Hit Listen, play your beat for at least 4 seconds, then hit Stop.";
        return rep;
    }

    std::vector<float> mono (n), side (n);
    for (size_t i = 0; i < n; ++i) { mono[i] = 0.5f * (L[i] + R[i]); side[i] = 0.5f * (L[i] - R[i]); }

    auto& m = rep.metrics;
    m.seconds = (float) (n / sr);
    float peak = 0;
    int clipRuns = 0, run = 0;
    for (size_t i = 0; i < n; ++i)
    {
        const float a = std::max (std::abs (L[i]), std::abs (R[i]));
        peak = std::max (peak, a);
        if (a >= 0.999f) { if (++run == 3) ++clipRuns; } else run = 0;
    }
    const double rms = rmsOf (mono);
    m.peakDb = dbOf (peak);
    m.rmsDb = dbOf (rms);
    m.crestDb = m.peakDb - m.rmsDb;

    if (m.rmsDb < -60.0f)
    {
        rep.error = "That was basically silence. Make sure Producers Heaven is on a mixer track your beat plays through (the Master works great).";
        return rep;
    }

    // ---- tonal balance ----
    const auto subSig = filtered (filtered (mono, sr, 0, 60.0), sr, 0, 60.0);
    const auto lowSig = filtered (mono, sr, 2, 120.0, 0.8);
    const auto lowMidSig = filtered (mono, sr, 2, 400.0, 1.0);
    const auto highMidSig = filtered (mono, sr, 2, 7500.0, 1.0);
    const auto airSig = filtered (filtered (mono, sr, 1, 10000.0), sr, 1, 10000.0);
    const auto kickBand = filtered (mono, sr, 0, 150.0);
    const auto clickBand = filtered (mono, sr, 2, 3000.0, 0.8);
    const auto bodyBand = filtered (mono, sr, 2, 200.0, 1.0);
    const auto crackBand = filtered (mono, sr, 2, 5000.0, 0.8);

    const float subRel = dbOf (rmsOf (subSig)) - m.rmsDb;
    const float lowRel = dbOf (rmsOf (lowSig)) - m.rmsDb;
    const float lowMidRel = dbOf (rmsOf (lowMidSig)) - m.rmsDb;
    const float airRel = dbOf (rmsOf (airSig)) - m.rmsDb;

    // ---- stereo ----
    const auto lowL = filtered (L, sr, 0, 150.0);
    const auto lowR = filtered (R, sr, 0, 150.0);
    double sLR = 0, sLL = 0, sRR = 0;
    for (size_t i = 0; i < n; ++i) { sLR += (double) lowL[i] * lowR[i]; sLL += (double) lowL[i] * lowL[i]; sRR += (double) lowR[i] * lowR[i]; }
    const double lowCorr = (sLL > 0 && sRR > 0) ? sLR / std::sqrt (sLL * sRR) : 1.0;
    const float sideDb = dbOf (rmsOf (side)) - m.rmsDb;

    // ---- rhythm ----
    const auto od = computeOnsets (mono, sr);
    m.bpm = estimateTempo (od.totalFlux, od.frameRate);
    m.kicks = (int) od.kicks.size();
    m.snares = (int) od.snares.size();
    m.hats = (int) od.hats.size();
    const size_t hop = (size_t) od.hop;

    // kick character
    float kSub = 0, kClick = 0, kDecay = 0, kPunch = 0; int kc = 0;
    for (size_t k = 0; k < od.kicks.size() && kc < 64; ++k)
    {
        const size_t s0 = (size_t) std::max (0, od.kicks[k] - 1) * hop;
        const size_t s1 = s0 + (size_t) (0.15 * sr);
        const double kb = rmsOf (kickBand, s0, s1);
        if (kb <= 1e-6) continue;
        kSub += dbOf (rmsOf (subSig, s0, s1)) - dbOf (kb);
        kClick += dbOf (rmsOf (clickBand, s0, s0 + (size_t) (0.02 * sr))) - dbOf (kb);
        const int next = k + 1 < od.kicks.size() ? od.kicks[k + 1] : (int) od.lowEnv.size();
        kDecay += decayTime (od.lowEnv, od.kicks[k], next, od.frameRate);
        float pk = 0;
        for (size_t i = s0; i < std::min (s1, n); ++i) pk = std::max (pk, std::abs (kickBand[i]));
        kPunch += dbOf (pk) - dbOf (kb);
        ++kc;
    }
    if (kc > 0) { kSub /= kc; kClick /= kc; kDecay /= kc; kPunch /= kc; }

    // snare character
    float sCrack = 0, sDecay = 0; int sc = 0;
    for (size_t k = 0; k < od.snares.size() && sc < 64; ++k)
    {
        const size_t s0 = (size_t) od.snares[k] * hop;
        const size_t s1 = s0 + (size_t) (0.12 * sr);
        const double body = rmsOf (bodyBand, s0, s1);
        if (body <= 1e-7) continue;
        sCrack += dbOf (rmsOf (crackBand, s0, s1)) - dbOf (body);
        const int next = k + 1 < od.snares.size() ? od.snares[k + 1] : (int) od.midEnv.size();
        sDecay += decayTime (od.midEnv, od.snares[k], next, od.frameRate);
        ++sc;
    }
    if (sc > 0) { sCrack /= sc; sDecay /= sc; }

    // hat character
    float hBright = 0; int hc = 0;
    std::vector<float> hatLevels;
    for (size_t k = 0; k < od.hats.size() && hc < 128; ++k)
    {
        const size_t s0 = (size_t) od.hats[k] * hop;
        const size_t s1 = s0 + (size_t) (0.04 * sr);
        const double hm = rmsOf (highMidSig, s0, s1);
        if (hm <= 1e-7) continue;
        hBright += dbOf (rmsOf (airSig, s0, s1)) - dbOf (hm);
        float pk = 0;
        for (int f = od.hats[k]; f < std::min ((int) od.highEnv.size(), od.hats[k] + 4); ++f) pk = std::max (pk, od.highEnv[(size_t) f]);
        hatLevels.push_back (pk);
        ++hc;
    }
    if (hc > 0) hBright /= hc;
    float hatCv = 1.0f;
    if (hatLevels.size() >= 4)
    {
        const double mu = std::accumulate (hatLevels.begin(), hatLevels.end(), 0.0) / hatLevels.size();
        double var = 0;
        for (auto v : hatLevels) var += (v - mu) * (v - mu);
        hatCv = mu > 0 ? (float) (std::sqrt (var / hatLevels.size()) / mu) : 1.0f;
    }

    // ---- scores ----
    m.punch = kc > 0 ? mapScore (kPunch, 2.5f, 9.0f) : mapScore (m.crestDb, 5.0f, 16.0f);
    m.lowEnd = mapScore (std::max (subRel, lowRel), -22.0f, -3.0f);
    m.brightness = mapScore (airRel, -42.0f, -14.0f);
    m.dynamics = mapScore (m.crestDb, 4.0f, 18.0f);
    m.width = mapScore (sideDb, -30.0f, -6.0f);

    // ---- tips ----
    auto& tips = rep.tips;
    auto add = [&tips] (Tip::Severity sev, std::string title, std::string detail)
    {
        Tip t; t.severity = sev; t.title = std::move (title); t.detail = std::move (detail);
        tips.push_back (t);
        return &tips.back();
    };
    auto withSound = [] (Tip* t, DrumType type, ParamOverride o, std::string label)
    {
        t->hasReplacement = true; t->replaceType = type; t->replaceWith = o; t->replaceLabel = std::move (label);
    };

    if (clipRuns > 3)
        add (Tip::Important, "Your beat is clipping",
             "I caught " + std::to_string (clipRuns) + " clipped spots. Pull your drum bus or master down about 3 dB and put a limiter last in the chain with the ceiling at -1 dB.");
    else if (m.peakDb < -14.0f)
        add (Tip::Suggestion, "Beat is running quiet",
             "Peaks are only around " + std::to_string ((int) m.peakDb) + " dB. That's fine while mixing, but bring it up with a limiter before you bounce so it competes with other songs.");

    if (kc < 2)
    {
        auto* t = add (Tip::Suggestion, "I barely heard a kick",
                       "The low end isn't getting clear kick hits. A solid kick gives the groove something to lock onto.");
        ParamOverride o; o.punch = 0.75f; o.click = 0.45f;
        withSound (t, DrumType::Kick, o, "Solid kick");
    }
    else
    {
        if (kSub < -9.0f && subRel < -12.0f)
        {
            auto* t = add (Tip::Important, "Kick is missing low end",
                           "Your kick is mostly mid-range thump with very little sub. On big speakers it will feel weak. Try a deeper kick or layer this one underneath.");
            ParamOverride o; o.pitch = 46.0f; o.decay = 0.6f; o.punch = 0.7f; o.click = 0.4f;
            withSound (t, DrumType::Kick, o, "Deeper kick");
        }
        if (kClick < -24.0f)
        {
            auto* t = add (Tip::Suggestion, "Kick won't cut through on phone speakers",
                           "There's almost no attack/click on the kick, so on phones and earbuds it disappears. A kick with more beater click fixes that.");
            ParamOverride o; o.click = 0.85f; o.punch = 0.85f; o.tone = 0.7f;
            withSound (t, DrumType::Kick, o, "Punchier kick");
        }
        if (kDecay > 0.5f && subRel > -9.0f)
        {
            auto* t = add (Tip::Suggestion, "Kick tail is crowding the low end",
                           "The kick rings out for about " + std::to_string ((int) (kDecay * 1000)) + " ms on top of a heavy sub. Use a shorter kick, or if you have an 808, sidechain it to the kick so they take turns.");
            ParamOverride o; o.decay = 0.15f; o.punch = 0.8f;
            withSound (t, DrumType::Kick, o, "Tighter kick");
        }
    }

    if (sc < 2)
    {
        auto* t = add (Tip::Suggestion, "No clear snare or clap backbeat",
                       "I couldn't find a consistent snare. Most beats need a strong backbeat - try layering a clap on beat 3 (or 2 and 4).");
        ParamOverride o; o.decay = 0.45f;
        withSound (t, DrumType::Clap, o, "Layer clap");
    }
    else
    {
        if (sCrack < -14.0f)
        {
            auto* t = add (Tip::Suggestion, "Snare sounds dull",
                           "The snare has lots of body but very little crack, so it gets buried behind the hats and melody.");
            ParamOverride o; o.tone = 0.85f; o.noise = 0.7f;
            withSound (t, DrumType::Snare, o, "Brighter snare");
        }
        else if (sCrack > 4.0f)
        {
            auto* t = add (Tip::Suggestion, "Snare is thin",
                           "The snare is nearly all top-end sizzle with no body. Layer something with weight around 180-220 Hz.");
            ParamOverride o; o.tone = 0.35f; o.noise = 0.35f; o.pitch = 185.0f;
            withSound (t, DrumType::Snare, o, "Fatter snare");
        }
        if (sDecay > 0.45f)
        {
            auto* t = add (Tip::Suggestion, "Snare tail is muddying the groove",
                           "The snare rings for about " + std::to_string ((int) (sDecay * 1000)) + " ms. A tighter snare (or a gate) will make the pocket feel cleaner.");
            ParamOverride o; o.decay = 0.2f;
            withSound (t, DrumType::Snare, o, "Tighter snare");
        }
    }

    if (hc < 4)
    {
        auto* t = add (Tip::Suggestion, "Add hi-hats for bounce",
                       "There's not much going on up top. Even simple 8th-note hats with a few rolls add a lot of movement.");
        withSound (t, DrumType::HiHat, ParamOverride(), "Fresh hi-hat");
    }
    else
    {
        if (hBright > 2.0f || airRel > -13.0f)
        {
            auto* t = add (Tip::Suggestion, "Hi-hats are harsh",
                           "There's a lot of energy above 10 kHz - it gets tiring fast. Try a smoother hat or roll off some top with an EQ.");
            ParamOverride o; o.tone = 0.25f;
            withSound (t, DrumType::HiHat, o, "Smoother hat");
        }
        else if (airRel < -38.0f)
        {
            auto* t = add (Tip::Suggestion, "Top end is dull",
                           "The beat has very little air. Crisper hats will make it sound more expensive.");
            ParamOverride o; o.tone = 0.9f;
            withSound (t, DrumType::HiHat, o, "Crispier hat");
        }
        if (hatCv < 0.08f && hc >= 8)
            add (Tip::Suggestion, "Hi-hats sound robotic",
                 "Every hat hits at almost the same volume. Vary velocities between roughly 70-100%, accent the downbeats, and throw in a roll or two.");
    }

    if (lowMidRel > -8.0f)
        add (Tip::Suggestion, "Low mids are muddy",
             "There's a build-up around 250-500 Hz. Cut 2-4 dB there on your sample, chords or 808 to open the mix up.");

    if (subRel > -3.0f)
    {
        auto* t = add (Tip::Suggestion, "Sub is overpowering everything",
                       "The sub is louder than the rest of the beat. Turn it down and add saturation so it's still heard on small speakers.");
        ParamOverride o; o.drive = 0.65f; o.tone = 0.6f;
        withSound (t, DrumType::Bass808, o, "Saturated 808");
    }

    if (m.crestDb < 6.0f)
        add (Tip::Important, "Beat is over-compressed",
             "There's very little difference between the peaks and the average level, so the drums lose their snap. Back off the limiter/compressor or use parallel compression.");
    else if (m.crestDb > 20.0f)
        add (Tip::Suggestion, "Drums are spiky",
             "The transients jump way above everything else. A little bus compression (2:1, slow attack) will glue the kit together.");

    if (lowCorr < 0.7)
        add (Tip::Important, "Low end is stereo",
             "Your bass/kick is spread across left and right, which loses punch on clubs and phones. Make everything below ~120 Hz mono.");

    std::stable_sort (tips.begin(), tips.end(), [] (const Tip& a, const Tip& b) { return a.severity > b.severity; });

    if (tips.empty())
        add (Tip::Good, "This beat is sitting nice",
             "Balanced low end, clear backbeat and healthy dynamics. If you want more character, try the FX Heaven page.");

    rep.valid = true;
    return rep;
}
} // namespace ph
