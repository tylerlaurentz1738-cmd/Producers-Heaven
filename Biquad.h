#pragma once
#include <cmath>
#include <vector>

namespace ph
{
constexpr float kPi = 3.14159265358979f;

// Simple RBJ biquad used for offline rendering and analysis (no JUCE dependency).
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }

    void setLowPass (double sr, double f, double q = 0.7071) { design (sr, f, q, 0); }
    void setHighPass (double sr, double f, double q = 0.7071) { design (sr, f, q, 1); }
    void setBandPass (double sr, double f, double q = 1.0) { design (sr, f, q, 2); }

    inline float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void processInPlace (std::vector<float>& v)
    {
        for (auto& s : v) s = process (s);
    }

private:
    void design (double sr, double f, double q, int type)
    {
        f = std::fmin (std::fmax (f, 10.0), sr * 0.45);
        const double w0 = 2.0 * 3.14159265358979 * f / sr;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * q);
        double nb0, nb1, nb2;
        const double na0 = 1 + alpha, na1 = -2 * cw, na2 = 1 - alpha;
        if (type == 0)      { nb0 = (1 - cw) / 2; nb1 = 1 - cw; nb2 = (1 - cw) / 2; }
        else if (type == 1) { nb0 = (1 + cw) / 2; nb1 = -(1 + cw); nb2 = (1 + cw) / 2; }
        else                { nb0 = alpha; nb1 = 0; nb2 = -alpha; }
        b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0);
        a1 = (float) (na1 / na0); a2 = (float) (na2 / na0);
    }
};

inline std::vector<float> filtered (const std::vector<float>& in, double sr, int type, double f, double q = 0.7071)
{
    Biquad b;
    if (type == 0) b.setLowPass (sr, f, q);
    else if (type == 1) b.setHighPass (sr, f, q);
    else b.setBandPass (sr, f, q);
    std::vector<float> out (in.size());
    for (size_t i = 0; i < in.size(); ++i) out[i] = b.process (in[i]);
    return out;
}

inline float dbOf (double linear) { return (float) (20.0 * std::log10 (std::fmax (linear, 1.0e-9))); }
} // namespace ph
