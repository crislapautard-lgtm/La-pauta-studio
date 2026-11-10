
#include "PluginManager.h"

// ============================================================
// LA PAUTA STUDIO
// PLUGIN MANAGER
// VST3 HOST - JUCE 9.0.5
// ============================================================

PluginManager::PluginManager()
{
#if JUCE_PLUGINHOST_VST3
    formatManager.addFormat(
        std::make_unique<juce::VST3PluginFormat>()
    );
#endif
}

PluginManager::~PluginManager() = default;

// ============================================================
// ESCANEAR PLUGINS VST3
// ============================================================

void PluginManager::scanPlugins()
{
    knownPlugins.clear();

    juce::FileSearchPath vst3Paths;

    // Carpeta VST3 de Windows 64 bits
    vst3Paths.add(
        juce::File(
            "C:\\Program Files\\Common Files\\VST3"
        )
    );

    // Carpeta VST3 alternativa
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

// ============================================================
// CANTIDAD DE PLUGINS
// ============================================================

int PluginManager::getNumPlugins() const
{
    return knownPlugins.getNumTypes();
}

// ============================================================
// NOMBRE DEL PLUGIN
// ============================================================

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
