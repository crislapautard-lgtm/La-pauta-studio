#include "BeatMaker.h"

BeatMaker::BeatMaker()
{
    addAndMakeVisible(title);
    title.setText("BEAT MAKER - 16 PASOS - LA PAUTA", juce::dontSendNotification);
    title.setFont(juce::Font(18.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colours::black);

    addAndMakeVisible(dembowBtn); addAndMakeVisible(trapBtn);
    addAndMakeVisible(drillBtn); addAndMakeVisible(clearBtn); addAndMakeVisible(playBtn);
    dembowBtn.addListener(this); trapBtn.addListener(this);
    drillBtn.addListener(this); clearBtn.addListener(this); playBtn.addListener(this);

    for (int r=0;r<4;r++) for(int s=0;s<16;s++) { addAndMakeVisible(steps[r][s]); steps[r][s].addListener(this); }
    loadDembow();
    startTimer(50);
}

void BeatMaker::loadDembow()
{
    for(int r=0;r<4;r++) for(int s=0;s<16;s++) pattern[r][s]=false;
    // KICK Dembow
    pattern[0][0]=true; pattern[0][6]=true; pattern[0][8]=true; pattern[0][14]=true;
    // SNARE
    pattern[1][4]=true; pattern[1][12]=true;
    // HI-HAT
    for(int s=0;s<16;s+=2) pattern[2][s]=true;
    // 808
    pattern[3][0]=true; pattern[3][7]=true; pattern[3][10]=true;
    for(int r=0;r<4;r++) for(int s=0;s<16;s++) steps[r][s].setToggleState(pattern[r][s], juce::dontSendNotification);
    bpmValue=95;
}

void BeatMaker::loadTrap()
{
    for(int r=0;r<4;r++) for(int s=0;s<16;s++) pattern[r][s]=false;
    pattern[0][0]=true; pattern[0][10]=true;
    pattern[1][8]=true;
    for(int s=0;s<16;s++) pattern[2][s]=true; pattern[2][3]=false; pattern[2][11]=false;
    pattern[3][0]=true; pattern[3][3]=true; pattern[3][6]=true; pattern[3][10]=true;
    for(int r=0;r<4;r++) for(int s=0;s<16;s++) steps[r][s].setToggleState(pattern[r][s], juce::dontSendNotification);
    bpmValue=140;
}

void BeatMaker::loadDrill()
{
    for(int r=0;r<4;r++) for(int s=0;s<16;s++) pattern[r][s]=false;
    pattern[0][0]=true; pattern[0][8]=true; pattern[0][11]=true;
    pattern[1][4]=true; pattern[1][12]=true;
    for(int s=0;s<16;s+=2) pattern[2][s]=true;
    pattern[3][0]=true; pattern[3][8]=true; pattern[3][14]=true;
    for(int r=0;r<4;r++) for(int s=0;s<16;s++) steps[r][s].setToggleState(pattern[r][s], juce::dontSendNotification);
    bpmValue=142;
}

void BeatMaker::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::white);
    g.setColour(juce::Colour(0xff00d4d4)); g.fillRect(0,0,getWidth(),40);
    g.setColour(juce::Colours::black);
    for(int r=0;r<4;r++) g.drawText(rowNames[r], 10, 70+r*45, 70, 25, juce::Justification::left);
    if(playing) { g.setColour(juce::Colours::red); g.fillRect(90+currentStep*35, 60, 30, 190); }
}

void BeatMaker::resized()
{
    title.setBounds(10,5,400,30);
    dembowBtn.setBounds(450,5,110,30); trapBtn.setBounds(570,5,110,30);
    drillBtn.setBounds(690,5,110,30); clearBtn.setBounds(810,5,90,30); playBtn.setBounds(910,5,100,30);
    for(int r=0;r<4;r++) for(int s=0;s<16;s++) steps[r][s].setBounds(90+s*35, 70+r*45, 30, 30);
}

void BeatMaker::buttonClicked(juce::Button* b)
{
    if(b==&dembowBtn) loadDembow();
    if(b==&trapBtn) loadTrap();
    if(b==&drillBtn) loadDrill();
    if(b==&clearBtn) { for(int r=0;r<4;r++) for(int s=0;s<16;s++) pattern[r][s]=false; for(int r=0;r<4;r++) for(int s=0;s<16;s++) steps[r][s].setToggleState(false, juce::dontSendNotification); }
    if(b==&playBtn) { playing=!playing; playBtn.setButtonText(playing?"STOP BEAT":"PLAY BEAT"); if(!playing) currentStep=0; }
    for(int r=0;r<4;r++) for(int s=0;s<16;s++) if(b==&steps[r][s]) pattern[r][s]=steps[r][s].getToggleState();
}

void BeatMaker::timerCallback()
{
    if(playing) { currentStep=(currentStep+1)%16; repaint(); }
}
