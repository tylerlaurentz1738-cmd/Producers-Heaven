#pragma once
#include "DrumSynth.h"
#include <array>
#include <random>
#include <string>
#include <vector>

namespace ph
{
constexpr int kStepsPerBar = 16;

// What we learn from an uploaded beat
struct GrooveProfile
{
    bool valid = false;
    std::string error;
    double bpm = 140.0;
    float swing = 0.0f;        // 0..0.5 of a 16th
    float brightness = 0.5f;   // 0..1
    float lowEnd = 0.5f;       // 0..1
    float density = 0.5f;      // 0..1 (hats per step)
    Style style = Style::Trap;
    std::array<float, kStepsPerBar> kick {}, snare {}, hat {};
    double seconds = 0;
};

struct Pattern
{
    int bars = 2;
    double bpm = 140.0;
    float swing = 0.0f;
    // velocity per step per drum, 0 = no hit
    std::array<std::vector<float>, kNumDrumTypes> hits;
    std::vector<int> hatDivision; // 1 = normal, 2 = 32nd roll, 3 = triplet roll
    int totalSteps() const { return bars * kStepsPerBar; }
};

struct DrumEvent
{
    DrumType type;
    double beat;     // position in quarter notes
    float velocity;  // 0..1
};

struct Kit
{
    std::array<DrumSound, kNumDrumTypes> sounds;
};

struct StereoAudio
{
    std::vector<float> left, right;
    double sampleRate = 44100.0;
};

class PatternEngine
{
public:
    static GrooveProfile analyzeReference (const std::vector<float>& mono, double sampleRate);
    static Pattern generate (const GrooveProfile& g, int bars, float variation, std::mt19937& rng);
    static Kit makeKit (const GrooveProfile& g, std::mt19937& rng, double sampleRate);
    static std::vector<DrumEvent> events (const Pattern& p);
    static StereoAudio render (const Pattern& p, const Kit& kit, double sampleRate);
};
} // namespace ph
