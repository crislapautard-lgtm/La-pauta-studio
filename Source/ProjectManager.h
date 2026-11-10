
#pragma once

#include <JuceHeader.h>
#include "Track.h"
#include "AudioTrack.h"
#include "TrackComponent.h"

#include <vector>
#include <memory>

// ============================================================
// LA PAUTA STUDIO
// ProjectManager.h
// Guardar y cargar sesiones
// ============================================================

class ProjectManager
{
public:

    // ========================================================
    // GUARDAR PROYECTO
    // ========================================================

    static bool saveProject(
        const juce::File& file,
        const std::vector<std::unique_ptr<Track>>& tracks)
    {
        if (file == juce::File())
            return false;

        juce::ValueTree projectState("LA_PAUTA_SESSION");

        projectState.setProperty("version", "1.0", nullptr);

        juce::ValueTree tracksNode("TRACKS");

        for (const auto& track : tracks)
        {
            if (track == nullptr)
                continue;

            if (auto* audioTrack = dynamic_cast<AudioTrack*>(track.get()))
            {
                juce::ValueTree trackNode("AUDIO_TRACK");

                trackNode.setProperty(
                    "name",
                    audioTrack->getTrackName(),
                    nullptr);

                trackNode.setProperty(
                    "volume",
                    audioTrack->getVolume(),
                    nullptr);

                trackNode.setProperty(
                    "pan",
                    audioTrack->getPan(),
                    nullptr);

                trackNode.setProperty(
                    "muted",
                    audioTrack->isMuted(),
                    nullptr);

                trackNode.setProperty(
                    "soloed",
                    audioTrack->isSoloed(),
                    nullptr);

                trackNode.setProperty(
                    "audioPath",
                    audioTrack->getCurrentAudioFilePath().getFullPathName(),
                    nullptr);

                tracksNode.addChild(trackNode, -1, nullptr);
            }
        }

        projectState.addChild(tracksNode, -1, nullptr);

        std::unique_ptr<juce::XmlElement> xml(
            projectState.createXml());

        if (xml == nullptr)
            return false;

        if (!file.getParentDirectory().exists())
        {
            if (!file.getParentDirectory().createDirectory())
                return false;
        }

        return xml->writeToFile(file, {});
    }

    // ========================================================
    // CARGAR PROYECTO
    // ========================================================

    static bool loadProject(
        const juce::File& file,
        std::vector<std::unique_ptr<Track>>& tracks,
        std::vector<std::unique_ptr<TrackComponent>>& trackComponents,
        juce::Component& parentComponent)
    {
        if (!file.existsAsFile())
            return false;

        auto xml = juce::XmlDocument::parse(file);

        if (xml == nullptr)
            return false;

        juce::ValueTree projectState =
            juce::ValueTree::fromXml(*xml);

        if (!projectState.isValid()
            || projectState.getType() != juce::Identifier("LA_PAUTA_SESSION"))
        {
            return false;
        }

        juce::ValueTree tracksNode =
            projectState.getChildWithName("TRACKS");

        if (!tracksNode.isValid())
            return false;

        // Construir primero las nuevas pistas.
        // Si algo falla, las pistas anteriores se conservan.

        std::vector<std::unique_ptr<Track>> newTracks;

        std::vector<std::unique_ptr<TrackComponent>> newComponents;

        for (int i = 0; i < tracksNode.getNumChildren(); ++i)
        {
            juce::ValueTree trackNode = tracksNode.getChild(i);

            if (trackNode.getType() != juce::Identifier("AUDIO_TRACK"))
                continue;

            juce::String name =
                trackNode.getProperty("name", "Audio Track").toString();

            if (name.isEmpty())
                name = "Audio Track";

            float volume = static_cast<float>(
                trackNode.getProperty("volume", 0.8f));

            float pan = static_cast<float>(
                trackNode.getProperty("pan", 0.0f));

            bool muted =
                static_cast<bool>(trackNode.getProperty("muted", false));

            bool soloed =
                static_cast<bool>(trackNode.getProperty("soloed", false));

            juce::String audioPath =
                trackNode.getProperty("audioPath", "").toString();

            volume = juce::jlimit(0.0f, 2.0f, volume);
            pan = juce::jlimit(-1.0f, 1.0f, pan);

            auto newTrack =
                std::make_unique<AudioTrack>(name.toStdString());

            newTrack->setVolume(volume);
            newTrack->setPan(pan);
            newTrack->setMute(muted);
            newTrack->setSolo(soloed);

            if (audioPath.isNotEmpty())
            {
                juce::File audioFile(audioPath);

                if (audioFile.existsAsFile())
                    newTrack->loadAudioFile(audioFile);
            }

            AudioTrack* trackPointer = newTrack.get();

            auto newTrackComponent =
                std::make_unique<TrackComponent>(name);

            newTrackComponent->onVolumeChanged =
                [trackPointer](float value)
                {
                    trackPointer->setVolume(value);
                };

            newTrackComponent->onPanChanged =
                [trackPointer](float value)
                {
                    trackPointer->setPan(value);
                };

            newTrackComponent->onMuteToggled =
                [trackPointer](bool value)
                {
                    trackPointer->setMute(value);
                };

            newTrackComponent->onSoloToggled =
                [trackPointer](bool value)
                {
                    trackPointer->setSolo(value);
                };

            newTrackComponent->onRecordToggled =
                [trackPointer](bool value)
                {
                    trackPointer->setIsRecording(value);
                };

            newTracks.push_back(std::move(newTrack));
            newComponents.push_back(std::move(newTrackComponent));
        }

        // Retirar los componentes anteriores de la ventana.

        for (auto& component : trackComponents)
        {
            if (component != nullptr)
                parentComponent.removeChildComponent(component.get());
        }

        trackComponents.clear();
        tracks.clear();

        // Instalar las nuevas pistas.

        tracks = std::move(newTracks);
        trackComponents = std::move(newComponents);

        for (auto& component : trackComponents)
        {
            if (component != nullptr)
                parentComponent.addAndMakeVisible(component.get());
        }

        parentComponent.resized();
        parentComponent.repaint();

        return true;
    }
};
