
#pragma once

#include <JuceHeader.h>

// ============================================================
// LA PAUTA STUDIO
// PLUGIN MANAGER
// VST3 HOST - 64 BITS
// ============================================================

class PluginManager
{
public:

    PluginManager();
    ~PluginManager();

    // Buscar plugins VST3 instalados
    void scanPlugins();

    // Obtener plugins encontrados
    const juce::KnownPluginList& getKnownPlugins() const;

    int getNumPlugins() const;

    juce::String getPluginName(int index) const;

    // Acceso al administrador de formatos
    juce::AudioPluginFormatManager& getFormatManager();

private:

    juce::AudioPluginFormatManager formatManager;

    juce::KnownPluginList knownPlugins;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginManager)
};
