#pragma once
#include <JuceHeader.h>
#include "AudioTrack.h"
#include "TrackComponent.h"
#include <vector>
#include <memory>

class ProjectManager {
public:
    static bool saveProject(const juce::File& file, const std::vector<std::unique_ptr<Track>>& tracks) {
        juce::ValueTree projectState("LA_PAUTA_SESSION");
        projectState.setProperty("version", "1.0", nullptr);

        juce::ValueTree tracksNode("TRACKS");

        for (size_t i = 0; i < tracks.size(); ++i) {
            if (auto* audioTrack = dynamic_cast<AudioTrack*>(tracks[i].get())) {
                juce::ValueTree trackNode("AUDIO_TRACK");
                trackNode.setProperty("name", audioTrack->getName(), nullptr);
                trackNode.setProperty("volume", audioTrack->getVolume(), nullptr);
                trackNode.setProperty("pan", audioTrack->getPan(), nullptr);
                trackNode.setProperty("muted", audioTrack->isMuted(), nullptr);
                trackNode.setProperty("soloed", audioTrack->isSoloed(), nullptr);
                trackNode.setProperty("audioPath", audioTrack->getCurrentAudioFilePath().getFullPathName(), nullptr);

                tracksNode.addChild(trackNode, -1, nullptr);
            }
        }

        projectState.addChild(tracksNode, -1, nullptr);

        if (auto xml = std::unique_ptr<juce::XmlElement>(projectState.createXml())) {
            return xml->writeToFile(file, "");
        }

        return false;
    }

    static bool loadProject(const juce::File& file, 
                            std::vector<std::unique_ptr<Track>>& tracks, 
                            std::vector<std::unique_ptr<TrackComponent>>& trackComponents, 
                            juce::Component& parentComponent) 
    {
        if (!file.existsAsFile()) return false;

        auto xml = juce::XmlDocument::parse(file);
        if (xml == nullptr) return false;

        juce::ValueTree projectState = juce::ValueTree::fromXml(*xml);
        if (!projectState.isValid() || projectState.getType() != "LA_PAUTA_SESSION") return false;

        tracks.clear();
        trackComponents.clear();

        juce::ValueTree tracksNode = projectState.getChildWithName("TRACKS");
        if (!tracksNode.isValid()) return false;

        for (int i = 0; i < tracksNode.getNumChildren(); ++i) {
            juce::ValueTree trackNode = tracksNode.getChild(i);
            
            if (trackNode.getType() == "AUDIO_TRACK") {
                juce::String name = trackNode.getProperty("name");
                float volume = trackNode.getProperty("volume");
                float pan = trackNode.getProperty("pan");
                bool muted = trackNode.getProperty("muted");
                bool soloed = trackNode.getProperty("soloed");
                juce::String audioPath = trackNode.getProperty("audioPath");

                int trackId = (int)tracks.size() + 1;
                auto newTrack = std::make_unique<AudioTrack>(name);
                
                newTrack->setVolume(volume);
                newTrack->setPan(pan);
                newTrack->setMute(muted);
                newTrack->setSolo(soloed);

                if (audioPath.isNotEmpty()) {
                    juce::File audioFile(audioPath);
                    if (audioFile.existsAsFile()) {
                        newTrack->loadAudioFile(audioFile);
                    }
                }

                auto newTrackComp = std::make_unique<TrackComponent>(trackId, newTrack.get());

                tracks.push_back(std::move(newTrack));
                trackComponents.push_back(std::move(newTrackComp));

                parentComponent.addAndMakeVisible(trackComponents.back().get());
            }
        }

        parentComponent.resized();
        return true;
    }
};
