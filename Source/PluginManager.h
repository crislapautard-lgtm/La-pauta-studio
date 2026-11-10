
#pragma once

#include <JuceHeader.h>
#include <memory>
#include <vector>

// ============================================================
// LA PAUTA STUDIO
// PLUGIN MANAGER
// VST3 HOST - 64 BITS
// ============================================================

class PluginManager
{
public:

    PluginManager()
    {
        formatManager.addDefaultFormats();
    }

    ~PluginManager() = default;

    // ========================================================
    // ESCANEAR PLUGINS VST3
    // ========================================================

    void scanPlugins()
    {
        knownPlugins.clear();

        auto formats = formatManager.getFormats();

        for (auto* format : formats)
        {
            if (format == nullptr)
                continue;

            if (format->getName().containsIgnoreCase("VST3"))
            {
                juce::FileSearchPath searchPaths;

                searchPaths.add(
                    juce::File(
                        "C:\\Program Files\\Common Files\\VST3"
                    )
                );

                searchPaths.add(
                    juce::File(
                        "C:\\Program Files (x86)\\Common Files\\VST3"
                    )
                );

                formatManager.scanAndAddFile(
                    searchPaths.toString(),
                    true,
                    knownPlugins,
                    *format
                );
            }
        }
    }

    // ========================================================
    // OBTENER PLUGINS DETECTADOS
    // ========================================================

    const juce::KnownPluginList& getKnownPlugins() const
    {
        return knownPlugins;
    }

    int getNumPlugins() const
    {
        return knownPlugins.getNumTypes();
    }

    juce::String getPluginName(int index) const
    {
        const auto& plugins = knownPlugins.getTypes();

        if (index < 0 || index >= plugins.size())
            return {};

        return plugins.getReference(index).name;
    }

    // ========================================================
    // OBTENER FORMATO VST3
    // ========================================================

    juce::AudioPluginFormatManager& getFormatManager()
    {
        return formatManager;
    }

private:

    juce::AudioPluginFormatManager formatManager;

    juce::KnownPluginList knownPlugins;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginManager)
};
