#pragma once
#include <JuceHeader.h>
#include "Engine/Synth.h"

class OpenOmniProcessor : public juce::AudioProcessor
{
public:
    OpenOmniProcessor();
    ~OpenOmniProcessor() override = default;

    // ── AudioProcessor interface ────────────────────────────────────────────
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "OpenOmni"; }
    bool acceptsMidi()  const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int  getNumPrograms()    override { return (int)synth.getPresetNames().size(); }
    int  getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ── APVTS ───────────────────────────────────────────────────────────────
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void syncParamsToSynth();

    OpenOmniSynth& getSynth() { return synth; }

private:
    OpenOmniSynth synth;
    int currentProgram = 0;

    // Convenience parameter accessors
    float getP(const juce::String& id) const;
    int   getPI(const juce::String& id) const;
    bool  getPB(const juce::String& id) const;
    std::string getPS(const juce::String& id) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenOmniProcessor)
};
