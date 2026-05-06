#pragma once
#include <cmath>
#include <string>
#include <algorithm>

// TPT State Variable Filter (Andy Simper / Cytomic formulation)
// Matches the Python SVFilter exactly — same biquad coefficients.
class SVFilter
{
public:
    void setSampleRate(int sr) { sampleRate = sr; }

    void setParams(float cutoff, float resonance, const std::string& mode)
    {
        float fc  = std::max(20.0f, std::min(cutoff, sampleRate * 0.48f));
        float Q   = std::max(0.1f, std::min(resonance, 8.0f));
        float g   = std::tan(3.14159265358979f * fc / sampleRate);
        float k   = 1.0f / Q;
        float D   = 1.0f + g * (g + k);

        a1 = 2.0f * (g * g - 1.0f) / D;
        a2 = (1.0f - k * g + g * g) / D;

        if (mode == "lowpass")
        {
            float b = g * g / D;
            b0 = b; b1 = 2.0f * b; b2 = b;
        }
        else if (mode == "highpass")
        {
            float b = 1.0f / D;
            b0 = b; b1 = -2.0f * b; b2 = b;
        }
        else // bandpass
        {
            float b = g * k / D;
            b0 = b; b1 = 0.0f; b2 = -b;
        }
    }

    float process(float x)
    {
        // Direct Form II transposed biquad
        float y = b0 * x + s1;
        s1 = b1 * x - a1 * y + s2;
        s2 = b2 * x - a2 * y;
        return y;
    }

    void processBlock(float* inout, int n)
    {
        for (int i = 0; i < n; ++i)
            inout[i] = process(inout[i]);
    }

    void reset() { s1 = s2 = 0.0f; }

private:
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
    float s1 = 0.0f, s2 = 0.0f;
    int   sampleRate = 44100;
};
