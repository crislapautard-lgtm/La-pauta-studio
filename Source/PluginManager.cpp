
#include "PluginManager.h"

// ============================================================
// LA PAUTA STUDIO
// PLUGIN MANAGER
// ============================================================

PluginManager::PluginManager()
{
    formatManager.addDefaultFormats();
}

PluginManager::~PluginManager() = default;

// ============================================================
// ESCANEAR PLUGINS VST3
// ============================================================

void PluginManager::scanPlugins()
{
    knownPlugins.clear();

    const juce::FileSearchPath searchPaths;

    juce::FileSearchPath vst3Paths;

    vst3Paths.add(
        juce::File(
            "C:\\Program Files\\Common Files\\VST3"
        )
    );

    vst3Paths.add(
        juce::File(
            "C:\\Program Files (x86)\\Common Files\\VST3"
        )
    );

    const auto& formats = formatManager.getFormats();

    for (int i = 0; i < formats.size(); ++i)
    {
        auto* format = formats.getUnchecked(i);

        if (format == nullptr)
            continue;

        if (!format->getName().containsIgnoreCase("VST3"))
            continue;

        juce::PluginDirectoryScanner scanner(
            knownPlugins,
            *format,
            vst3Paths,
            true,
            juce::File()
        );

        bool cancelled = false;
        juce::String pluginBeingScanned;

        while (scanner.scanNextFile(
            cancelled,
            pluginBeingScanned))
        {
            if (cancelled)
                break;
        }
    }
}

// ============================================================
// CONSULTAR PLUGINS
// ============================================================

const juce::KnownPluginList&
PluginManager::getKnownPlugins() const
{
    return knownPlugins;
}

int PluginManager::getNumPlugins() const
{
    return knownPlugins.getNumTypes();
}

juce::String PluginManager::getPluginName(int index) const
{
    const auto& plugins = knownPlugins.getTypes();

    if (index < 0 || index >= plugins.size())
        return {};

    return plugins.getReference(index).name;
}

// ============================================================
// FORMATO DE PLUGINS
// ============================================================

juce::AudioPluginFormatManager&
PluginManager::getFormatManager()
{
    return formatManager;
}
