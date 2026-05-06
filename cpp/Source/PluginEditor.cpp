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
                          juce::Slider&) override
    {
        float radius = std::min(w, h) * 0.4f;
        float cx = x + w * 0.5f, cy = y + h * 0.5f;

        juce::Path track;
        track.addCentredArc(cx, cy, radius, radius, 0.0f, startAngle, endAngle, true);
        g.setColour(juce::Colour{0xff334466});
        g.strokePath(track, juce::PathStrokeType(2.5f));

        float angle = startAngle + sliderPos * (endAngle - startAngle);
        juce::Path fill;
        fill.addCentredArc(cx, cy, radius, radius, 0.0f, startAngle, angle, true);
        g.setColour(ACCENT);
        g.strokePath(fill, juce::PathStrokeType(2.5f));

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
    setSize(1100, 530);

    // Preset bar
    presetLabel.setText("Preset", juce::dontSendNotification);
    presetLabel.setColour(juce::Label::textColourId, ACCENT);
    addAndMakeVisible(presetLabel);
    auto names = p.getSynth().getPresetNames();
    for (int i = 0; i < (int)names.size(); ++i)
        presetCombo.addItem(names[i], i + 1);
    presetCombo.setSelectedItemIndex(p.getCurrentProgram(), juce::dontSendNotification);
    presetCombo.onChange = [this, &p]() {
        p.setCurrentProgram(presetCombo.getSelectedItemIndex());
    };
    addAndMakeVisible(presetCombo);

    // Group boxes
    oscAGroup.setText("OSC A");      addAndMakeVisible(oscAGroup);
    oscBGroup.setText("OSC B");      addAndMakeVisible(oscBGroup);
    envGroup.setText("AMP ENV");     addAndMakeVisible(envGroup);
    filtGroup.setText("FILTER");     addAndMakeVisible(filtGroup);
    fenvGroup.setText("FILTER ENV"); addAndMakeVisible(fenvGroup);
    penvGroup.setText("PITCH ENV");  addAndMakeVisible(penvGroup);
    lfoGroup.setText("LFO");         addAndMakeVisible(lfoGroup);
    voiceGroup.setText("VOICE");     addAndMakeVisible(voiceGroup);
    fxGroup.setText("FX");           addAndMakeVisible(fxGroup);
    masterGroup.setText("MASTER");   addAndMakeVisible(masterGroup);

    const juce::StringArray waves{"sine","soft","triangle","sawtooth","square","bell","gamelan"};
    const juce::StringArray fmodes{"lowpass","highpass","bandpass"};
    const juce::StringArray ltgts{"pitch","filter","volume"};
    const juce::StringArray lwaves{"sine","triangle","sawtooth","square"};

    // OSC A
    makeCombo(oscAWaveCombo,  oscAWaveLabel,  "Wave",  waves, "osc_a_wave",  oscAWaveAttach);
    makeKnob(oscALevelKnob, oscALevelLabel, "Level", "osc_a_level", oscALevelAtt);
    makeKnob(oscAOctKnob,   oscAOctLabel,   "Oct",   "osc_a_oct",   oscAOctAtt);
    makeKnob(oscASemiKnob,  oscASemiLabel,  "Semi",  "osc_a_semi",  oscASemiAtt);
    makeKnob(oscAFineKnob,  oscAFineLabel,  "Fine",  "osc_a_fine",  oscAFineAtt);

    // OSC B
    makeCombo(oscBWaveCombo,  oscBWaveLabel,  "Wave",  waves, "osc_b_wave",  oscBWaveAttach);
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
    makeKnob(penvAmtKnob, penvAmtLbl, "Amt", "penv_amount", penvAmtAtt);
    makeKnob(penvAtkKnob, penvAtkLbl, "Att", "penv_attack", penvAtkAtt);
    makeKnob(penvDecKnob, penvDecLbl, "Dec", "penv_decay",  penvDecAtt);

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
    makeKnob(revSizeKnob, revSizeLbl, "R.Size", "reverb_size",    revSizeAtt);
    makeKnob(revDampKnob, revDampLbl, "R.Damp", "reverb_damp",    revDampAtt);
    makeKnob(revWetKnob,  revWetLbl,  "R.Wet",  "reverb_wet",     revWetAtt);
    makeKnob(delTimeKnob, delTimeLbl, "D.Time", "delay_time",     delTimeAtt);
    makeKnob(delFbKnob,   delFbLbl,   "D.Fbk",  "delay_feedback", delFbAtt);
    makeKnob(delWetKnob,  delWetLbl,  "D.Wet",  "delay_wet",      delWetAtt);
    makeKnob(chrRateKnob, chrRateLbl, "C.Rate", "chorus_rate",    chrRateAtt);
    makeKnob(chrDepKnob,  chrDepLbl,  "C.Dep",  "chorus_depth",   chrDepAtt);
    makeKnob(chrWetKnob,  chrWetLbl,  "C.Wet",  "chorus_wet",     chrWetAtt);

    // Master
    makeKnob(masterVolKnob, masterVolLbl, "Volume", "master_volume", masterVolAtt);
}

OpenOmniEditor::~OpenOmniEditor()
{
    setLookAndFeel(nullptr);
}

void OpenOmniEditor::makeKnob(juce::Slider& s, juce::Label& l, const juce::String& text,
                               const juce::String& paramId, std::unique_ptr<Attach>& att)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 14);
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
    // Layout constants
    const int KW   = 46;   // knob column width
    const int KH   = 82;   // rotary knob height
    const int LH   = 16;   // label height
    const int CH   = 22;   // combo height
    const int CW   = 78;   // combo width
    const int GM   = 8;    // group inner margin
    const int GT   = 22;   // group title bar height
    const int ROWH = KH + LH + GT + GM * 2;   // total row height
    const int GAP  = 6;    // gap between groups

    // Helpers — no structured bindings, plain lambdas
    auto placeK = [&](juce::Slider& s, juce::Label& l, int x, int y)
    {
        s.setBounds(x, y,      KW, KH);
        l.setBounds(x, y + KH, KW, LH);
    };
    auto placeC = [&](juce::ComboBox& c, juce::Label& l, int x, int y)
    {
        c.setBounds(x, y,      CW, CH);
        l.setBounds(x, y + CH, CW, LH);
    };

    // ── Header ───────────────────────────────────────────────────────────────
    presetLabel.setBounds(6, 8, 55, 22);
    presetCombo.setBounds(65, 8, 200, 24);

    // ── Row 1  (OSC A | OSC B | AMP ENV | FILTER) ───────────────────────────
    int y   = 38;
    int cy  = y + GT + GM;   // content y inside group
    int gx  = 4;

    // OSC A
    int oscAW = CW + 4 * KW + GM * 2;
    oscAGroup.setBounds(gx, y, oscAW, ROWH);
    placeC(oscAWaveCombo, oscAWaveLabel, gx + GM, cy);
    placeK(oscALevelKnob, oscALevelLabel, gx + GM + CW,           cy);
    placeK(oscAOctKnob,   oscAOctLabel,   gx + GM + CW + KW,      cy);
    placeK(oscASemiKnob,  oscASemiLabel,  gx + GM + CW + KW * 2,  cy);
    placeK(oscAFineKnob,  oscAFineLabel,  gx + GM + CW + KW * 3,  cy);
    gx += oscAW + GAP;

    // OSC B
    int oscBW = CW + 4 * KW + GM * 2;
    oscBGroup.setBounds(gx, y, oscBW, ROWH);
    oscBEnableBtn.setBounds(gx + GM, cy, CW, 18);
    placeC(oscBWaveCombo, oscBWaveLabel, gx + GM, cy + 20);
    placeK(oscBLevelKnob, oscBLevelLabel, gx + GM + CW,           cy);
    placeK(oscBOctKnob,   oscBOctLabel,   gx + GM + CW + KW,      cy);
    placeK(oscBSemiKnob,  oscBSemiLabel,  gx + GM + CW + KW * 2,  cy);
    placeK(oscBFineKnob,  oscBFineLabel,  gx + GM + CW + KW * 3,  cy);
    gx += oscBW + GAP;

    // AMP ENV
    int envW = 4 * KW + GM * 2;
    envGroup.setBounds(gx, y, envW, ROWH);
    placeK(atkKnob, atkLbl, gx + GM,           cy);
    placeK(decKnob, decLbl, gx + GM + KW,      cy);
    placeK(susKnob, susLbl, gx + GM + KW * 2,  cy);
    placeK(relKnob, relLbl, gx + GM + KW * 3,  cy);
    gx += envW + GAP;

    // FILTER
    int filtW = CW + 4 * KW + GM * 2;
    filtGroup.setBounds(gx, y, filtW, ROWH);
    placeC(filterModeCombo, filterModeLabel, gx + GM, cy);
    placeK(filterCutKnob,  filterCutLbl,  gx + GM + CW,           cy);
    placeK(filterResKnob,  filterResLbl,  gx + GM + CW + KW,      cy);
    placeK(filterEnvKnob,  filterEnvLbl,  gx + GM + CW + KW * 2,  cy);
    placeK(filterKtrkKnob, filterKtrkLbl, gx + GM + CW + KW * 3,  cy);

    // ── Row 2  (FENV | PENV | LFO | VOICE) ──────────────────────────────────
    y  += ROWH + GAP;
    cy  = y + GT + GM;
    gx  = 4;

    // FILTER ENV
    int fenvW = 4 * KW + GM * 2;
    fenvGroup.setBounds(gx, y, fenvW, ROWH);
    placeK(fatkKnob, fatkLbl, gx + GM,           cy);
    placeK(fdecKnob, fdecLbl, gx + GM + KW,      cy);
    placeK(fsusKnob, fsusLbl, gx + GM + KW * 2,  cy);
    placeK(frelKnob, frelLbl, gx + GM + KW * 3,  cy);
    gx += fenvW + GAP;

    // PITCH ENV
    int penvW = 3 * KW + GM * 2;
    penvGroup.setBounds(gx, y, penvW, ROWH);
    placeK(penvAmtKnob, penvAmtLbl, gx + GM,           cy);
    placeK(penvAtkKnob, penvAtkLbl, gx + GM + KW,      cy);
    placeK(penvDecKnob, penvDecLbl, gx + GM + KW * 2,  cy);
    gx += penvW + GAP;

    // LFO
    int lfoW = CW * 2 + 2 * KW + GM * 2;
    lfoGroup.setBounds(gx, y, lfoW, ROWH);
    placeC(lfoWaveCombo,   lfoWaveLbl,   gx + GM,           cy);
    placeC(lfoTargetCombo, lfoTargetLbl, gx + GM + CW,      cy);
    placeK(lfoRateKnob,  lfoRateLbl,  gx + GM + CW * 2,      cy);
    placeK(lfoDepthKnob, lfoDepthLbl, gx + GM + CW * 2 + KW, cy);
    gx += lfoW + GAP;

    // VOICE
    int voiceW = 3 * KW + GM * 2;
    voiceGroup.setBounds(gx, y, voiceW, ROWH);
    placeK(unisonKnob, unisonLbl, gx + GM,           cy);
    placeK(detuneKnob, detuneLbl, gx + GM + KW,      cy);
    placeK(glideKnob,  glideLbl,  gx + GM + KW * 2,  cy);

    // ── Row 3  (FX | MASTER) ─────────────────────────────────────────────────
    y  += ROWH + GAP;
    cy  = y + GT + GM;
    gx  = 4;

    // FX (9 knobs: reverb x3, delay x3, chorus x3)
    int fxW = 9 * KW + GM * 2;
    fxGroup.setBounds(gx, y, fxW, ROWH);
    placeK(revSizeKnob, revSizeLbl, gx + GM + KW * 0,  cy);
    placeK(revDampKnob, revDampLbl, gx + GM + KW * 1,  cy);
    placeK(revWetKnob,  revWetLbl,  gx + GM + KW * 2,  cy);
    placeK(delTimeKnob, delTimeLbl, gx + GM + KW * 3,  cy);
    placeK(delFbKnob,   delFbLbl,   gx + GM + KW * 4,  cy);
    placeK(delWetKnob,  delWetLbl,  gx + GM + KW * 5,  cy);
    placeK(chrRateKnob, chrRateLbl, gx + GM + KW * 6,  cy);
    placeK(chrDepKnob,  chrDepLbl,  gx + GM + KW * 7,  cy);
    placeK(chrWetKnob,  chrWetLbl,  gx + GM + KW * 8,  cy);
    gx += fxW + GAP;

    // MASTER
    int masterW = KW + GM * 2;
    masterGroup.setBounds(gx, y, masterW, ROWH);
    placeK(masterVolKnob, masterVolLbl, gx + GM, cy);
}

// ── Paint ────────────────────────────────────────────────────────────────────
void OpenOmniEditor::paint(juce::Graphics& g)
{
    g.fillAll(BG_DARK);

    juce::ColourGradient grad{BG_MED, 0, 0, BG_DARK, 0, 36.0f, false};
    g.setGradientFill(grad);
    g.fillRect(0, 0, getWidth(), 36);

    g.setColour(ACCENT);
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(20.0f).withStyle("Bold")));
    g.drawText("OPEN OMNI", 10, 4, 160, 28, juce::Justification::centredLeft);

    g.setColour(ACCENT.withAlpha(0.3f));
    g.drawHorizontalLine(36, 0.0f, (float)getWidth());
}

void OpenOmniEditor::timerCallback() {}
