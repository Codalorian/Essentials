#pragma once
#include "Voice.h"
#include "LFO.h"
#include "Reverb.h"
#include "Delay.h"
#include "Chorus.h"
#include <array>
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include <algorithm>
#include <cmath>

static constexpr int MAX_VOICES = 16;

struct PresetData
{
    std::string name;
    SynthParams voice;
    // Effects
    float reverbSize     = 0.85f;
    float reverbDamp     = 0.5f;
    float reverbWet      = 0.6f;
    float delayTime      = 0.375f;
    float delayFeedback  = 0.4f;
    float delayWet       = 0.25f;
    float chorusRate     = 0.5f;
    float chorusDepth    = 0.003f;
    float chorusWet      = 0.4f;
    std::string lfoWave  = "sine";
    float lfoRate        = 0.3f;
    float masterVolume   = 0.8f;
};

class OpenOmniSynth
{
public:
    OpenOmniSynth()
    {
        init(44100);
    }

    void init(int sampleRate)
    {
        sr = sampleRate;
        for (auto& v : voices) v.setSampleRate(sampleRate);
        lfo.setSampleRate(sampleRate);
        reverb.init(sampleRate);
        delay.init(sampleRate);
        chorus.init(sampleRate);
        loadPresetByName("Gamelan Wheel");
    }

    // ── Parameters ──────────────────────────────────────────────────────────

    void applyPreset(const PresetData& p)
    {
        std::lock_guard<std::mutex> lock(mtx);
        current = p;
        lfo.rate     = p.lfoRate;
        lfo.waveform = p.lfoWave;
        reverb.setParams(p.reverbSize, p.reverbDamp, p.reverbWet);
        delay.time     = p.delayTime;
        delay.feedback = p.delayFeedback;
        delay.wet      = p.delayWet;
        chorus.rate    = p.chorusRate;
        chorus.depth   = p.chorusDepth;
        chorus.wet     = p.chorusWet;
    }

    PresetData& getCurrentPreset() { return current; }

    // Call after any parameter change to push effects params
    void updateEffects()
    {
        lfo.rate     = current.lfoRate;
        lfo.waveform = current.lfoWave;
        reverb.setParams(current.reverbSize, current.reverbDamp, current.reverbWet);
        delay.time     = current.delayTime;
        delay.feedback = current.delayFeedback;
        delay.wet      = current.delayWet;
        chorus.rate    = current.chorusRate;
        chorus.depth   = current.chorusDepth;
        chorus.wet     = current.chorusWet;
    }

    // ── MIDI ────────────────────────────────────────────────────────────────

    void noteOn(int note, int velocity)
    {
        std::lock_guard<std::mutex> lock(mtx);
        int idx = stealVoice();
        SynthParams vp = current.voice;
        vp.sampleRate  = sr;
        voices[idx].noteOn(note, velocity, vp);
        voices[idx].age  = age++;
        voices[idx].note = note;
        active[note]     = idx;
    }

    void noteOff(int note)
    {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = active.find(note);
        if (it != active.end())
        {
            voices[it->second].noteOff();
            active.erase(it);
        }
    }

    void allNotesOff()
    {
        std::lock_guard<std::mutex> lock(mtx);
        for (auto& v : voices) v.noteOff();
        active.clear();
    }

    void setPitchBend(float semitones)
    {
        current.voice.pitchBendSemi = semitones;
    }

    // ── Render ──────────────────────────────────────────────────────────────

    void render(float* left, float* right, int n)
    {
        std::fill(left,  left  + n, 0.0f);
        std::fill(right, right + n, 0.0f);

        float lfoVal = lfo.tick();
        current.voice.lfoVal = lfoVal;

        std::lock_guard<std::mutex> lock(mtx);

        std::vector<float> vL(n, 0.0f), vR(n, 0.0f);
        for (auto& v : voices)
        {
            if (!v.isActive()) continue;
            std::fill(vL.begin(), vL.end(), 0.0f);
            std::fill(vR.begin(), vR.end(), 0.0f);
            SynthParams vp = current.voice;
            vp.sampleRate  = sr;
            v.render(vL.data(), vR.data(), n, vp);
            for (int i = 0; i < n; ++i) { left[i] += vL[i]; right[i] += vR[i]; }
        }

        // Soft clip
        for (int i = 0; i < n; ++i)
        {
            left[i]  = std::tanh(left[i]  * 0.65f);
            right[i] = std::tanh(right[i] * 0.65f);
        }

        chorus.process(left, right, n);
        delay.process(left,  right, n);
        reverb.process(left, right, n);

        float vol = current.masterVolume;
        for (int i = 0; i < n; ++i)
        {
            left[i]  = std::max(-1.0f, std::min(1.0f, left[i]  * vol));
            right[i] = std::max(-1.0f, std::min(1.0f, right[i] * vol));
        }
    }

    // ── Presets ─────────────────────────────────────────────────────────────

    std::vector<std::string> getPresetNames() const
    {
        return {"Gamelan Wheel","Moonlight Bells","Dream Pad",
                "Sad Bells","Solar Wind","Plucked Koto","Glass Keys"};
    }

    void loadPresetByName(const std::string& name)
    {
        if      (name == "Moonlight Bells") applyPreset(moonlightBells());
        else if (name == "Dream Pad")       applyPreset(dreamPad());
        else if (name == "Sad Bells")       applyPreset(sadBells());
        else if (name == "Solar Wind")      applyPreset(solarWind());
        else if (name == "Plucked Koto")    applyPreset(pluckedKoto());
        else if (name == "Glass Keys")      applyPreset(glassKeys());
        else                                applyPreset(gamelanWheel());
    }

private:
    int                          sr  = 44100;
    int                          age = 0;
    std::array<Voice, MAX_VOICES> voices;
    LFO                          lfo;
    OmniReverb                   reverb;
    StereoDelay                  delay;
    Chorus                       chorus;
    PresetData                   current;
    std::unordered_map<int,int>  active;
    std::mutex                   mtx;

    int stealVoice()
    {
        for (int i = 0; i < MAX_VOICES; ++i)
            if (!voices[i].isActive()) return i;
        int oldest = 0;
        for (int i = 1; i < MAX_VOICES; ++i)
            if (voices[i].age < voices[oldest].age) oldest = i;
        return oldest;
    }

    // ── Preset factories ────────────────────────────────────────────────────

    static PresetData gamelanWheel()
    {
        PresetData p;
        p.name = "Gamelan Wheel";
        auto& v = p.voice;
        v.oscAWave="bell";  v.oscALevel=0.78f; v.oscAOct=0; v.oscASemi=0;  v.oscAFine=0.0f;
        v.oscBWave="gamelan"; v.oscBLevel=0.62f; v.oscBOct=1; v.oscBSemi=5; v.oscBFine=-12.0f;
        v.oscBEnabled=true;
        v.attack=0.004f; v.decay=0.90f; v.sustain=0.07f; v.release=4.2f;
        v.fenvAttack=0.003f; v.fenvDecay=0.55f; v.fenvSustain=0.0f; v.fenvRelease=2.2f;
        v.filterMode="lowpass"; v.filterCutoff=5800.0f; v.filterResonance=0.38f;
        v.filterEnvAmt=0.28f; v.filterKeytrack=0.55f;
        v.lfoTarget="filter"; v.lfoDepth=0.14f;
        v.unisonVoices=3; v.unisonDetune=10.0f;
        v.glideTime=0.04f;
        v.penvAmount=2.2f; v.penvAttack=0.001f; v.penvDecay=0.20f;
        p.lfoWave="sine"; p.lfoRate=0.22f;
        p.reverbSize=0.92f; p.reverbDamp=0.28f; p.reverbWet=0.70f;
        p.delayTime=0.333f; p.delayFeedback=0.44f; p.delayWet=0.32f;
        p.chorusRate=0.35f; p.chorusDepth=0.003f; p.chorusWet=0.42f;
        p.masterVolume=0.78f;
        return p;
    }

    static PresetData moonlightBells()
    {
        PresetData p;
        p.name = "Moonlight Bells";
        auto& v = p.voice;
        v.oscAWave="sine"; v.oscALevel=0.85f; v.oscAOct=0; v.oscASemi=0; v.oscAFine=0.0f;
        v.oscBWave="soft"; v.oscBLevel=0.45f; v.oscBOct=1; v.oscBSemi=7; v.oscBFine=-4.0f;
        v.oscBEnabled=true;
        v.attack=0.06f; v.decay=0.4f; v.sustain=0.55f; v.release=2.8f;
        v.fenvAttack=0.01f; v.fenvDecay=0.6f; v.fenvSustain=0.0f; v.fenvRelease=1.5f;
        v.filterMode="lowpass"; v.filterCutoff=3800.0f; v.filterResonance=0.28f;
        v.filterEnvAmt=0.35f; v.filterKeytrack=0.3f;
        v.lfoTarget="pitch"; v.lfoDepth=0.18f;
        v.unisonVoices=2; v.unisonDetune=6.0f;
        v.glideTime=0.0f;
        v.penvAmount=1.8f; v.penvAttack=0.001f; v.penvDecay=0.22f;
        p.lfoWave="sine"; p.lfoRate=0.32f;
        p.reverbSize=0.88f; p.reverbDamp=0.45f; p.reverbWet=0.62f;
        p.delayTime=0.375f; p.delayFeedback=0.38f; p.delayWet=0.24f;
        p.chorusRate=0.45f; p.chorusDepth=0.0028f; p.chorusWet=0.38f;
        p.masterVolume=0.80f;
        return p;
    }

    static PresetData dreamPad()
    {
        PresetData p;
        p.name = "Dream Pad";
        auto& v = p.voice;
        v.oscAWave="soft"; v.oscALevel=0.9f; v.oscAOct=0; v.oscASemi=0; v.oscAFine=0.0f;
        v.oscBWave="triangle"; v.oscBLevel=0.5f; v.oscBOct=0; v.oscBSemi=0; v.oscBFine=7.0f;
        v.oscBEnabled=true;
        v.attack=0.6f; v.decay=0.5f; v.sustain=0.75f; v.release=3.5f;
        v.fenvAttack=0.8f; v.fenvDecay=1.0f; v.fenvSustain=0.3f; v.fenvRelease=2.0f;
        v.filterMode="lowpass"; v.filterCutoff=1800.0f; v.filterResonance=0.2f;
        v.filterEnvAmt=0.5f; v.filterKeytrack=0.4f;
        v.lfoTarget="pitch"; v.lfoDepth=0.08f;
        v.unisonVoices=4; v.unisonDetune=14.0f;
        v.glideTime=0.12f;
        v.penvAmount=0.0f; v.penvAttack=0.001f; v.penvDecay=0.1f;
        p.lfoWave="sine"; p.lfoRate=0.2f;
        p.reverbSize=0.92f; p.reverbDamp=0.55f; p.reverbWet=0.75f;
        p.delayTime=0.50f; p.delayFeedback=0.35f; p.delayWet=0.22f;
        p.chorusRate=0.3f; p.chorusDepth=0.004f; p.chorusWet=0.5f;
        p.masterVolume=0.75f;
        return p;
    }

    static PresetData sadBells()
    {
        PresetData p;
        p.name = "Sad Bells";
        auto& v = p.voice;
        v.oscAWave="sine"; v.oscALevel=0.9f; v.oscAOct=0; v.oscASemi=0; v.oscAFine=0.0f;
        v.oscBWave="sine"; v.oscBLevel=0.35f; v.oscBOct=2; v.oscBSemi=0; v.oscBFine=2.0f;
        v.oscBEnabled=true;
        v.attack=0.01f; v.decay=0.8f; v.sustain=0.1f; v.release=3.0f;
        v.fenvAttack=0.01f; v.fenvDecay=0.4f; v.fenvSustain=0.0f; v.fenvRelease=1.0f;
        v.filterMode="lowpass"; v.filterCutoff=5000.0f; v.filterResonance=0.4f;
        v.filterEnvAmt=0.2f; v.filterKeytrack=0.0f;
        v.lfoTarget="pitch"; v.lfoDepth=0.1f;
        v.unisonVoices=1; v.unisonDetune=0.0f;
        v.glideTime=0.0f;
        v.penvAmount=0.0f; v.penvAttack=0.001f; v.penvDecay=0.1f;
        p.lfoWave="sine"; p.lfoRate=0.5f;
        p.reverbSize=0.9f; p.reverbDamp=0.35f; p.reverbWet=0.7f;
        p.delayTime=0.333f; p.delayFeedback=0.42f; p.delayWet=0.3f;
        p.chorusRate=0.6f; p.chorusDepth=0.002f; p.chorusWet=0.25f;
        p.masterVolume=0.78f;
        return p;
    }

    static PresetData solarWind()
    {
        PresetData p;
        p.name = "Solar Wind";
        auto& v = p.voice;
        v.oscAWave="sawtooth"; v.oscALevel=0.8f; v.oscAOct=0; v.oscASemi=0; v.oscAFine=0.0f;
        v.oscBWave="sawtooth"; v.oscBLevel=0.55f; v.oscBOct=0; v.oscBSemi=0; v.oscBFine=8.0f;
        v.oscBEnabled=true;
        v.attack=0.02f; v.decay=0.2f; v.sustain=0.8f; v.release=0.5f;
        v.fenvAttack=0.01f; v.fenvDecay=0.15f; v.fenvSustain=0.5f; v.fenvRelease=0.3f;
        v.filterMode="lowpass"; v.filterCutoff=2800.0f; v.filterResonance=0.6f;
        v.filterEnvAmt=0.45f; v.filterKeytrack=0.7f;
        v.lfoTarget="volume"; v.lfoDepth=0.22f;
        v.unisonVoices=2; v.unisonDetune=8.0f;
        v.glideTime=0.0f;
        v.penvAmount=0.0f; v.penvAttack=0.001f; v.penvDecay=0.1f;
        p.lfoWave="sine"; p.lfoRate=5.5f;
        p.reverbSize=0.72f; p.reverbDamp=0.6f; p.reverbWet=0.35f;
        p.delayTime=0.25f; p.delayFeedback=0.3f; p.delayWet=0.18f;
        p.chorusRate=0.8f; p.chorusDepth=0.005f; p.chorusWet=0.32f;
        p.masterVolume=0.82f;
        return p;
    }

    static PresetData pluckedKoto()
    {
        PresetData p;
        p.name = "Plucked Koto";
        auto& v = p.voice;
        v.oscAWave="triangle"; v.oscALevel=0.85f; v.oscAOct=0; v.oscASemi=0; v.oscAFine=0.0f;
        v.oscBWave="bell"; v.oscBLevel=0.50f; v.oscBOct=2; v.oscBSemi=0; v.oscBFine=5.0f;
        v.oscBEnabled=true;
        v.attack=0.002f; v.decay=0.55f; v.sustain=0.12f; v.release=3.5f;
        v.fenvAttack=0.001f; v.fenvDecay=0.3f; v.fenvSustain=0.0f; v.fenvRelease=1.5f;
        v.filterMode="lowpass"; v.filterCutoff=7200.0f; v.filterResonance=0.32f;
        v.filterEnvAmt=0.3f; v.filterKeytrack=0.65f;
        v.lfoTarget="pitch"; v.lfoDepth=0.06f;
        v.unisonVoices=2; v.unisonDetune=5.0f;
        v.glideTime=0.0f;
        v.penvAmount=1.2f; v.penvAttack=0.001f; v.penvDecay=0.15f;
        p.lfoWave="sine"; p.lfoRate=0.4f;
        p.reverbSize=0.85f; p.reverbDamp=0.4f; p.reverbWet=0.55f;
        p.delayTime=0.286f; p.delayFeedback=0.36f; p.delayWet=0.22f;
        p.chorusRate=0.5f; p.chorusDepth=0.002f; p.chorusWet=0.28f;
        p.masterVolume=0.80f;
        return p;
    }

    static PresetData glassKeys()
    {
        PresetData p;
        p.name = "Glass Keys";
        auto& v = p.voice;
        v.oscAWave="bell"; v.oscALevel=0.72f; v.oscAOct=0; v.oscASemi=0; v.oscAFine=0.0f;
        v.oscBWave="soft"; v.oscBLevel=0.58f; v.oscBOct=1; v.oscBSemi=0; v.oscBFine=-3.0f;
        v.oscBEnabled=true;
        v.attack=0.35f; v.decay=0.6f; v.sustain=0.85f; v.release=5.0f;
        v.fenvAttack=0.5f; v.fenvDecay=1.2f; v.fenvSustain=0.15f; v.fenvRelease=3.0f;
        v.filterMode="lowpass"; v.filterCutoff=3200.0f; v.filterResonance=0.18f;
        v.filterEnvAmt=0.4f; v.filterKeytrack=0.5f;
        v.lfoTarget="filter"; v.lfoDepth=0.10f;
        v.unisonVoices=4; v.unisonDetune=18.0f;
        v.glideTime=0.08f;
        v.penvAmount=0.0f; v.penvAttack=0.001f; v.penvDecay=0.1f;
        p.lfoWave="sine"; p.lfoRate=0.28f;
        p.reverbSize=0.95f; p.reverbDamp=0.2f; p.reverbWet=0.78f;
        p.delayTime=0.5f; p.delayFeedback=0.28f; p.delayWet=0.20f;
        p.chorusRate=0.22f; p.chorusDepth=0.006f; p.chorusWet=0.55f;
        p.masterVolume=0.76f;
        return p;
    }
};
