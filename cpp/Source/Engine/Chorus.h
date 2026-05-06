#pragma once
#include <vector>
#include <cmath>

class Chorus
{
public:
    float rate  = 0.5f;   // Hz
    float depth = 0.003f; // seconds
    float wet   = 0.4f;

    void init(int sampleRate)
    {
        sr = sampleRate;
        int bufSize = (int)(sampleRate * 0.05f);  // 50 ms max
        bufL.assign(bufSize, 0.0f);
        bufR.assign(bufSize, 0.0f);
        writeIdx = 0;
        phaseL   = 0.0f;
        phaseR   = 0.5f;  // 180° offset for width
    }

    void process(float* left, float* right, int n)
    {
        int bufSize = (int)bufL.size();

        for (int i = 0; i < n; ++i)
        {
            // Write dry signal into buffer
            bufL[writeIdx] = left[i];
            bufR[writeIdx] = right[i];

            // LFO values (sine, 180° apart)
            float lfoL = 0.5f * (1.0f + std::sin(6.28318f * phaseL));
            float lfoR = 0.5f * (1.0f + std::sin(6.28318f * phaseR));

            // Delay in samples
            float delayL = depth * sr * lfoL;
            float delayR = depth * sr * lfoR;

            // Fractional read
            auto readFrac = [&](std::vector<float>& buf, float delay) -> float {
                float readPos = writeIdx - delay;
                while (readPos < 0) readPos += bufSize;
                int   r0   = (int)readPos % bufSize;
                int   r1   = (r0 + 1) % bufSize;
                float frac = readPos - (int)readPos;
                return buf[r0] * (1.0f - frac) + buf[r1] * frac;
            };

            float wetL = readFrac(bufL, delayL);
            float wetR = readFrac(bufR, delayR);

            left[i]  = left[i]  * (1.0f - wet) + wetL * wet;
            right[i] = right[i] * (1.0f - wet) + wetR * wet;

            // Advance LFO phases
            phaseL += rate / sr;
            if (phaseL >= 1.0f) phaseL -= 1.0f;
            phaseR += rate / sr;
            if (phaseR >= 1.0f) phaseR -= 1.0f;

            if (++writeIdx >= bufSize) writeIdx = 0;
        }
    }

private:
    std::vector<float> bufL, bufR;
    int   writeIdx = 0;
    float phaseL   = 0.0f;
    float phaseR   = 0.5f;
    int   sr       = 44100;
};
