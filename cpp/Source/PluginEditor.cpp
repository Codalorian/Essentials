#include "PluginEditor.h"

static const juce::Colour BG_DARK  {0xff1a1a2e};
static const juce::Colour BG_MED   {0xff16213e};
static const juce::Colour ACCENT   {0xff00d4aa};
static const juce::Colour TEXT_COL {0xffe0e0e0};

// ── LookAndFeel ──────────────────────────────────────────────────────────────
struct OmniLookAndFeel : public juce::LookAndFeel_V4
{
    OmniLookAndFeel()
    {
        setColour(juce::Slider::thumbColourId,              ACCENT);
        setColour(juce::Slider::rotarySliderFillColourId,   ACCENT);
        setColour(juce::Slider::rotarySliderOutlineColourId,juce::Colour{0xff334466});
        setColour(juce::Slider::textBoxTextColourId,        TEXT_COL);
        setColour(juce::Slider::textBoxOutlineColourId,     juce::Colours::transparentBlack);
        setColour(juce::Label::textColourId,                TEXT_COL);
        setColour(juce::GroupComponent::outlineColourId,    juce::Colour{0xff334466});
        setColour(juce::GroupComponent::textColourId,       ACCENT);
        setColour(juce::ComboBox::backgroundColourId,       juce::Colour{0xff0f3460});
        setColour(juce::ComboBox::textColourId,             TEXT_COL);
        setColour(juce::ComboBox::outlineColourId,          juce::Colour{0xff334466});
        setColour(juce::PopupMenu::backgroundColourId,      juce::Colour{0xff0f3460});
        setColour(juce::PopupMenu::textColourId,            TEXT_COL);
        setColour(juce::ToggleButton::textColourId,         TEXT_COL);
        setColour(juce::ToggleButton::tickColourId,         ACCENT);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                          float sliderPos, float startAngle, float endAngle,
                          juce::Slider& s) override
    {
        float radius = std::min(w, h) * 0.4f;
        float cx = x + w * 0.5f, cy = y + h * 0.5f;

        // Track
        juce::Path track;
        track.addCentredArc(cx, cy, radius, radius, 0.0f, startAngle, endAngle, true);
        g.setColour(juce::Colour{0xff334466});
        g.strokePath(track, juce::PathStrokeType(2.5f));

        // Fill
        float angle = startAngle + sliderPos * (endAngle - startAngle);
        juce::Path fill;
        fill.addCentredArc(cx, cy, radius, radius, 0.0f, startAngle, angle, true);
        g.setColour(ACCENT);
        g.strokePath(fill, juce::PathStrokeType(2.5f));

        // Thumb dot
        float tx = cx + radius * std::sin(angle);
        float ty = cy - radius * std::cos(angle);
        g.setColour(juce::Colours::white);
        g.fillEllipse(tx - 3.5f, ty - 3.5f, 7.0f, 7.0f);
    }
} omniLF;


// ── Constructor ──────────────────────────────────────────────────────────────
OpenOmniEditor::OpenOmniEditor(OpenOmniProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&omniLF);
    setSize(1020, 640);

    // Preset combo
    presetLabel.setText("Preset", juce::dontSendNotification);
    presetLabel.setColour(juce::Label::textColourId, ACCENT);
    addAndMakeVisible(presetLabel);
    auto names = p.getSynth().getPresetNames();
    for (int i = 0; i < (int)names.size(); ++i)
        presetCombo.addItem(names[i], i + 1);
    presetCombo.setSelectedItemIndex(p.getCurrentProgram(), juce::dontSendNotification);
    presetCombo.onChange = [this, &p]()
    {
        p.setCurrentProgram(presetCombo.getSelectedItemIndex());
    };
    addAndMakeVisible(presetCombo);

    // Section labels
    for (auto* g : {&oscAGroup,&oscBGroup,&envGroup,&filtGroup,
                    &fenvGroup,&penvGroup,&lfoGroup,&voiceGroup,&fxGroup,&masterGroup})
        addAndMakeVisible(g);

    oscAGroup.setText("OSC A");
    oscBGroup.setText("OSC B");
    envGroup.setText("AMP ENV");
    filtGroup.setText("FILTER");
    fenvGroup.setText("FILTER ENV");
    penvGroup.setText("PITCH ENV");
    lfoGroup.setText("LFO");
    voiceGroup.setText("VOICE");
    fxGroup.setText("FX");
    masterGroup.setText("MASTER");

    const juce::StringArray waves{"sine","soft","triangle","sawtooth","square","bell","gamelan"};
    const juce::StringArray fmodes{"lowpass","highpass","bandpass"};
    const juce::StringArray ltgts{"pitch","filter","volume"};
    const juce::StringArray lwaves{"sine","triangle","sawtooth","square"};

    // OSC A
    makeCombo(oscAWaveCombo, oscAWaveLabel, "Wave",  waves, "osc_a_wave",  oscAWaveAttach);
    makeKnob(oscALevelKnob, oscALevelLabel, "Level", "osc_a_level", oscALevelAtt);
    makeKnob(oscAOctKnob,   oscAOctLabel,   "Oct",   "osc_a_oct",   oscAOctAtt);
    makeKnob(oscASemiKnob,  oscASemiLabel,  "Semi",  "osc_a_semi",  oscASemiAtt);
    makeKnob(oscAFineKnob,  oscAFineLabel,  "Fine",  "osc_a_fine",  oscAFineAtt);

    // OSC B
    makeCombo(oscBWaveCombo, oscBWaveLabel, "Wave",  waves, "osc_b_wave",  oscBWaveAttach);
    makeKnob(oscBLevelKnob, oscBLevelLabel, "Level", "osc_b_level", oscBLevelAtt);
    makeKnob(oscBOctKnob,   oscBOctLabel,   "Oct",   "osc_b_oct",   oscBOctAtt);
    makeKnob(oscBSemiKnob,  oscBSemiLabel,  "Semi",  "osc_b_semi",  oscBSemiAtt);
    makeKnob(oscBFineKnob,  oscBFineLabel,  "Fine",  "osc_b_fine",  oscBFineAtt);
    oscBEnableBtn.setToggleState(true, juce::dontSendNotification);
    oscBEnableAtt = std::make_unique<BAttach>(processor.apvts, "osc_b_en", oscBEnableBtn);
    addAndMakeVisible(oscBEnableBtn);

    // Amp env
    makeKnob(atkKnob, atkLbl, "ATK", "attack",  atkAtt);
    makeKnob(decKnob, decLbl, "DEC", "decay",   decAtt);
    makeKnob(susKnob, susLbl, "SUS", "sustain", susAtt);
    makeKnob(relKnob, relLbl, "REL", "release", relAtt);

    // Filter
    makeCombo(filterModeCombo, filterModeLabel, "Mode", fmodes, "filter_mode", filterModeAtt);
    makeKnob(filterCutKnob,  filterCutLbl,  "Cutoff", "filter_cutoff",    filterCutAtt);
    makeKnob(filterResKnob,  filterResLbl,  "Res",    "filter_resonance", filterResAtt);
    makeKnob(filterEnvKnob,  filterEnvLbl,  "Env",    "filter_env_amt",   filterEnvAtt);
    makeKnob(filterKtrkKnob, filterKtrkLbl, "KTrk",   "filter_keytrack",  filterKtrkAtt);

    // Filter envelope
    makeKnob(fatkKnob, fatkLbl, "ATK", "fenv_attack",  fatkAtt);
    makeKnob(fdecKnob, fdecLbl, "DEC", "fenv_decay",   fdecAtt);
    makeKnob(fsusKnob, fsusLbl, "SUS", "fenv_sustain", fsusAtt);
    makeKnob(frelKnob, frelLbl, "REL", "fenv_release", frelAtt);

    // Pitch envelope
    makeKnob(penvAmtKnob, penvAmtLbl, "Amt",  "penv_amount", penvAmtAtt);
    makeKnob(penvAtkKnob, penvAtkLbl, "Att",  "penv_attack", penvAtkAtt);
    makeKnob(penvDecKnob, penvDecLbl, "Dec",  "penv_decay",  penvDecAtt);

    // LFO
    makeCombo(lfoWaveCombo,   lfoWaveLbl,   "Wave",   lwaves, "lfo_wave",   lfoWaveAtt);
    makeCombo(lfoTargetCombo, lfoTargetLbl, "Target", ltgts,  "lfo_target", lfoTargetAtt);
    makeKnob(lfoRateKnob,  lfoRateLbl,  "Rate",  "lfo_rate",  lfoRateAtt);
    makeKnob(lfoDepthKnob, lfoDepthLbl, "Depth", "lfo_depth", lfoDepthAtt);

    // Voice
    makeKnob(unisonKnob, unisonLbl, "Voices", "unison_voices", unisonAtt);
    makeKnob(detuneKnob, detuneLbl, "Detune", "unison_detune", detuneAtt);
    makeKnob(glideKnob,  glideLbl,  "Glide",  "glide_time",   glideAtt);

    // FX
    makeKnob(revSizeKnob, revSizeLbl, "R.Size", "reverb_size",     revSizeAtt);
    makeKnob(revDampKnob, revDampLbl, "R.Damp", "reverb_damp",     revDampAtt);
    makeKnob(revWetKnob,  revWetLbl,  "R.Wet",  "reverb_wet",      revWetAtt);
    makeKnob(delTimeKnob, delTimeLbl, "D.Time", "delay_time",      delTimeAtt);
    makeKnob(delFbKnob,   delFbLbl,   "D.Fbk",  "delay_feedback",  delFbAtt);
    makeKnob(delWetKnob,  delWetLbl,  "D.Wet",  "delay_wet",       delWetAtt);
    makeKnob(chrRateKnob, chrRateLbl, "C.Rate", "chorus_rate",     chrRateAtt);
    makeKnob(chrDepKnob,  chrDepLbl,  "C.Dep",  "chorus_depth",    chrDepAtt);
    makeKnob(chrWetKnob,  chrWetLbl,  "C.Wet",  "chorus_wet",      chrWetAtt);

    // Master
    makeKnob(masterVolKnob, masterVolLbl, "Volume", "master_volume", masterVolAtt);

    startTimerHz(30);
}

OpenOmniEditor::~OpenOmniEditor()
{
    setLookAndFeel(nullptr);
}

void OpenOmniEditor::makeKnob(juce::Slider& s, juce::Label& l, const juce::String& text,
                               const juce::String& paramId, std::unique_ptr<Attach>& att)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 14);
    addAndMakeVisible(s);
    l.setText(text, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(l);
    att = std::make_unique<Attach>(processor.apvts, paramId, s);
}

void OpenOmniEditor::makeCombo(juce::ComboBox& c, juce::Label& l, const juce::String& text,
                                const juce::StringArray& items, const juce::String& paramId,
                                std::unique_ptr<CAttach>& att)
{
    c.addItemList(items, 1);
    addAndMakeVisible(c);
    l.setText(text, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(l);
    att = std::make_unique<CAttach>(processor.apvts, paramId, c);
}

// ── Layout ───────────────────────────────────────────────────────────────────
void OpenOmniEditor::resized()
{
    auto area = getLocalBounds().reduced(6);
    int KW = 68, KH = 80, COMBO_H = 22, LABEL_H = 16, ROW_H = 100;
    int GW = 5*KW + 10;  // group width for 5-knob rows

    // Preset bar (top)
    auto topBar = area.removeFromTop(30);
    presetLabel.setBounds(topBar.removeFromLeft(60));
    presetCombo.setBounds(topBar.removeFromLeft(200));

    area.removeFromTop(4);

    // Row 1: OSC A | OSC B | AMP ENV | FILTER
    auto row1 = area.removeFromTop(ROW_H + 10);

    // OSC A — 5 controls: wave combo + 4 knobs
    auto oscAR = row1.removeFromLeft(GW + 12);
    oscAGroup.setBounds(oscAR);
    auto oscAI = oscAR.reduced(6, 14);
    oscAWaveLabel.setBounds(oscAI.removeFromLeft(8));
    oscAI.removeFromLeft(2);
    auto wa = oscAI.removeFromLeft(70).removeFromTop(COMBO_H + LABEL_H);
    oscAWaveLabel.setBounds(wa.removeFromBottom(LABEL_H));
    oscAWaveCombo.setBounds(wa);
    for (auto* [k, l] : {std::pair<juce::Slider*, juce::Label*>{&oscALevelKnob,&oscALevelLabel},
                                                                {&oscAOctKnob,&oscAOctLabel},
                                                                {&oscASemiKnob,&oscASemiLabel},
                                                                {&oscAFineKnob,&oscAFineLabel}})
    {
        auto col = oscAI.removeFromLeft(KW);
        l->setBounds(col.removeFromBottom(LABEL_H));
        k->setBounds(col.removeFromTop(KH - LABEL_H - 14));
    }
    row1.removeFromLeft(4);

    // OSC B — same layout
    auto oscBR = row1.removeFromLeft(GW + 30);
    oscBGroup.setBounds(oscBR);
    auto oscBI = oscBR.reduced(6, 14);
    auto wb = oscBI.removeFromLeft(70).removeFromTop(COMBO_H + LABEL_H + 4);
    oscBEnableBtn.setBounds(wb.removeFromTop(20));
    wb.removeFromTop(2);
    oscBWaveLabel.setBounds(wb.removeFromBottom(LABEL_H));
    oscBWaveCombo.setBounds(wb);
    for (auto* [k, l] : {std::pair<juce::Slider*, juce::Label*>{&oscBLevelKnob,&oscBLevelLabel},
                                                                {&oscBOctKnob,&oscBOctLabel},
                                                                {&oscBSemiKnob,&oscBSemiLabel},
                                                                {&oscBFineKnob,&oscBFineLabel}})
    {
        auto col = oscBI.removeFromLeft(KW);
        l->setBounds(col.removeFromBottom(LABEL_H));
        k->setBounds(col.removeFromTop(KH - LABEL_H - 14));
    }
    row1.removeFromLeft(4);

    // Amp Env
    auto envR = row1.removeFromLeft(4*KW + 12);
    envGroup.setBounds(envR);
    auto envI = envR.reduced(6, 14);
    for (auto* [k, l] : {std::pair<juce::Slider*, juce::Label*>{&atkKnob,&atkLbl},
                                                               {&decKnob,&decLbl},
                                                               {&susKnob,&susLbl},
                                                               {&relKnob,&relLbl}})
    {
        auto col = envI.removeFromLeft(KW);
        l->setBounds(col.removeFromBottom(LABEL_H));
        k->setBounds(col.removeFromTop(KH - LABEL_H - 14));
    }

    row1.removeFromLeft(4);

    // Filter
    auto filtR = row1;
    filtGroup.setBounds(filtR);
    auto filtI = filtR.reduced(6, 14);
    auto fmW = filtI.removeFromLeft(70).removeFromTop(COMBO_H + LABEL_H);
    filterModeLabel.setBounds(fmW.removeFromBottom(LABEL_H));
    filterModeCombo.setBounds(fmW);
    for (auto* [k, l] : {std::pair<juce::Slider*, juce::Label*>{&filterCutKnob,&filterCutLbl},
                                                               {&filterResKnob,&filterResLbl},
                                                               {&filterEnvKnob,&filterEnvLbl},
                                                               {&filterKtrkKnob,&filterKtrkLbl}})
    {
        auto col = filtI.removeFromLeft(KW);
        l->setBounds(col.removeFromBottom(LABEL_H));
        k->setBounds(col.removeFromTop(KH - LABEL_H - 14));
    }

    area.removeFromTop(6);

    // Row 2: F.ENV | P.ENV | LFO | VOICE
    auto row2 = area.removeFromTop(ROW_H + 10);

    // Filter Env
    auto fenvR = row2.removeFromLeft(4*KW + 12);
    fenvGroup.setBounds(fenvR);
    auto fenvI = fenvR.reduced(6, 14);
    for (auto* [k, l] : {std::pair<juce::Slider*, juce::Label*>{&fatkKnob,&fatkLbl},
                                                               {&fdecKnob,&fdecLbl},
                                                               {&fsusKnob,&fsusLbl},
                                                               {&frelKnob,&frelLbl}})
    {
        auto col = fenvI.removeFromLeft(KW);
        l->setBounds(col.removeFromBottom(LABEL_H));
        k->setBounds(col.removeFromTop(KH - LABEL_H - 14));
    }
    row2.removeFromLeft(4);

    // Pitch Env
    auto penvR = row2.removeFromLeft(3*KW + 12);
    penvGroup.setBounds(penvR);
    auto penvI = penvR.reduced(6, 14);
    for (auto* [k, l] : {std::pair<juce::Slider*, juce::Label*>{&penvAmtKnob,&penvAmtLbl},
                                                               {&penvAtkKnob,&penvAtkLbl},
                                                               {&penvDecKnob,&penvDecLbl}})
    {
        auto col = penvI.removeFromLeft(KW);
        l->setBounds(col.removeFromBottom(LABEL_H));
        k->setBounds(col.removeFromTop(KH - LABEL_H - 14));
    }
    row2.removeFromLeft(4);

    // LFO
    auto lfoR = row2.removeFromLeft(4*KW + 80);
    lfoGroup.setBounds(lfoR);
    auto lfoI = lfoR.reduced(6, 14);
    auto lwW = lfoI.removeFromLeft(70).removeFromTop(COMBO_H + LABEL_H);
    lfoWaveLbl.setBounds(lwW.removeFromBottom(LABEL_H));
    lfoWaveCombo.setBounds(lwW);
    auto ltW = lfoI.removeFromLeft(70).removeFromTop(COMBO_H + LABEL_H);
    lfoTargetLbl.setBounds(ltW.removeFromBottom(LABEL_H));
    lfoTargetCombo.setBounds(ltW);
    for (auto* [k, l] : {std::pair<juce::Slider*, juce::Label*>{&lfoRateKnob,&lfoRateLbl},
                                                               {&lfoDepthKnob,&lfoDepthLbl}})
    {
        auto col = lfoI.removeFromLeft(KW);
        l->setBounds(col.removeFromBottom(LABEL_H));
        k->setBounds(col.removeFromTop(KH - LABEL_H - 14));
    }
    row2.removeFromLeft(4);

    // Voice
    auto voiceR = row2;
    voiceGroup.setBounds(voiceR);
    auto voiceI = voiceR.reduced(6, 14);
    for (auto* [k, l] : {std::pair<juce::Slider*, juce::Label*>{&unisonKnob,&unisonLbl},
                                                               {&detuneKnob,&detuneLbl},
                                                               {&glideKnob,&glideLbl}})
    {
        auto col = voiceI.removeFromLeft(KW);
        l->setBounds(col.removeFromBottom(LABEL_H));
        k->setBounds(col.removeFromTop(KH - LABEL_H - 14));
    }

    area.removeFromTop(6);

    // Row 3: FX (9 knobs) | Master
    auto row3 = area;
    auto fxR = row3.removeFromLeft(9*KW + 16);
    fxGroup.setBounds(fxR);
    auto fxI = fxR.reduced(6, 14);
    for (auto* [k, l] : {std::pair<juce::Slider*, juce::Label*>
        {&revSizeKnob,&revSizeLbl},{&revDampKnob,&revDampLbl},{&revWetKnob,&revWetLbl},
        {&delTimeKnob,&delTimeLbl},{&delFbKnob,&delFbLbl},{&delWetKnob,&delWetLbl},
        {&chrRateKnob,&chrRateLbl},{&chrDepKnob,&chrDepLbl},{&chrWetKnob,&chrWetLbl}})
    {
        auto col = fxI.removeFromLeft(KW);
        l->setBounds(col.removeFromBottom(LABEL_H));
        k->setBounds(col.removeFromTop(KH - LABEL_H - 14));
    }

    row3.removeFromLeft(4);

    auto masterR = row3;
    masterGroup.setBounds(masterR);
    auto masterI = masterR.reduced(6, 14);
    auto col = masterI.removeFromLeft(KW);
    masterVolLbl.setBounds(col.removeFromBottom(LABEL_H));
    masterVolKnob.setBounds(col.removeFromTop(KH - LABEL_H - 14));
}

// ── Paint ────────────────────────────────────────────────────────────────────
void OpenOmniEditor::paint(juce::Graphics& g)
{
    g.fillAll(BG_DARK);

    // Header gradient band
    juce::ColourGradient grad{BG_MED, 0, 0, BG_DARK, 0, 36.0f, false};
    g.setGradientFill(grad);
    g.fillRect(0, 0, getWidth(), 36);

    g.setColour(ACCENT);
    g.setFont(juce::Font("Arial", 20.0f, juce::Font::bold));
    g.drawText("OPEN OMNI", 10, 4, 160, 28, juce::Justification::centredLeft);

    g.setColour(ACCENT.withAlpha(0.3f));
    g.drawHorizontalLine(36, 0.0f, (float)getWidth());
}

void OpenOmniEditor::timerCallback()
{
    // Nothing needed — APVTS handles repaints automatically
}
