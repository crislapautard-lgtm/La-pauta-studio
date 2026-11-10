
#include "PluginManager.h"

// ============================================================
// CONSTRUCTOR
// ============================================================

PluginManager::PluginManager()
    : juce::Thread("La Pauta VST3 Scanner")
{
#if JUCE_PLUGINHOST_VST3
    formatManager.addFormat(
        std::make_unique<juce::VST3PluginFormat>()
    );
#endif
}

// ============================================================
// DESTRUCTOR
// ============================================================

PluginManager::~PluginManager()
{
    stopThread(5000);
}

// ============================================================
// INICIAR ESCANEO ASINCRONO
// ============================================================

void PluginManager::scanPluginsAsync(
    std::function<void()> onFinished)
{
    if (isThreadRunning())
        return;

    scanFinishedCallback = std::move(onFinished);

    startThread();
}

// ============================================================
// ESTADO DEL ESCANEO
// ============================================================

bool PluginManager::isScanning() const
{
    return isThreadRunning();
}

// ============================================================
// PROCESO DEL HILO DE ESCANEO
// ============================================================

void PluginManager::run()
{
    knownPlugins.clear();

#if JUCE_PLUGINHOST_VST3

    juce::FileSearchPath vst3Paths;

    // CARPETA ESTANDAR VST3 DE WINDOWS 64 BITS

    vst3Paths.add(
        juce::File(
            "C:\\Program Files\\Common Files\\VST3"
        )
    );

    const auto& formats = formatManager.getFormats();

    for (int i = 0; i < formats.size(); ++i)
    {
        if (threadShouldExit())
            break;

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

        while (!threadShouldExit()
               && scanner.scanNextFile(
                   cancelled,
                   pluginBeingScanned))
        {
            if (cancelled || threadShouldExit())
                break;
        }
    }

#endif

    // DEVOLVER EL RESULTADO A LA INTERFAZ

    auto callback = std::move(scanFinishedCallback);

    juce::MessageManager::callAsync(
        [callback = std::move(callback)]() mutable
        {
            if (callback)
                callback();
        }
    );
}

// ============================================================
// LISTA DE PLUGINS
// ============================================================

const juce::KnownPluginList&
PluginManager::getKnownPlugins() const
{
    return knownPlugins;
}

// ============================================================
// CANTIDAD
// ============================================================

int PluginManager::getNumPlugins() const
{
    return knownPlugins.getNumTypes();
}

// ============================================================
// NOMBRE
// ============================================================

juce::String PluginManager::getPluginName(int index) const
{
    const auto& plugins = knownPlugins.getTypes();

    if (index < 0 || index >= plugins.size())
        return {};

    return plugins.getReference(index).name;
}

// ============================================================
// FORMAT MANAGER
// ============================================================

juce::AudioPluginFormatManager&
PluginManager::getFormatManager()
{
    return formatManager;
}
