#pragma once
#include <random>
#include <string>
#include <vector>

namespace ph
{
enum class DrumType { Kick = 0, Bass808, Snare, Clap, HiHat, OpenHat, Perc, Rim, Count };
constexpr int kNumDrumTypes = (int) DrumType::Count;

enum class Style { Trap = 0, BoomBap, Drill, LoFi, House, Count };
constexpr int kNumStyles = (int) Style::Count;

// Every drum is described by the same handful of macro controls (all 0..1 except pitch in Hz).
struct DrumParams
{
    float pitch = 50.0f;  // fundamental (Hz) for tonal drums, tone shift for noisy ones
    float decay = 0.5f;   // 0 = very short, 1 = long
    float punch = 0.5f;   // pitch-envelope depth / transient strength
    float tone  = 0.5f;   // dark (0) .. bright (1)
    float noise = 0.5f;   // amount of noise layer
    float drive = 0.2f;   // saturation
    float click = 0.3f;   // transient click layer
};

struct DrumSound
{
    DrumType type = DrumType::Kick;
    DrumParams params;
    std::vector<float> samples; // mono
    double sampleRate = 44100.0;
    std::string name;
};

class DrumSynth
{
public:
    static std::vector<float> render (DrumType type, const DrumParams& p, double sampleRate);
    static DrumParams randomParams (DrumType type, Style style, std::mt19937& rng);
    static DrumSound generate (DrumType type, Style style, std::mt19937& rng, double sampleRate);
    static DrumSound fromParams (DrumType type, const DrumParams& p, double sampleRate, const std::string& name);

    static const char* typeName (DrumType t);
    static const char* styleName (Style s);
    static int midiNote (DrumType t); // GM-ish note used for MIDI export / triggering
    static std::string randomName (DrumType t, std::mt19937& rng);
};
} // namespace ph
