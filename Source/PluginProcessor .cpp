#include "PluginProcessor.h"
#include "PluginEditor.h"

LaPautaProcessor::LaPautaProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void LaPautaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
}

bool LaPautaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void LaPautaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    engine.process (buffer);
}

juce::AudioProcessorEditor* LaPautaProcessor::createEditor()
{
    return new LaPautaEditor (*this);
}

// Punto de entrada que JUCE necesita para crear el plugin / la app Standalone
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LaPautaProcessor();
}
