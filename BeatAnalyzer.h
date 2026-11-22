#pragma once
#include "DrumSynth.h"
#include <string>
#include <vector>

namespace ph
{
// ---------- shared onset / tempo analysis ----------
struct OnsetData
{
    double sampleRate = 44100.0;
    int hop = 256;
    double frameRate = 0;
    std::vector<float> lowEnv, midEnv, highEnv;    // per-frame RMS
    std::vector<float> lowFlux, midFlux, highFlux; // positive spectral-ish flux per band
    std::vector<float> totalFlux;
    std::vector<int> kicks, snares, hats;          // onset frame indices
};

OnsetData computeOnsets (const std::vector<float>& mono, double sampleRate);
double estimateTempo (const std::vector<float>& flux, double frameRate);

// ---------- Beat Doctor ----------
struct ParamOverride
{
    float pitch = -1, decay = -1, punch = -1, tone = -1, noise = -1, drive = -1, click = -1;
    DrumParams apply (DrumParams p) const;
};

struct Tip
{
    enum Severity { Good = 0, Suggestion = 1, Important = 2 };
    Severity severity = Suggestion;
    std::string title;
    std::string detail;
    bool hasReplacement = false;
    DrumType replaceType = DrumType::Kick;
    ParamOverride replaceWith;
    std::string replaceLabel; // e.g. "Deeper kick"
};

struct BeatMetrics
{
    double bpm = 0;
    float punch = 0, lowEnd = 0, brightness = 0, dynamics = 0, width = 0; // 0..100 scores
    float peakDb = -100, rmsDb = -100, crestDb = 0;
    int kicks = 0, snares = 0, hats = 0;
    float seconds = 0;
};

struct BeatReport
{
    bool valid = false;
    std::string error;
    BeatMetrics metrics;
    std::vector<Tip> tips;
};

class BeatAnalyzer
{
public:
    static BeatReport analyze (const std::vector<float>& left, const std::vector<float>& right, double sampleRate);
    static DrumSound makeReplacement (const Tip& tip, Style style, std::mt19937& rng, double sampleRate);
};
} // namespace ph
