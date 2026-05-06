#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class OpenOmniEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit OpenOmniEditor(OpenOmniProcessor&);
    ~OpenOmniEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void buildUI();

    using APVTS  = juce::AudioProcessorValueTreeState;
    using Attach = APVTS::SliderAttachment;
    using CAttach= APVTS::ComboBoxAttachment;
    using BAttach= APVTS::ButtonAttachment;

    OpenOmniProcessor& processor;

    // ── Sections ────────────────────────────────────────────────────────────
    struct Section
    {
        juce::GroupComponent box;
        std::vector<juce::Slider>  knobs;
        std::vector<juce::Label>   labels;
        std::vector<std::unique_ptr<Attach>> attaches;
    };

    // Oscillator A
    juce::ComboBox oscAWaveCombo;
    juce::Label    oscAWaveLabel;
    std::unique_ptr<CAttach> oscAWaveAttach;
    juce::Slider   oscALevelKnob, oscAOctKnob, oscASemiKnob, oscAFineKnob;
    juce::Label    oscALevelLabel, oscAOctLabel, oscASemiLabel, oscAFineLabel;
    std::unique_ptr<Attach> oscALevelAtt, oscAOctAtt, oscASemiAtt, oscAFineAtt;

    // Oscillator B
    juce::ComboBox oscBWaveCombo;
    juce::Label    oscBWaveLabel;
    std::unique_ptr<CAttach> oscBWaveAttach;
    juce::Slider   oscBLevelKnob, oscBOctKnob, oscBSemiKnob, oscBFineKnob;
    juce::Label    oscBLevelLabel, oscBOctLabel, oscBSemiLabel, oscBFineLabel;
    std::unique_ptr<Attach> oscBLevelAtt, oscBOctAtt, oscBSemiAtt, oscBFineAtt;
    juce::ToggleButton oscBEnableBtn {"B On"};
    std::unique_ptr<BAttach> oscBEnableAtt;

    // Amp envelope
    juce::Slider   atkKnob, decKnob, susKnob, relKnob;
    juce::Label    atkLbl, decLbl, susLbl, relLbl;
    std::unique_ptr<Attach> atkAtt, decAtt, susAtt, relAtt;

    // Filter
    juce::ComboBox filterModeCombo;
    juce::Label    filterModeLabel;
    std::unique_ptr<CAttach> filterModeAtt;
    juce::Slider   filterCutKnob, filterResKnob, filterEnvKnob, filterKtrkKnob;
    juce::Label    filterCutLbl, filterResLbl, filterEnvLbl, filterKtrkLbl;
    std::unique_ptr<Attach> filterCutAtt, filterResAtt, filterEnvAtt, filterKtrkAtt;

    // Filter envelope
    juce::Slider   fatkKnob, fdecKnob, fsusKnob, frelKnob;
    juce::Label    fatkLbl, fdecLbl, fsusLbl, frelLbl;
    std::unique_ptr<Attach> fatkAtt, fdecAtt, fsusAtt, frelAtt;

    // Pitch envelope
    juce::Slider   penvAmtKnob, penvAtkKnob, penvDecKnob;
    juce::Label    penvAmtLbl, penvAtkLbl, penvDecLbl;
    std::unique_ptr<Attach> penvAmtAtt, penvAtkAtt, penvDecAtt;

    // LFO
    juce::ComboBox lfoWaveCombo, lfoTargetCombo;
    juce::Label    lfoWaveLbl, lfoTargetLbl;
    std::unique_ptr<CAttach> lfoWaveAtt, lfoTargetAtt;
    juce::Slider   lfoRateKnob, lfoDepthKnob;
    juce::Label    lfoRateLbl, lfoDepthLbl;
    std::unique_ptr<Attach> lfoRateAtt, lfoDepthAtt;

    // Voice
    juce::Slider   unisonKnob, detuneKnob, glideKnob;
    juce::Label    unisonLbl, detuneLbl, glideLbl;
    std::unique_ptr<Attach> unisonAtt, detuneAtt, glideAtt;

    // Effects
    juce::Slider   revSizeKnob, revDampKnob, revWetKnob;
    juce::Label    revSizeLbl, revDampLbl, revWetLbl;
    std::unique_ptr<Attach> revSizeAtt, revDampAtt, revWetAtt;

    juce::Slider   delTimeKnob, delFbKnob, delWetKnob;
    juce::Label    delTimeLbl, delFbLbl, delWetLbl;
    std::unique_ptr<Attach> delTimeAtt, delFbAtt, delWetAtt;

    juce::Slider   chrRateKnob, chrDepKnob, chrWetKnob;
    juce::Label    chrRateLbl, chrDepLbl, chrWetLbl;
    std::unique_ptr<Attach> chrRateAtt, chrDepAtt, chrWetAtt;

    // Master
    juce::Slider   masterVolKnob;
    juce::Label    masterVolLbl;
    std::unique_ptr<Attach> masterVolAtt;

    // Preset selector
    juce::ComboBox presetCombo;
    juce::Label    presetLabel;

    // Section group boxes
    juce::GroupComponent oscAGroup, oscBGroup, envGroup, filtGroup, fenvGroup,
                          penvGroup, lfoGroup, voiceGroup, fxGroup, masterGroup;

    // Helper
    void makeKnob(juce::Slider& s, juce::Label& l, const juce::String& text,
                  const juce::String& paramId,
                  std::unique_ptr<Attach>& att);
    void makeCombo(juce::ComboBox& c, juce::Label& l, const juce::String& text,
                   const juce::StringArray& items, const juce::String& paramId,
                   std::unique_ptr<CAttach>& att);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenOmniEditor)
};
