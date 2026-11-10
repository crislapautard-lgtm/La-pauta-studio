
#include "BeatMaker.h"

// ============================================================
// LA PAUTA STUDIO
// BEAT MAKER - 16 PASOS
// ============================================================

BeatMaker::BeatMaker()
{
    setSize(1020, 280);

    addAndMakeVisible(title);
    title.setText(
        "BEAT MAKER - 16 PASOS - LA PAUTA",
        juce::dontSendNotification);

    title.setFont(juce::Font(18.0f, juce::Font::bold));
    title.setColour(
        juce::Label::textColourId,
        juce::Colour(0xff36d6c5));

    addAndMakeVisible(dembowBtn);
    addAndMakeVisible(trapBtn);
    addAndMakeVisible(drillBtn);
    addAndMakeVisible(clearBtn);
    addAndMakeVisible(playBtn);

    dembowBtn.addListener(this);
    trapBtn.addListener(this);
    drillBtn.addListener(this);
    clearBtn.addListener(this);
    playBtn.addListener(this);

    for (int r = 0; r < 4; ++r)
    {
        for (int s = 0; s < 16; ++s)
        {
            addAndMakeVisible(steps[r][s]);
            steps[r][s].addListener(this);
        }
    }

    loadDembow();
    startTimer(50);
}

// ============================================================
// PATRON DEMBOW
// ============================================================

void BeatMaker::loadDembow()
{
    clearPattern();

    pattern[0][0] = true;
    pattern[0][6] = true;
    pattern[0][8] = true;
    pattern[0][14] = true;

    pattern[1][4] = true;
    pattern[1][12] = true;

    for (int s = 0; s < 16; s += 2)
        pattern[2][s] = true;

    pattern[3][0] = true;
    pattern[3][7] = true;
    pattern[3][10] = true;

    bpmValue = 95;

    for (int r = 0; r < 4; ++r)
        for (int s = 0; s < 16; ++s)
            steps[r][s].setToggleState(
                pattern[r][s],
                juce::dontSendNotification);

    repaint();
}

// ============================================================
// PATRON TRAP
// ============================================================

void BeatMaker::loadTrap()
{
    clearPattern();

    pattern[0][0] = true;
    pattern[0][10] = true;

    pattern[1][8] = true;

    for (int s = 0; s < 16; ++s)
        pattern[2][s] = true;

    pattern[2][3] = false;
    pattern[2][11] = false;

    pattern[3][0] = true;
    pattern[3][3] = true;
    pattern[3][6] = true;
    pattern[3][10] = true;

    bpmValue = 140;

    for (int r = 0; r < 4; ++r)
        for (int s = 0; s < 16; ++s)
            steps[r][s].setToggleState(
                pattern[r][s],
                juce::dontSendNotification);

    repaint();
}

// ============================================================
// PATRON DRILL
// ============================================================

void BeatMaker::loadDrill()
{
    clearPattern();

    pattern[0][0] = true;
    pattern[0][8] = true;
    pattern[0][11] = true;

    pattern[1][4] = true;
    pattern[1][12] = true;

    for (int s = 0; s < 16; s += 2)
        pattern[2][s] = true;

    pattern[3][0] = true;
    pattern[3][8] = true;
    pattern[3][14] = true;

    bpmValue = 142;

    for (int r = 0; r < 4; ++r)
        for (int s = 0; s < 16; ++s)
            steps[r][s].setToggleState(
                pattern[r][s],
                juce::dontSendNotification);

    repaint();
}

// ============================================================
// INTERFAZ
// ============================================================

void BeatMaker::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff111820));

    g.setColour(juce::Colour(0xff1d303a));
    g.fillRect(0, 0, getWidth(), 45);

    g.setColour(juce::Colour(0xff36d6c5));
    g.drawRect(getLocalBounds(), 1);

    for (int r = 0; r < 4; ++r)
    {
        g.setColour(juce::Colour(0xffdce6ed));

        g.drawText(
            rowNames[r],
            10,
            70 + r * 45,
            70,
            30,
            juce::Justification::centredLeft);
    }

    if (playing)
    {
        g.setColour(juce::Colour(0x5536d6c5));

        g.fillRect(
            90 + currentStep * 35,
            60,
            30,
            190);
    }
}

// ============================================================
// POSICIONAMIENTO
// ============================================================

void BeatMaker::resized()
{
    title.setBounds(10, 5, 400, 30);

    dembowBtn.setBounds(450, 5, 110, 30);
    trapBtn.setBounds(570, 5, 110, 30);
    drillBtn.setBounds(690, 5, 110, 30);
    clearBtn.setBounds(810, 5, 90, 30);
    playBtn.setBounds(910, 5, 100, 30);

    for (int r = 0; r < 4; ++r)
    {
        for (int s = 0; s < 16; ++s)
        {
            steps[r][s].setBounds(
                90 + s * 35,
                70 + r * 45,
                30,
                30);
        }
    }
}

// ============================================================
// BOTONES
// ============================================================

void BeatMaker::buttonClicked(juce::Button* button)
{
    if (button == &dembowBtn)
        loadDembow();

    else if (button == &trapBtn)
        loadTrap();

    else if (button == &drillBtn)
        loadDrill();

    else if (button == &clearBtn)
        clearPattern();

    else if (button == &playBtn)
    {
        playing = !playing;

        playBtn.setButtonText(
            playing ? "STOP BEAT" : "PLAY BEAT");

        if (playing)
            currentStep = 0;

        repaint();
    }

    for (int r = 0; r < 4; ++r)
    {
        for (int s = 0; s < 16; ++s)
        {
            if (button == &steps[r][s])
            {
                pattern[r][s] =
                    steps[r][s].getToggleState();

                repaint();
                return;
            }
        }
    }
}

// ============================================================
// TEMPORIZADOR
// ============================================================

void BeatMaker::timerCallback()
{
    if (!playing)
        return;

    const double millisecondsPerStep =
        60000.0 / (juce::jmax(1, bpmValue) * 4.0);

    static double elapsed = 0.0;

    elapsed += 50.0;

    if (elapsed >= millisecondsPerStep)
    {
        elapsed = 0.0;
        currentStep = (currentStep + 1) % 16;
        repaint();
    }
}
