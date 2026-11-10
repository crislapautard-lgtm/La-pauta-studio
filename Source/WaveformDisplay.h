
#pragma once

#include <JuceHeader.h>

class WaveformDisplay : public juce::Component,
                        public juce::ChangeListener
{
public:
    explicit WaveformDisplay(
        juce::AudioFormatManager& formatManager)
        : thumbnailCache(5),
          thumbnail(512, formatManager, thumbnailCache)
    {
        thumbnail.addChangeListener(this);
    }

    ~WaveformDisplay() override
    {
        thumbnail.removeChangeListener(this);
    }

    void setFile(const juce::File& file)
    {
        if (!file.existsAsFile())
            return;

        thumbnail.setSource(
            new juce::FileInputSource(file));

        fileLoaded = true;
        repaint();
    }

    void clear()
    {
        thumbnail.clear();
        fileLoaded = false;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff171c24));

        g.setColour(juce::Colour(0xff303b49));
        g.drawRect(getLocalBounds(), 1);

        if (fileLoaded && thumbnail.getTotalLength() > 0.0)
        {
            g.setColour(juce::Colour(0xff36d6c5));

            thumbnail.drawChannels(
                g,
                getLocalBounds().reduced(3),
                0.0,
                thumbnail.getTotalLength(),
                1.0f);
        }
        else
        {
            g.setColour(juce::Colours::lightgrey);
            g.setFont(12.0f);

            g.drawText(
                "SIN AUDIO - LISTO PARA GRABAR",
                getLocalBounds(),
                juce::Justification::centred,
                true);
        }
    }

    void resized() override
    {
    }

    void changeListenerCallback(
        juce::ChangeBroadcaster* source) override
    {
        if (source == &thumbnail)
            repaint();
    }

private:
    juce::AudioThumbnailCache thumbnailCache;
    juce::AudioThumbnail thumbnail;

    bool fileLoaded = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
        WaveformDisplay)
};
