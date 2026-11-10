
#pragma once

#include <JuceHeader.h>

// ============================================================
// LA PAUTA STUDIO
// PLUGIN MANAGER
// VST3 HOST - 64 BITS
// JUCE 9.0.5
// ============================================================

class PluginManager
{
public:

    PluginManager();

    ~PluginManager();

    // Escaneo de plugins
    void scanPlugins();

    // Lista de plugins encontrados
    const juce::KnownPluginList& getKnownPlugins() const;

    // Cantidad de plugins
    int getNumPlugins() const;

    // Nombre de plugin
    juce::String getPluginName(int index) const;

    // Acceso al administrador de formatos
    juce::AudioPluginFormatManager& getFormatManager();

private:

    juce::AudioPluginFormatManager formatManager;

    juce::KnownPluginList knownPlugins;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
        PluginManager
    )
};
