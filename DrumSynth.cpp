#include "DrumSynth.h"
#include "Biquad.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ph
{
namespace
{
constexpr float kTwoPi = 6.28318530718f;

struct Noise
{
    uint32_t state;
    explicit Noise (uint32_t seed) : state (seed ? seed : 0x9E3779B9u) {}
    inline float next()
    {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        return (float) ((double) state / 4294967295.0 * 2.0 - 1.0);
    }
};

inline float clamp01 (float v) { return std::min (1.0f, std::max (0.0f, v)); }
inline float sat (float x, float drive)
{
    const float k = 1.0f + drive * 6.0f;
    return std::tanh (x * k) / std::tanh (k);
}

uint32_t seedFrom (const DrumParams& p, int type)
{
    auto h = (uint32_t) (p.pitch * 131.0f + p.decay * 7919.0f + p.tone * 104729.0f + p.noise * 1299709.0f + (float) type * 17.0f);
    return h * 2654435761u + 1u;
}

void finalise (std::vector<float>& s, double sr, float targetPeak)
{
    if (s.empty()) return;
    // DC blocker
    float prevX = 0, prevY = 0;
    for (auto& v : s) { const float y = v - prevX + 0.9995f * prevY; prevX = v; prevY = y; v = y; }

    const int fadeIn = std::min ((int) s.size(), 16);
    for (int i = 0; i < fadeIn; ++i) s[(size_t) i] *= (float) i / (float) fadeIn;
    const int fadeOut = std::min ((int) s.size(), (int) (sr * 0.006));
    for (int i = 0; i < fadeOut; ++i) s[s.size() - 1 - (size_t) i] *= (float) i / (float) fadeOut;

    float peak = 0;
    for (auto v : s) peak = std::max (peak, std::abs (v));
    if (peak > 1.0e-6f)
        for (auto& v : s) v *= targetPeak / peak;
}

std::vector<float> renderKick (const DrumParams& p, double sr, bool is808)
{
    const float len = is808 ? 0.6f + p.decay * 2.2f : 0.18f + p.decay * 0.9f;
    const int n = (int) (len * sr);
    std::vector<float> out ((size_t) n);
    Noise noise (seedFrom (p, is808 ? 1 : 0));
    Biquad clickHp; clickHp.setHighPass (sr, 2500.0);

    const float f0 = p.pitch;
    const float startF = is808 ? f0 * (1.0f + p.punch * 1.6f) : f0 * (2.0f + p.punch * 6.0f);
    const float pitchTau = is808 ? 0.035f + 0.04f * (1.0f - p.punch) : 0.012f + 0.035f * (1.0f - p.punch);
    const float ampTau = is808 ? 0.25f + p.decay * 1.1f : 0.06f + p.decay * 0.38f;
    float phase = 0;

    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / (float) sr;
        const float f = f0 + (startF - f0) * std::exp (-t / pitchTau);
        phase += kTwoPi * f / (float) sr;
        if (phase > kTwoPi) phase -= kTwoPi;

        const float attack = std::min (1.0f, t / (is808 ? 0.003f : 0.0008f));
        float amp = attack * std::exp (-t / ampTau);
        if (! is808 && t < 0.02f) amp *= 1.0f + p.punch * 0.25f; // short "hold" for punch

        float body = std::sin (phase) + 0.18f * p.tone * std::sin (2.0f * phase);
        body = sat (body * amp, p.drive * (is808 ? 1.4f : 1.0f));

        const float clickEnv = std::exp (-t / 0.0035f);
        const float click = clickHp.process (noise.next()) * clickEnv * p.click * 0.9f
                          + std::sin (kTwoPi * 1600.0f * t) * clickEnv * p.click * 0.25f;
        out[(size_t) i] = body + click;
    }
    finalise (out, sr, is808 ? 0.85f : 0.9f);
    return out;
}

std::vector<float> renderSnare (const DrumParams& p, double sr)
{
    const float len = 0.14f + p.decay * 0.4f;
    const int n = (int) (len * sr);
    std::vector<float> out ((size_t) n);
    Noise noise (seedFrom (p, 2));
    Biquad hp, lp;
    hp.setHighPass (sr, 900.0 + p.tone * 2800.0);
    lp.setLowPass (sr, 6000.0 + p.tone * 9000.0);

    const float bodyTau = 0.025f + p.decay * 0.06f;
    const float noiseTau = 0.045f + p.decay * 0.16f;
    float ph1 = 0, ph2 = 0;
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / (float) sr;
        const float f = p.pitch * (1.0f + 0.6f * p.punch * std::exp (-t / 0.008f));
        ph1 += kTwoPi * f / (float) sr;
        ph2 += kTwoPi * f * 1.73f / (float) sr;
        const float body = (std::sin (ph1) + 0.5f * std::sin (ph2)) * std::exp (-t / bodyTau);
        const float nz = lp.process (hp.process (noise.next())) * std::exp (-t / noiseTau);
        const float click = noise.next() * std::exp (-t / 0.002f) * p.click * 0.5f;
        const float mix = body * (1.0f - p.noise * 0.55f) + nz * (0.45f + p.noise * 1.1f) + click;
        out[(size_t) i] = sat (mix, p.drive);
    }
    finalise (out, sr, 0.9f);
    return out;
}

std::vector<float> renderClap (const DrumParams& p, double sr)
{
    const float tailTau = 0.05f + p.decay * 0.28f;
    const int n = (int) ((0.04f + tailTau * 5.0f) * sr);
    std::vector<float> out ((size_t) n);
    Noise noise (seedFrom (p, 3));
    Biquad bp, hp;
    bp.setBandPass (sr, (1000.0 + p.tone * 1500.0) * p.pitch, 0.9);
    hp.setHighPass (sr, 500.0);
    const float bursts[] = { 0.0f, 0.010f, 0.021f, 0.031f };

    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / (float) sr;
        float env = 0;
        for (auto b : bursts)
            if (t >= b) env = std::max (env, std::exp (-(t - b) / 0.0055f));
        if (t >= bursts[3]) env = std::max (env, 0.75f * std::exp (-(t - bursts[3]) / tailTau));
        const float x = hp.process (bp.process (noise.next())) * env;
        out[(size_t) i] = sat (x * 2.0f, p.drive);
    }
    finalise (out, sr, 0.88f);
    return out;
}

std::vector<float> renderHat (const DrumParams& p, double sr, bool open)
{
    const float tau = open ? 0.11f + p.decay * 0.32f : 0.010f + p.decay * 0.045f;
    const int n = (int) ((tau * 6.0f + 0.005f) * sr);
    std::vector<float> out ((size_t) n);
    Noise noise (seedFrom (p, open ? 5 : 4));
    static const float freqs[] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };
    float phases[6] = {};
    Biquad hp1, hp2, lp;
    hp1.setHighPass (sr, 6500.0 + p.tone * 2000.0);
    hp2.setHighPass (sr, 6500.0 + p.tone * 2000.0);
    lp.setLowPass (sr, std::min (sr * 0.45, 11000.0 + p.tone * 9000.0));

    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / (float) sr;
        float metal = 0;
        for (int k = 0; k < 6; ++k)
        {
            phases[k] += freqs[k] * p.pitch * 1.8f / (float) sr;
            if (phases[k] >= 1.0f) phases[k] -= 1.0f;
            metal += phases[k] < 0.5f ? 1.0f : -1.0f;
        }
        metal /= 6.0f;
        const float src = metal * (1.0f - p.noise * 0.7f) + noise.next() * (0.3f + p.noise * 0.7f);
        const float attack = std::min (1.0f, t / 0.0006f);
        const float env = attack * std::exp (-t / tau);
        out[(size_t) i] = sat (lp.process (hp2.process (hp1.process (src))) * env * 2.0f, p.drive * 0.5f);
    }
    finalise (out, sr, open ? 0.7f : 0.75f);
    return out;
}

std::vector<float> renderPerc (const DrumParams& p, double sr)
{
    const float tau = 0.04f + p.decay * 0.22f;
    const int n = (int) (tau * 6.0f * sr);
    std::vector<float> out ((size_t) n);
    Noise noise (seedFrom (p, 6));
    float car = 0, mod = 0;
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / (float) sr;
        const float f = p.pitch * (1.0f + 0.5f * p.punch * std::exp (-t / 0.01f));
        mod += kTwoPi * f * 1.41f / (float) sr;
        car += kTwoPi * f / (float) sr;
        const float index = p.tone * 2.5f * std::exp (-t / (tau * 0.5f));
        const float s = std::sin (car + index * std::sin (mod));
        const float click = noise.next() * std::exp (-t / 0.0015f) * p.click * 0.4f;
        out[(size_t) i] = sat (s * std::exp (-t / tau) + click, p.drive);
    }
    finalise (out, sr, 0.85f);
    return out;
}

std::vector<float> renderRim (const DrumParams& p, double sr)
{
    const float tau = 0.007f + p.decay * 0.025f;
    const int n = (int) ((tau * 7.0f + 0.01f) * sr);
    std::vector<float> out ((size_t) n);
    Noise noise (seedFrom (p, 7));
    Biquad hp; hp.setHighPass (sr, 3000.0);
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / (float) sr;
        const float s = std::sin (kTwoPi * 1700.0f * p.pitch * t) + 0.6f * std::sin (kTwoPi * 520.0f * p.pitch * t);
        const float nz = hp.process (noise.next()) * std::exp (-t / 0.003f) * (0.3f + p.noise);
        out[(size_t) i] = sat ((s * (0.6f + 0.4f * p.tone) + nz) * std::exp (-t / tau), p.drive);
    }
    finalise (out, sr, 0.8f);
    return out;
}
} // namespace

std::vector<float> DrumSynth::render (DrumType type, const DrumParams& p, double sr)
{
    switch (type)
    {
        case DrumType::Kick:    return renderKick (p, sr, false);
        case DrumType::Bass808: return renderKick (p, sr, true);
        case DrumType::Snare:   return renderSnare (p, sr);
        case DrumType::Clap:    return renderClap (p, sr);
        case DrumType::HiHat:   return renderHat (p, sr, false);
        case DrumType::OpenHat: return renderHat (p, sr, true);
        case DrumType::Perc:    return renderPerc (p, sr);
        case DrumType::Rim:     return renderRim (p, sr);
        default: break;
    }
    return {};
}

DrumParams DrumSynth::randomParams (DrumType type, Style style, std::mt19937& rng)
{
    auto u = [&rng] (float a, float b) { return std::uniform_real_distribution<float> (a, b) (rng); };

    // Style biases (added on top of each drum's base range)
    float toneB = 0, decayB = 0, driveB = 0, punchB = 0;
    switch (style)
    {
        case Style::Trap:    toneB = 0.10f;  decayB = 0.0f;   driveB = 0.05f; punchB = 0.15f; break;
        case Style::BoomBap: toneB = -0.15f; decayB = 0.05f;  driveB = 0.15f; punchB = 0.0f;  break;
        case Style::Drill:   toneB = 0.15f;  decayB = -0.05f; driveB = 0.10f; punchB = 0.10f; break;
        case Style::LoFi:    toneB = -0.30f; decayB = -0.05f; driveB = 0.25f; punchB = -0.1f; break;
        case Style::House:   toneB = 0.05f;  decayB = 0.10f;  driveB = 0.0f;  punchB = 0.10f; break;
        default: break;
    }

    DrumParams p;
    p.tone  = u (0.3f, 0.75f);
    p.decay = u (0.25f, 0.7f);
    p.punch = u (0.35f, 0.85f);
    p.noise = u (0.3f, 0.7f);
    p.drive = u (0.05f, 0.35f);
    p.click = u (0.15f, 0.6f);

    switch (type)
    {
        case DrumType::Kick:
            p.pitch = u (44.0f, 62.0f);
            if (style == Style::House) { p.pitch = u (48.0f, 56.0f); decayB += 0.1f; }
            if (style == Style::Drill || style == Style::Trap) decayB -= 0.15f;
            break;
        case DrumType::Bass808:
            p.pitch = u (34.0f, 52.0f);
            p.decay = u (0.4f, 0.85f);
            p.drive = u (0.15f, 0.55f);
            p.click = u (0.0f, 0.25f);
            if (style == Style::Drill) { p.punch = u (0.6f, 0.95f); decayB += 0.1f; }
            break;
        case DrumType::Snare:
            p.pitch = u (160.0f, 250.0f);
            if (style == Style::BoomBap) { p.pitch = u (150.0f, 200.0f); p.noise = u (0.35f, 0.6f); }
            break;
        case DrumType::Clap:
            p.pitch = u (0.8f, 1.25f);
            break;
        case DrumType::HiHat:
        case DrumType::OpenHat:
            p.pitch = u (0.85f, 1.3f);
            p.decay = type == DrumType::HiHat ? u (0.1f, 0.6f) : u (0.3f, 0.8f);
            break;
        case DrumType::Perc:
            p.pitch = u (170.0f, 480.0f);
            break;
        case DrumType::Rim:
            p.pitch = u (0.85f, 1.2f);
            break;
        default: break;
    }

    p.tone  = clamp01 (p.tone + toneB);
    p.decay = clamp01 (p.decay + decayB);
    p.drive = clamp01 (p.drive + driveB);
    p.punch = clamp01 (p.punch + punchB);
    return p;
}

std::string DrumSynth::randomName (DrumType t, std::mt19937& rng)
{
    static const char* words[] = { "Halo", "Seraph", "Cloud", "Nimbus", "Gold", "Glow", "Astral", "Zion", "Aura",
                                   "Pearl", "Velvet", "Nova", "Gospel", "Grace", "Ether", "Choir", "Heavy", "Lux" };
    std::uniform_int_distribution<int> w (0, (int) (sizeof (words) / sizeof (words[0])) - 1);
    std::uniform_int_distribution<int> num (1, 99);
    return std::string (words[w (rng)]) + " " + typeName (t) + " " + std::to_string (num (rng));
}

DrumSound DrumSynth::fromParams (DrumType type, const DrumParams& p, double sr, const std::string& name)
{
    DrumSound s;
    s.type = type;
    s.params = p;
    s.sampleRate = sr;
    s.samples = render (type, p, sr);
    s.name = name;
    return s;
}

DrumSound DrumSynth::generate (DrumType type, Style style, std::mt19937& rng, double sr)
{
    const auto p = randomParams (type, style, rng);
    return fromParams (type, p, sr, randomName (type, rng));
}

const char* DrumSynth::typeName (DrumType t)
{
    switch (t)
    {
        case DrumType::Kick: return "Kick";
        case DrumType::Bass808: return "808";
        case DrumType::Snare: return "Snare";
        case DrumType::Clap: return "Clap";
        case DrumType::HiHat: return "Hi-Hat";
        case DrumType::OpenHat: return "Open Hat";
        case DrumType::Perc: return "Perc";
        case DrumType::Rim: return "Rim";
        default: return "Drum";
    }
}

const char* DrumSynth::styleName (Style s)
{
    switch (s)
    {
        case Style::Trap: return "Trap";
        case Style::BoomBap: return "Boom Bap";
        case Style::Drill: return "Drill";
        case Style::LoFi: return "Lo-Fi";
        case Style::House: return "House";
        default: return "Trap";
    }
}

int DrumSynth::midiNote (DrumType t)
{
    switch (t)
    {
        case DrumType::Kick: return 36;
        case DrumType::Bass808: return 35;
        case DrumType::Snare: return 38;
        case DrumType::Clap: return 39;
        case DrumType::HiHat: return 42;
        case DrumType::OpenHat: return 46;
        case DrumType::Perc: return 47;
        case DrumType::Rim: return 37;
        default: return 36;
    }
}
} // namespace ph
