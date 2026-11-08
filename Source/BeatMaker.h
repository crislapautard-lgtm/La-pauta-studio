#pragma once
#include <JuceHeader.h>

class BeatMaker : public juce::Component, public juce::Button::Listener, public juce::Timer
{
public:
    BeatMaker();
    void paint(juce::Graphics& g) override;
    void resized() override;
    void buttonClicked(juce::Button* b) override;
    void timerCallback() override;
    void setBPM(int bpm) { bpmValue = bpm; }
    int getCurrentStep() { return currentStep; }
    bool isStepOn(int row, int step) { return pattern[row][step]; }
    void clearPattern() { for (int r=0;r<4;r++) for(int s=0;s<16;s++) pattern[r][s]=false; loadDembow(); }
    void loadDembow();
    void loadTrap();
    void loadDrill();

private:
    juce::TextButton dembowBtn{"DEMBOW 95"};
    juce::TextButton trapBtn{"TRAP 140"};
    juce::TextButton drillBtn{"DRILL 142"};
    juce::TextButton clearBtn{"LIMPIAR"};
    juce::TextButton playBtn{"PLAY BEAT"};
    juce::Label title;
    juce::ToggleButton steps[4][16];
    juce::String rowNames[4] = {"KICK", "SNARE", "HI-HAT", "808"};
    bool pattern[4][16] = {};
    int currentStep = 0;
    int bpmValue = 95;
    bool playing = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BeatMaker)
};
