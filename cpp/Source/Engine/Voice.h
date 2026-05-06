#pragma once
#include "Oscillator.h"
#include "Envelope.h"
#include "Filter.h"
#include <cmath>
#include <string>
#include <array>
#include <vector>

static constexpr float A4_HZ   = 440.0f;
static constexpr int   A4_MIDI = 69;

inline float midiToHz(int note)
{
    return A4_HZ * std::pow(2.0f, (note - A4_MIDI) / 12.0f);
}

struct SynthParams
{
    // Oscillator A
    std::string oscAWave  = "bell";
    float       oscALevel = 0.78f;
    int         oscAOct   = 0;
    int         oscASemi  = 0;
    float       oscAFine  = 0.0f;

    // Oscillator B
    std::string oscBWave    = "gamelan";
    float       oscBLevel   = 0.62f;
    int         oscBOct     = 1;
    int         oscBSemi    = 5;
    float       oscBFine    = -12.0f;
    bool        oscBEnabled = true;

    // Amplitude envelope
    float attack  = 0.004f;
    float decay   = 0.90f;
    float sustain = 0.07f;
    float release = 4.2f;

    // Filter envelope
    float fenvAttack  = 0.003f;
    float fenvDecay   = 0.55f;
    float fenvSustain = 0.0f;
    float fenvRelease = 2.2f;

    // Pitch envelope
    float penvAmount = 2.2f;
    float penvAttack = 0.001f;
    float penvDecay  = 0.20f;

    // Filter
    std::string filterMode      = "lowpass";
    float       filterCutoff    = 5800.0f;
    float       filterResonance = 0.38f;
    float       filterEnvAmt    = 0.28f;
    float       filterKeytrack  = 0.55f;

    // LFO
    std::string lfoTarget = "filter";
    float       lfoDepth  = 0.14f;

    // Unison
    int   unisonVoices = 3;
    float unisonDetune = 10.0f;

    // Glide
    float glideTime = 0.04f;

    // Modulation
    float pitchBendSemi = 0.0f;
    float lfoVal        = 0.0f;

    int   sampleRate = 44100;
};

class Voice
{
public:
    int  note = -1;
    int  age  = 0;

    void setSampleRate(int sr)
    {
        sampleRate = sr;
        for (auto& e : {&ampEnv, &filtEnv, &pitchEnv})
            e->setSampleRate(sr);
        filtL.setSampleRate(sr);
        filtR.setSampleRate(sr);
    }

    void noteOn(int midiNote, int velocity, const SynthParams& p)
    {
        note   = midiNote;
        params = p;
        float freq = midiToHz(midiNote);

        oscA.waveform = p.oscAWave;
        oscB.waveform = p.oscBWave;

        float newIncA = calcInc(freq, p.oscAFine, p.oscAOct, p.oscASemi);
        float newIncB = calcInc(freq, p.oscBFine, p.oscBOct, p.oscBSemi);

        bool isGliding = (p.glideTime > 0.0f && targetIncA > 0.0f && ampEnv.isActive());

        if (isGliding)
        {
            glideStartIncA  = oscA.phaseIncrement;
            glideStartIncB  = oscB.phaseIncrement;
            glideSamples    = (int)(p.glideTime * sampleRate);
            glidePos        = 0;
        }
        else
        {
            glideStartIncA  = newIncA;
            glideStartIncB  = newIncB;
            glideSamples    = 0;
            glidePos        = 0;
            // Reset unison phases for clean attack
            int nUni = std::max(1, p.unisonVoices);
            for (int i = 0; i < 8; ++i)
            {
                unisonPhasesA[i] = std::fmod((float)i / nUni, 1.0f);
                unisonPhasesB[i] = std::fmod((float)i / nUni + 0.13f, 1.0f);
            }
        }

        oscA.phaseIncrement = isGliding ? glideStartIncA : newIncA;
        oscB.phaseIncrement = isGliding ? glideStartIncB : newIncB;
        targetIncA = newIncA;
        targetIncB = newIncB;
        baseFreq   = freq;

        ampEnv.attack  = p.attack;
        ampEnv.decay   = p.decay;
        ampEnv.sustain = p.sustain;
        ampEnv.release = p.release;
        ampEnv.noteOn(velocity);

        filtEnv.attack  = p.fenvAttack;
        filtEnv.decay   = p.fenvDecay;
        filtEnv.sustain = p.fenvSustain;
        filtEnv.release = p.fenvRelease;
        filtEnv.noteOn(velocity);

        pitchEnv.attack  = p.penvAttack;
        pitchEnv.decay   = p.penvDecay;
        pitchEnv.sustain = 0.0f;
        pitchEnv.release = 0.05f;
        pitchEnv.noteOn(velocity);
    }

    void noteOff()
    {
        ampEnv.noteOff();
        filtEnv.noteOff();
        pitchEnv.noteOff();
    }

    bool isActive() const { return ampEnv.isActive(); }

    // render fills left[0..n) and right[0..n)
    void render(float* left, float* right, int n, const SynthParams& p)
    {
        params = p;

        // Glide
        if (glidePos < glideSamples)
        {
            float t = std::min((float)glidePos / glideSamples, 1.0f);
            oscA.phaseIncrement = glideStartIncA + t * (targetIncA - glideStartIncA);
            oscB.phaseIncrement = glideStartIncB + t * (targetIncB - glideStartIncB);
            glidePos += n;
        }

        // Pitch envelope (ascending glide: starts flat, rises to natural pitch)
        std::vector<float> penvBuf(n);
        float penvMean = pitchEnv.render(penvBuf.data(), n);
        float pitchBend = p.pitchBendSemi;
        float penvRatio = std::pow(2.0f, (pitchBend - penvMean * p.penvAmount) / 12.0f);

        if (p.lfoTarget == "pitch" && p.lfoDepth > 0.0f)
            penvRatio *= std::pow(2.0f, p.lfoVal * p.lfoDepth / 12.0f);

        float baseIncA = oscA.phaseIncrement;
        float baseIncB = oscB.phaseIncrement;

        int   nUni  = std::max(1, p.unisonVoices);
        float detune = p.unisonDetune;

        // Render oscillators with unison
        std::vector<float> tmpL(n, 0.0f), tmpR(n, 0.0f);

        renderOscUnison(oscA, unisonPhasesA, tmpL.data(), tmpR.data(), n,
                        baseIncA * penvRatio, nUni, detune, p.oscALevel);

        if (p.oscBEnabled)
        {
            renderOscUnison(oscB, unisonPhasesB, tmpL.data(), tmpR.data(), n,
                            baseIncB * penvRatio, nUni, detune, p.oscBLevel);
        }

        oscA.phaseIncrement = baseIncA;
        oscB.phaseIncrement = baseIncB;

        // Mix (0.5 to combine A+B)
        for (int i = 0; i < n; ++i)
        {
            left[i]  += tmpL[i] * 0.5f;
            right[i] += tmpR[i] * 0.5f;
        }

        // Amplitude envelope
        std::vector<float> ampBuf(n);
        ampEnv.render(ampBuf.data(), n);
        for (int i = 0; i < n; ++i)
        {
            left[i]  *= ampBuf[i];
            right[i] *= ampBuf[i];
        }

        // Volume LFO tremolo
        if (p.lfoTarget == "volume" && p.lfoDepth > 0.0f)
        {
            float volMod = 1.0f + p.lfoVal * p.lfoDepth * 0.5f;
            for (int i = 0; i < n; ++i) { left[i] *= volMod; right[i] *= volMod; }
        }

        // Filter: cutoff with envelope + LFO + keytrack
        float baseCut  = p.filterCutoff;
        float envAmt   = p.filterEnvAmt;

        std::vector<float> fenvBuf(n);
        float fenvMean = filtEnv.render(fenvBuf.data(), n);
        float modCut   = baseCut * (1.0f + envAmt * fenvMean * 3.0f);

        if (p.filterKeytrack != 0.0f && note >= 0)
        {
            float keyRatio = midiToHz(note) / 440.0f;
            modCut *= std::pow(keyRatio, p.filterKeytrack);
        }

        if (p.lfoTarget == "filter" && p.lfoDepth > 0.0f)
            modCut *= 1.0f + p.lfoVal * p.lfoDepth;

        modCut = std::max(30.0f, std::min(modCut, 18000.0f));

        filtL.setParams(modCut, p.filterResonance, p.filterMode);
        filtR.setParams(modCut, p.filterResonance, p.filterMode);

        filtL.processBlock(left,  n);
        filtR.processBlock(right, n);
    }

private:
    WavetableOscillator oscA, oscB;
    OmniADSR                ampEnv, filtEnv, pitchEnv;
    SVFilter            filtL, filtR;
    SynthParams         params;
    int                 sampleRate = 44100;

    // Glide state
    float targetIncA     = 0.0f;
    float targetIncB     = 0.0f;
    float glideStartIncA = 0.0f;
    float glideStartIncB = 0.0f;
    int   glideSamples   = 0;
    int   glidePos       = 0;
    float baseFreq       = 0.0f;

    // Unison phase memory (8 slots, same as Python)
    std::array<float, 8> unisonPhasesA = {0.0f};
    std::array<float, 8> unisonPhasesB = {0.13f, 0.13f, 0.13f, 0.13f, 0.13f, 0.13f, 0.13f, 0.13f};

    float calcInc(float freq, float fine, int oct, int semi) const
    {
        float f = freq * std::pow(2.0f, oct + semi / 12.0f + fine / 1200.0f);
        return std::max(f, 0.01f) / sampleRate;
    }

    // Render osc nUni times with spread detune, accumulate into outL/outR
    void renderOscUnison(WavetableOscillator& osc, std::array<float,8>& phases,
                         float* outL, float* outR, int n,
                         float baseInc, int nUni, float detuneCents, float level)
    {
        float half = detuneCents * 0.5f;
        float norm = std::sqrt((float)nUni);

        std::vector<float> chunk(n);

        for (int vi = 0; vi < nUni; ++vi)
        {
            float cents   = (nUni > 1) ? (-half + (float)vi / (nUni - 1) * detuneCents) : 0.0f;
            float ratio   = std::pow(2.0f, cents / 1200.0f);
            osc.phase     = phases[vi];
            osc.phaseIncrement = baseInc * ratio;

            std::fill(chunk.begin(), chunk.end(), 0.0f);
            phases[vi] = osc.renderAndAdvance(chunk.data(), n);

            float panT  = (nUni > 1) ? (float)vi / (nUni - 1) : 0.5f;
            float panL  = std::cos(panT * 1.5707963f);
            float panR  = std::sin(panT * 1.5707963f);

            float scale = level / norm;
            for (int i = 0; i < n; ++i)
            {
                outL[i] += chunk[i] * panL * scale;
                outR[i] += chunk[i] * panR * scale;
            }
        }

        osc.phaseIncrement = baseInc;
    }
};
