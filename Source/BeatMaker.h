
#pragma once

#include <JuceHeader.h>

// ============================================================
// LA PAUTA STUDIO
// BEAT MAKER - 16 PASOS
// ============================================================

class BeatMaker : public juce::Component,
                  public juce::Button::Listener,
                  public juce::Timer
{
public:

    BeatMaker();
    ~BeatMaker() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void buttonClicked(juce::Button* button) override;
    void timerCallback() override;

    // BPM
    void setBPM(int bpm)
    {
        bpmValue = juce::jlimit(40, 250, bpm);
    }

    int getBPM() const
    {
        return bpmValue;
    }

    // Transporte
    int getCurrentStep() const
    {
        return currentStep;
    }

    bool isPlaying() const
    {
        return playing;
    }

    // Patrón
    bool isStepOn(int row, int step) const
    {
        if (row < 0 || row >= 4 ||
            step < 0 || step >= 16)
            return false;

        return pattern[row][step];
    }

    void clearPattern()
    {
        for (int row = 0; row < 4; ++row)
        {
            for (int step = 0; step < 16; ++step)
            {
                pattern[row][step] = false;
                steps[row][step].setToggleState(
                    false,
                    juce::dontSendNotification);
            }
        }

        currentStep = 0;
        repaint();
    }

    void loadDembow();
    void loadTrap();
    void loadDrill();

private:

    // Botones de patrones
    juce::TextButton dembowBtn { "DEMBOW 95" };
    juce::TextButton trapBtn { "TRAP 140" };
    juce::TextButton drillBtn { "DRILL 142" };

    // Transporte
    juce::TextButton clearBtn { "LIMPIAR" };
    juce::TextButton playBtn { "PLAY BEAT" };

    juce::Label title;

    // Secuenciador de 4 instrumentos x 16 pasos
    juce::ToggleButton steps[4][16];

    juce::String rowNames[4] =
    {
        "KICK",
        "SNARE",
        "HI-HAT",
        "808"
    };

    bool pattern[4][16] = {};

    int currentStep = 0;
    int bpmValue = 95;

    bool playing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BeatMaker)
};
