#pragma once
#include <JuceHeader.h>

class WaveformDisplay : public juce::Component,
                        public juce::ChangeBroadcaster,
                        public juce::ChangeListener
{
public:
    WaveformDisplay(juce::AudioFormatManager& formatManager)
        : thumbnailCache(5), // Caché para almacenar los picos de hasta 5 archivos
          thumbnail(512, formatManager, thumbnailCache)
    {
        thumbnail.addChangeListener(this);
    }

    ~WaveformDisplay() override {
        thumbnail.removeChangeListener(this);
    }

    // Cargar el archivo de audio grabado para generar su gráfico
    void setFile(const juce::File& file) {
        thumbnail.setSource(new juce::FileInputSource(file));
        fileLoaded = true;
        repaint();
    }

    void paint (juce::Graphics& g) override {
        // Fondo oscuro elegante estilo REAPER para la región de audio (Item)
        g.fillAll(juce::Colour(0xff252526));
        g.setColour(juce::Colour(0xff333333));
        g.drawRect(getLocalBounds(), 1);

        if (fileLoaded) {
            // Dibujar la forma de onda en color verde/cian profesional
            g.setColour(juce::Colour(0xff4cd137)); // Verde brillante estilo pista activa
            
            thumbnail.drawChannel(g, 
                                  getLocalBounds(), 
                                  0.0,                             // Tiempo inicial (segundos)
                                  thumbnail.getTotalLength(),      // Duración total
                                  0,                               // Canal (0 = Izquierdo)
                                  1.0f);                           // Vertical zoom
        } else {
            // Texto guía si la pista está vacía y lista para grabar
            g.setColour(juce::Colours::grey);
            g.setFont(12.0f);
            g.drawText("Sin audio grabado (Presiona REC)", getLocalBounds(), juce::Justification::centred, true);
        }
    }

    void resized() override {
        // Nada complejo requerido por ahora
    }

    void changeListenerCallback (juce::ChangeBroadcaster* source) override {
        // Cuando JUCE termina de calcular los picos del archivo en segundo plano, repintamos la pantalla
        if (source == &thumbnail) {
            repaint();
        }
    }

private:
    juce::AudioThumbnailCache thumbnailCache;
    juce::AudioThumbnail thumbnail;
    bool fileLoaded = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformDisplay)
};
