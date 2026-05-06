#include "PluginProcessor.h"
#include "PluginEditor.h"

// ── Parameter IDs (match APVTS layout) ──────────────────────────────────────
namespace ParamID
{
    // Wave selectors stored as float indices; convert to string when needed
    static const juce::StringArray WAVES {"sine","soft","triangle","sawtooth","square","bell","gamelan"};
    static const juce::StringArray FMODES{"lowpass","highpass","bandpass"};
    static const juce::StringArray LTGTS {"pitch","filter","volume"};
    static const juce::StringArray LWAVES{"sine","triangle","sawtooth","square"};
}

juce::AudioProcessorValueTreeState::ParameterLayout OpenOmniProcessor::createParameterLayout()
{
    using APVTS = juce::AudioProcessorValueTreeState;
    using Param = juce::AudioParameterFloat;
    using PChoice = juce::AudioParameterChoice;
    using PBool   = juce::AudioParameterBool;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto f = [&](const juce::String& id, const juce::String& name,
                 float lo, float hi, float def)
    {
        params.push_back(std::make_unique<Param>(id, name,
            juce::NormalisableRange<float>(lo, hi), def));
    };
    auto choice = [&](const juce::String& id, const juce::String& name,
                      juce::StringArray choices, int def)
    {
        params.push_back(std::make_unique<PChoice>(id, name, choices, def));
    };
    auto b = [&](const juce::String& id, const juce::String& name, bool def)
    {
        params.push_back(std::make_unique<PBool>(id, name, def));
    };

    // OSC A
    choice("osc_a_wave",  "Osc A Wave",  ParamID::WAVES, 5);  // bell=5
    f("osc_a_level", "Osc A Level", 0.0f, 1.0f,  0.78f);
    f("osc_a_oct",   "Osc A Oct",  -2.0f, 2.0f,  0.0f);
    f("osc_a_semi",  "Osc A Semi", -12.0f,12.0f, 0.0f);
    f("osc_a_fine",  "Osc A Fine", -100.0f,100.0f,0.0f);

    // OSC B
    choice("osc_b_wave",  "Osc B Wave",  ParamID::WAVES, 6);  // gamelan=6
    f("osc_b_level", "Osc B Level", 0.0f, 1.0f,  0.62f);
    f("osc_b_oct",   "Osc B Oct",  -2.0f, 2.0f,  1.0f);
    f("osc_b_semi",  "Osc B Semi", -12.0f,12.0f, 5.0f);
    f("osc_b_fine",  "Osc B Fine", -100.0f,100.0f,-12.0f);
    b("osc_b_en",    "Osc B Enable", true);

    // Amplitude envelope
    f("attack",  "Attack",  0.001f, 4.0f,   0.004f);
    f("decay",   "Decay",   0.01f,  4.0f,   0.90f);
    f("sustain", "Sustain", 0.0f,   1.0f,   0.07f);
    f("release", "Release", 0.01f,  8.0f,   4.2f);

    // Filter envelope
    f("fenv_attack",  "F.Env Att",  0.001f, 4.0f,  0.003f);
    f("fenv_decay",   "F.Env Dec",  0.01f,  4.0f,  0.55f);
    f("fenv_sustain", "F.Env Sus",  0.0f,   1.0f,  0.0f);
    f("fenv_release", "F.Env Rel",  0.01f,  8.0f,  2.2f);

    // Pitch envelope
    f("penv_amount", "P.Env Amt", 0.0f, 6.0f,   2.2f);
    f("penv_attack", "P.Env Att", 0.001f, 0.2f,  0.001f);
    f("penv_decay",  "P.Env Dec", 0.01f,  1.0f,  0.20f);

    // Filter
    choice("filter_mode", "Filter Mode", ParamID::FMODES, 0);
    f("filter_cutoff",    "Filter Cutoff",   30.0f, 18000.0f, 5800.0f);
    f("filter_resonance", "Filter Resonance", 0.1f,    8.0f,  0.38f);
    f("filter_env_amt",   "Filter Env Amt",   0.0f,    1.0f,  0.28f);
    f("filter_keytrack",  "Filter Keytrack",  0.0f,    1.0f,  0.55f);

    // LFO
    choice("lfo_wave",   "LFO Wave",   ParamID::LWAVES, 0);
    choice("lfo_target", "LFO Target", ParamID::LTGTS,  0);
    f("lfo_rate",  "LFO Rate",  0.01f, 20.0f, 0.22f);
    f("lfo_depth", "LFO Depth", 0.0f,  1.0f,  0.14f);

    // Reverb
    f("reverb_size", "Reverb Size",  0.0f, 1.0f, 0.92f);
    f("reverb_damp", "Reverb Damp",  0.0f, 1.0f, 0.28f);
    f("reverb_wet",  "Reverb Wet",   0.0f, 1.0f, 0.70f);

    // Delay
    f("delay_time",     "Delay Time",     0.01f, 2.0f,  0.333f);
    f("delay_feedback", "Delay Feedback", 0.0f,  0.95f, 0.44f);
    f("delay_wet",      "Delay Wet",      0.0f,  1.0f,  0.32f);

    // Chorus
    f("chorus_rate",  "Chorus Rate",  0.01f, 10.0f, 0.35f);
    f("chorus_depth", "Chorus Depth", 0.0f,  0.02f, 0.003f);
    f("chorus_wet",   "Chorus Wet",   0.0f,  1.0f,  0.42f);

    // Unison / Glide / Master
    f("unison_voices", "Unison Voices", 1.0f, 8.0f,  3.0f);
    f("unison_detune", "Unison Detune", 0.0f, 50.0f, 10.0f);
    f("glide_time",    "Glide Time",    0.0f, 2.0f,  0.04f);
    f("master_volume", "Master Volume", 0.0f, 1.0f,  0.78f);

    return {params.begin(), params.end()};
}

// ── Constructor ──────────────────────────────────────────────────────────────
OpenOmniProcessor::OpenOmniProcessor()
    : AudioProcessor(BusesProperties()
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "STATE", createParameterLayout())
{
}

// ── Helpers ──────────────────────────────────────────────────────────────────
float OpenOmniProcessor::getP(const juce::String& id) const
{
    return apvts.getRawParameterValue(id)->load();
}

int OpenOmniProcessor::getPI(const juce::String& id) const
{
    return (int)std::round(apvts.getRawParameterValue(id)->load());
}

bool OpenOmniProcessor::getPB(const juce::String& id) const
{
    return apvts.getRawParameterValue(id)->load() > 0.5f;
}

std::string OpenOmniProcessor::getPS(const juce::String& id) const
{
    auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(id));
    if (p) return p->getCurrentChoiceName().toStdString();
    return "";
}

// ── Prepare / Render ─────────────────────────────────────────────────────────
void OpenOmniProcessor::prepareToPlay(double sampleRate, int)
{
    synth.init((int)sampleRate);
    syncParamsToSynth();
}

void OpenOmniProcessor::syncParamsToSynth()
{
    auto& p = synth.getCurrentPreset();
    auto& v = p.voice;

    v.oscAWave    = getPS("osc_a_wave");
    v.oscALevel   = getP("osc_a_level");
    v.oscAOct     = getPI("osc_a_oct");
    v.oscASemi    = getPI("osc_a_semi");
    v.oscAFine    = getP("osc_a_fine");

    v.oscBWave    = getPS("osc_b_wave");
    v.oscBLevel   = getP("osc_b_level");
    v.oscBOct     = getPI("osc_b_oct");
    v.oscBSemi    = getPI("osc_b_semi");
    v.oscBFine    = getP("osc_b_fine");
    v.oscBEnabled = getPB("osc_b_en");

    v.attack  = getP("attack");
    v.decay   = getP("decay");
    v.sustain = getP("sustain");
    v.release = getP("release");

    v.fenvAttack  = getP("fenv_attack");
    v.fenvDecay   = getP("fenv_decay");
    v.fenvSustain = getP("fenv_sustain");
    v.fenvRelease = getP("fenv_release");

    v.penvAmount = getP("penv_amount");
    v.penvAttack = getP("penv_attack");
    v.penvDecay  = getP("penv_decay");

    v.filterMode      = getPS("filter_mode");
    v.filterCutoff    = getP("filter_cutoff");
    v.filterResonance = getP("filter_resonance");
    v.filterEnvAmt    = getP("filter_env_amt");
    v.filterKeytrack  = getP("filter_keytrack");

    v.lfoTarget = getPS("lfo_target");
    v.lfoDepth  = getP("lfo_depth");

    p.lfoWave = getPS("lfo_wave");
    p.lfoRate = getP("lfo_rate");

    p.reverbSize = getP("reverb_size");
    p.reverbDamp = getP("reverb_damp");
    p.reverbWet  = getP("reverb_wet");

    p.delayTime     = getP("delay_time");
    p.delayFeedback = getP("delay_feedback");
    p.delayWet      = getP("delay_wet");

    p.chorusRate  = getP("chorus_rate");
    p.chorusDepth = getP("chorus_depth");
    p.chorusWet   = getP("chorus_wet");

    v.unisonVoices = (int)std::round(getP("unison_voices"));
    v.unisonDetune = getP("unison_detune");
    v.glideTime    = getP("glide_time");
    p.masterVolume = getP("master_volume");

    synth.updateEffects();
}

void OpenOmniProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    syncParamsToSynth();

    int numSamples = buffer.getNumSamples();
    auto* left  = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : left;

    // Process MIDI events
    for (const auto meta : midiMessages)
    {
        auto msg = meta.getMessage();
        if      (msg.isNoteOn())  synth.noteOn(msg.getNoteNumber(), msg.getVelocity());
        else if (msg.isNoteOff()) synth.noteOff(msg.getNoteNumber());
        else if (msg.isAllNotesOff()) synth.allNotesOff();
        else if (msg.isPitchWheel())
        {
            // Range: ±2 semitones
            float bend = (msg.getPitchWheelValue() - 8192) / 8192.0f * 2.0f;
            synth.setPitchBend(bend);
        }
    }

    synth.render(left, right, numSamples);
}

// ── Editor ───────────────────────────────────────────────────────────────────
juce::AudioProcessorEditor* OpenOmniProcessor::createEditor()
{
    return new OpenOmniEditor(*this);
}

// ── Programs ─────────────────────────────────────────────────────────────────
void OpenOmniProcessor::setCurrentProgram(int index)
{
    auto names = synth.getPresetNames();
    if (index >= 0 && index < (int)names.size())
    {
        currentProgram = index;
        synth.loadPresetByName(names[index]);
        // Sync APVTS back from preset (basic sync — preset values become new defaults)
    }
}

const juce::String OpenOmniProcessor::getProgramName(int index)
{
    auto names = synth.getPresetNames();
    if (index >= 0 && index < (int)names.size())
        return names[index];
    return "Unknown";
}

// ── State ────────────────────────────────────────────────────────────────────
void OpenOmniProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void OpenOmniProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

// ── Factory ──────────────────────────────────────────────────────────────────
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OpenOmniProcessor();
}
