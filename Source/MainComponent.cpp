#include "MainComponent.h"

MainComponent::MainComponent(AudioPluginAudioProcessor& p) : audioProcessor(p)
{
    for (int i = 0; i < AudioPluginAudioProcessor::numTracks; ++i)
    {
        auto trackName = "Vocal " + juce::String(i + 1);
        auto trackComp = std::make_unique<TrackComponent>(trackName);

        trackComp->onVolumeChanged = [this, i](float val) { audioProcessor.trackVolume[i].store(val); };
        trackComp->onPanChanged    = [this, i](float val) { audioProcessor.trackPan[i].store(val); };
        trackComp->onMuteToggled   = [this, i](bool val)  { audioProcessor.trackMute[i].store(val); return val; };
        trackComp->onSoloToggled   = [this, i](bool val)  { audioProcessor.trackSolo[i].store(val); return val; };
        trackComp->onRecordToggled = [this, i](bool val)  { 
            audioProcessor.setTrackRecording(i, val); 
            return val; 
        };

        trackContainer.addAndMakeVisible(trackComp.get());
        tracks.push_back(std::move(trackComp));
    }

    for (int s = 0; s < AudioPluginAudioProcessor::numSubgroups; ++s)
    {
        auto subName = "SUB " + juce::String(s + 1);
        auto subComp = std::make_unique<TrackComponent>(subName);

        subComp->onVolumeChanged = [this, s](float val) { audioProcessor.subgroupVolume[s].store(val); };
        
        trackContainer.addAndMakeVisible(subComp.get());
        subgroups.push_back(std::move(subComp));
    }

    for (int a = 0; a < AudioPluginAudioProcessor::numAuxes; ++a)
    {
        auto auxName = (a == 0) ? "AUX Reverb" : "AUX Delay";
        auto auxComp = std::make_unique<TrackComponent>(auxName);

        auxComp->onVolumeChanged = [this, a](float val) { audioProcessor.auxVolume[a].store(val); };

        trackContainer.addAndMakeVisible(auxComp.get());
        auxes.push_back(std::move(auxComp));
    }

    masterTrack.onVolumeChanged = [this](float val) { audioProcessor.masterVolume.store(val); };
    trackContainer.addAndMakeVisible(masterTrack);

    // Control de BPM interactivo en tiempo real
    bpmSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    bpmSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    bpmSlider.setRange(40.0, 300.0, 1.0);
    bpmSlider.setValue(audioProcessor.currentBPM.load());
    bpmSlider.onValueChange = [this] {
        audioProcessor.setBPM((float)bpmSlider.getValue());
    };
    trackContainer.addAndMakeVisible(bpmSlider);

    bpmLabel.setText("TEMPO (BPM)", juce::dontSendNotification);
    bpmLabel.setJustificationType(juce::Justification::centred);
    bpmLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    trackContainer.addAndMakeVisible(bpmLabel);

    viewport.setViewedComponent(&trackContainer, false);
    viewport.setScrollBarsShown(true, true);
    addAndMakeVisible(viewport);

    setSize(950, 520);
}

MainComponent::~MainComponent() {}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e1e));
}

void MainComponent::resized()
{
    viewport.setBounds(getLocalBounds());

    int trackWidth = 110;
    int spacing = 8;
    
    int totalItems = AudioPluginAudioProcessor::numTracks + AudioPluginAudioProcessor::numSubgroups + AudioPluginAudioProcessor::numAuxes + 1;
    int totalWidth = (totalItems * (trackWidth + spacing)) + 120;

    trackContainer.setBounds(0, 0, totalWidth, getHeight() - 20);

    int xPos = 15;

    for (int i = 0; i < AudioPluginAudioProcessor::numTracks; ++i)
    {
        tracks[i]->setBounds(xPos, 15, trackWidth, 450);
        xPos += trackWidth + spacing;
    }

    xPos += 15;

    for (int s = 0; s < AudioPluginAudioProcessor::numSubgroups; ++s)
    {
        subgroups[s]->setBounds(xPos, 15, trackWidth, 450);
        xPos += trackWidth + spacing;
    }

    xPos += 15;

    for (int a = 0; a < AudioPluginAudioProcessor::numAuxes; ++a)
    {
        auxes[a]->setBounds(xPos, 15, trackWidth, 450);
        xPos += trackWidth + spacing;
    }

    xPos += 20;
    masterTrack.setBounds(xPos, 15, trackWidth, 450);

    xPos += trackWidth + 15;
    bpmLabel.setBounds(xPos, 20, 90, 20);
    bpmSlider.setBounds(xPos, 45, 90, 90);
}
