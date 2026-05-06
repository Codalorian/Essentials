#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

class StereoDelay
{
public:
    float time     = 0.375f;  // seconds
    float feedback = 0.4f;
    float wet      = 0.25f;

    void init(int sampleRate)
    {
        sr = sampleRate;
        int maxSamples = (int)(sampleRate * 2.5f);  // 2.5 s max
        bufL.assign(maxSamples, 0.0f);
        bufR.assign(maxSamples, 0.0f);
        idxL = idxR = 0;
    }

    void process(float* left, float* right, int n)
    {
        int delaySamples = std::max(1, std::min((int)(time * sr), (int)bufL.size() - 1));

        for (int i = 0; i < n; ++i)
        {
            int readL = ((int)bufL.size() + idxL - delaySamples) % (int)bufL.size();
            int readR = ((int)bufR.size() + idxR - delaySamples) % (int)bufR.size();

            float wetL = bufL[readL];
            float wetR = bufR[readR];

            bufL[idxL] = left[i]  + wetL * feedback;
            bufR[idxR] = right[i] + wetR * feedback;

            left[i]  = left[i]  * (1.0f - wet) + wetL * wet;
            right[i] = right[i] * (1.0f - wet) + wetR * wet;

            if (++idxL >= (int)bufL.size()) idxL = 0;
            if (++idxR >= (int)bufR.size()) idxR = 0;
        }
    }

private:
    std::vector<float> bufL, bufR;
    int idxL = 0, idxR = 0;
    int sr   = 44100;
};
