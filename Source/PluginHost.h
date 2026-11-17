#pragma once
#include <JuceHeader.h>
#include <functional>

using namespace juce;

// Escaneo, lista y creación de plugins VST3.
// El escaneo usa un archivo "dead man's pedal": si un plugin cuelga o cierra el programa,
// al reabrir se marca como defectuoso y se ignora (no rompe el DAW).
class PluginHost
{
public:
    PluginHost()
    {
        formatManager.addFormat (new VST3PluginFormat());

        auto dir = File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("LaPautaStudio");
        dir.createDirectory();
        deadMans = dir.getChildFile ("scan_crash.txt");
        listFile = dir.getChildFile ("plugins.xml");

        if (auto xml = XmlDocument::parse (listFile))
            list.recreateFromXml (*xml);
    }

    void saveList()
    {
        if (auto xml = list.createXml())
            xml->writeTo (listFile);
    }

    // Se llama desde un hilo secundario.
    void scanAll (Thread& thread, std::function<void (const String&, float)> progress)
    {
        for (auto* fmt : formatManager.getFormats())
        {
            PluginDirectoryScanner scanner (list, *fmt, fmt->getDefaultLocationsToSearch(), true, deadMans, true);
            String name;

            while (scanner.scanNextFile (true, name))
            {
                progress (name, (float) scanner.getProgress());
                if (thread.threadShouldExit())
                    return;
            }
        }
        saveList();
    }

    std::unique_ptr<AudioPluginInstance> create (const PluginDescription& d, double sr, int bs, String& err)
    {
        return formatManager.createPluginInstance (d, sr, bs, err);
    }

    String buildReport() const
    {
        String s;
        auto types = list.getTypes();
        s << "Plugins analizados: " << types.size() << "\n\n";

        for (auto& d : types)
        {
            s << d.name << "  |  " << d.manufacturerName << "  |  v" << d.version << "  |  "
              << d.numInputChannels << " in / " << d.numOutputChannels << " out  |  "
              << (d.isInstrument ? "Instrumento" : "Efecto") << "  |  " << d.pluginFormatName << "\n";
        }

        auto bad = list.getBlacklistedFiles();
        if (bad.size() > 0)
        {
            s << "\nPlugins con fallos (ignorados para proteger el DAW):\n";
            for (auto& f : bad)
                s << "   " << f << "\n";
        }
        return s;
    }

    AudioPluginFormatManager formatManager;
    KnownPluginList list;

private:
    File listFile, deadMans;
};
