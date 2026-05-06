#pragma once
#include <cmath>
#include <string>

class LFO
{
public:
    float       rate     = 0.5f;
    std::string waveform = "sine";

    void setSampleRate(int sr) { sampleRate = sr; }

    float tick()
    {
        phase += rate / sampleRate;
        if (phase >= 1.0f) phase -= 1.0f;

        const float twoPi = 6.28318530718f;

        if (waveform == "sine")
            return std::sin(twoPi * phase);

        if (waveform == "triangle")
            return (phase < 0.5f) ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase);

        if (waveform == "sawtooth")
            return 2.0f * phase - 1.0f;

        if (waveform == "square")
            return phase < 0.5f ? 1.0f : -1.0f;

        return std::sin(twoPi * phase);
    }

    void reset() { phase = 0.0f; }

private:
    float phase      = 0.0f;
    int   sampleRate = 44100;
};
