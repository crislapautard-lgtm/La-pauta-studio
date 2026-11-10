
#pragma once

#include <JuceHeader.h>
#include <functional>

// ============================================================
// LA PAUTA STUDIO
// VST3 PLUGIN MANAGER - WINDOWS x64
// ESCANEO EN SEGUNDO PLANO
// ============================================================

class PluginManager : private juce::Thread
{
public:
    PluginManager();
    ~PluginManager() override;

    void scanPluginsAsync(std::function<void()> onFinished);

    bool isScanning() const;

    const juce::KnownPluginList& getKnownPlugins() const;

    int getNumPlugins() const;

    juce::String getPluginName(int index) const;

    juce::AudioPluginFormatManager& getFormatManager();

private:
    void run() override;

    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPlugins;

    std::function<void()> scanFinishedCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginManager)
};
