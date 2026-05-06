#pragma once
#include <vector>
#include <array>
#include <cmath>
#include <numeric>

// Freeverb — same tunings as the Python implementation
static constexpr std::array<int,8> COMB_L   = {1116,1188,1277,1356,1422,1491,1557,1617};
static constexpr std::array<int,8> COMB_R   = {1139,1211,1300,1379,1445,1514,1580,1640};
static constexpr std::array<int,4> APASS_L  = {556,441,341,225};
static constexpr std::array<int,4> APASS_R  = {579,464,364,248};

static constexpr float FIXED_GAIN   = 0.015f;
static constexpr float SCALE_ROOM   = 0.28f;
static constexpr float OFFSET_ROOM  = 0.7f;
static constexpr float SCALE_DAMP   = 0.4f;
static constexpr float APASS_FDBK   = 0.5f;

class CombFilter
{
public:
    float feedback = 0.84f;
    float damp1    = 0.2f;
    float damp2    = 0.8f;

    void init(int size)
    {
        buf.assign(size, 0.0f);
        idx = 0;
        filterStore = 0.0f;
    }

    float process(float input)
    {
        float output    = buf[idx];
        filterStore     = output * damp2 + filterStore * damp1;
        buf[idx]        = input + filterStore * feedback;
        if (++idx >= (int)buf.size()) idx = 0;
        return output;
    }

    void processBlock(const float* in, float* out, int n)
    {
        for (int i = 0; i < n; ++i)
            out[i] = process(in[i]);
    }

private:
    std::vector<float> buf;
    int   idx         = 0;
    float filterStore = 0.0f;
};

class AllpassFilter
{
public:
    void init(int size, float feedback = APASS_FDBK)
    {
        buf.assign(size, 0.0f);
        this->feedback = feedback;
        idx = 0;
    }

    float process(float input)
    {
        float bufOut  = buf[idx];
        float output  = -input + bufOut;
        buf[idx]      = input + bufOut * feedback;
        if (++idx >= (int)buf.size()) idx = 0;
        return output;
    }

    void processBlock(float* inout, int n)
    {
        for (int i = 0; i < n; ++i)
            inout[i] = process(inout[i]);
    }

private:
    std::vector<float> buf;
    float feedback = 0.5f;
    int   idx      = 0;
};


class Reverb
{
public:
    Reverb()
    {
        init(44100);
    }

    void init(int sampleRate)
    {
        float sc = sampleRate / 44100.0f;
        for (int i = 0; i < 8; ++i)
        {
            combsL[i].init((int)(COMB_L[i] * sc));
            combsR[i].init((int)(COMB_R[i] * sc));
        }
        for (int i = 0; i < 4; ++i)
        {
            apL[i].init((int)(APASS_L[i] * sc));
            apR[i].init((int)(APASS_R[i] * sc));
        }
        setParams(0.85f, 0.5f, 0.6f);
    }

    void setParams(float roomSize, float damp, float wet, float width = 1.0f)
    {
        this->wet      = wet;
        this->width    = width;
        float fb = roomSize * SCALE_ROOM + OFFSET_ROOM;
        float d1 = damp * SCALE_DAMP;
        float d2 = 1.0f - d1;
        for (auto& c : combsL) { c.feedback = fb; c.damp1 = d1; c.damp2 = d2; }
        for (auto& c : combsR) { c.feedback = fb; c.damp1 = d1; c.damp2 = d2; }
        wet1 = wet * (width / 2.0f + 0.5f);
        wet2 = wet * ((1.0f - width) / 2.0f);
        dry  = 1.0f - wet;
    }

    // stereo: interleaved [L0,R0, L1,R1, ...] or separate buffers
    void process(float* left, float* right, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            float mono = (left[i] + right[i]) * FIXED_GAIN;

            float outL = 0.0f, outR = 0.0f;
            for (auto& c : combsL) outL += c.process(mono);
            for (auto& c : combsR) outR += c.process(mono);
            for (auto& a : apL)    outL  = a.process(outL);
            for (auto& a : apR)    outR  = a.process(outR);

            float dryL = left[i], dryR = right[i];
            left[i]  = outL * wet1 + outR * wet2 + dryL * dry;
            right[i] = outR * wet1 + outL * wet2 + dryR * dry;
        }
    }

private:
    CombFilter   combsL[8], combsR[8];
    AllpassFilter apL[4],   apR[4];
    float wet  = 0.5f;
    float wet1 = 0.5f;
    float wet2 = 0.0f;
    float dry  = 0.5f;
    float width= 1.0f;
};
