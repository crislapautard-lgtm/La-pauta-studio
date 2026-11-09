#include <JuceHeader.h>

// Componente principal integrado directamente para garantizar compilación limpia
class MainContentComponent : public juce::Component
{
public:
    MainContentComponent()
    {
        setSize(800, 600);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::darkgrey);
        g.setColour(juce::Colours::white);
        g.setFont(24.0f);
        g.drawText("La Pauta Studio - Listo para Grabar", getLocalBounds(), juce::Justification::centred, true);
    }

    void resized() override {}
};

class LaPautaStudioApplication : public juce::JUCEApplication
{
public:
    LaPautaStudioApplication() {}

    const juce::String getApplicationName() override       { return "La Pauta Studio"; }
    const juce::String getApplicationVersion() override    { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise (const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted (const juce::String&) override
    {
    }

    class MainWindow   : public juce::DocumentWindow
    {
    public:
        MainWindow (const juce::String& name)
            : DocumentWindow (name,
                              juce::Desktop::getInstance().getDefaultLookAndFeel()
                                  .findColour (juce::ResizableWindow::backgroundColourId),
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            
            // Usamos nuestro componente integrado infalible
            setContentOwned (new MainContentComponent(), true);

            centreWithSize (1024, 768);
            setResizable (true, true);
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (LaPautaStudioApplication)
