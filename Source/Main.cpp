#include <JuceHeader.h>
#include "UI.h"

class LaPautaStudioApp : public JUCEApplication
{
public:
    const String getApplicationName() override       { return "La Pauta Studio"; }
    const String getApplicationVersion() override    { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override       { return true; }

    void initialise (const String&) override         { mainWindow = std::make_unique<MainWindow> (getApplicationName()); }
    void shutdown() override                         { mainWindow = nullptr; }
    void systemRequestedQuit() override              { quit(); }

    class MainWindow : public DocumentWindow
    {
    public:
        explicit MainWindow (const String& name)
            : DocumentWindow (name, Colour (0xff151515), DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (true, true);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);

            // Renderizador por software: funciona en PCs sin buena tarjeta de video.
            if (auto* peer = getPeer())
                peer->setCurrentRenderingEngine (0);
        }

        void closeButtonPressed() override { JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (LaPautaStudioApp)
