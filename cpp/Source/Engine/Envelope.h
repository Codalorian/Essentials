#pragma once
#include <cmath>
#include <algorithm>

class ADSR
{
public:
    float attack  = 0.05f;
    float decay   = 0.3f;
    float sustain = 0.6f;
    float release = 2.0f;

    void setSampleRate(int sr) { sampleRate = sr; }

    void noteOn(int velocity)
    {
        velScale = velocity / 127.0f;
        state    = State::Attack;
        level    = 0.0f;
    }

    void noteOff()
    {
        if (state != State::Idle)
        {
            releaseStart = level;
            state        = State::Release;
            releasePos   = 0;
        }
    }

    bool isActive() const { return state != State::Idle; }

    // Fills buf[0..n) with the envelope, returns mean value
    float render(float* buf, int n)
    {
        float sum = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            switch (state)
            {
            case State::Idle:
                buf[i] = 0.0f;
                break;

            case State::Attack:
            {
                float dur = std::max(attack, 0.0001f) * sampleRate;
                attackPos++;
                level = (float)attackPos / dur;
                if (level >= 1.0f) { level = 1.0f; state = State::Decay; decayPos = 0; }
                buf[i] = level * velScale;
                break;
            }

            case State::Decay:
            {
                float dur = std::max(decay, 0.0001f) * sampleRate;
                decayPos++;
                level = 1.0f - (1.0f - sustain) * ((float)decayPos / dur);
                if ((float)decayPos >= dur) { level = sustain; state = State::Sustain; }
                buf[i] = level * velScale;
                break;
            }

            case State::Sustain:
                buf[i] = sustain * velScale;
                break;

            case State::Release:
            {
                float dur = std::max(release, 0.001f) * sampleRate;
                float t   = (float)releasePos / dur;
                // Exponential-shaped release
                level = releaseStart * std::pow(0.001f, t);
                releasePos++;
                if (t >= 1.0f || level < 1e-5f) { level = 0.0f; state = State::Idle; }
                buf[i] = level;
                break;
            }
            }
            sum += buf[i];
        }
        return sum / n;
    }

    // Scalar tick — returns one envelope sample
    float tick()
    {
        float v = 0.0f;
        render(&v, 1);
        return v;
    }

private:
    enum class State { Idle, Attack, Decay, Sustain, Release };
    State state     = State::Idle;
    float level     = 0.0f;
    float velScale  = 1.0f;
    float releaseStart = 0.0f;
    int   attackPos = 0;
    int   decayPos  = 0;
    int   releasePos= 0;
    int   sampleRate= 44100;
};
