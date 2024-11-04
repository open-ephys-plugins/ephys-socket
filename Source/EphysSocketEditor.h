#ifndef __EPHYSSOCKETEDITORH__
#define __EPHYSSOCKETEDITORH__

#ifdef _WIN32
#include <Windows.h>
#endif

#include <VisualizerEditorHeaders.h>

#include "EphysSocketHeader.h"

namespace EphysSocketNode
{
    class EphysSocket;

    class EphysSocketEditor : public GenericEditor, 
                              public Label::Listener,
                              public Button::Listener
    {

    public:

        /** Constructor */
        EphysSocketEditor(GenericProcessor* parentNode, EphysSocket *node);

        /** Button listener callback, called by button when pressed. */
        void buttonClicked(Button* button);

        /** Called by processor graph in beginning of the acquisition, disables editor completely. */
        void startAcquisition();

        /** Called by processor graph at the end of the acquisition, reenables editor completely. */
        void stopAcquisition();

        /** Called when configuration is saved. Adds editors config to xml. */
        void saveCustomParametersToXml(XmlElement* xml) override;

        /** Called when configuration is loaded. Reads editors config from xml. */
        void loadCustomParametersFromXml(XmlElement* xml) override;

        /** Called when label is changed */
        void labelTextChanged(Label* label);

        // Changes colors and disables UI elements
        void disableInputs();

        // Changes colors and enables UI elements
        void enableInputs();

    private:

        // Button that connects/disconnects from/to server
        std::unique_ptr<UtilityButton> connectButton;
        std::unique_ptr<UtilityButton> disconnectButton;

        String stringConnect = "CONNECT";
        String stringDisconnect = "DISCONNECT";

        // Port
        std::unique_ptr<Label> portLabel;
        std::unique_ptr<Label> portInput;

        // Fs
        std::unique_ptr<Label> sampleRateLabel;
        std::unique_ptr<Label> sampleRateInput;

        // Scale
        std::unique_ptr<Label> scaleLabel;
        std::unique_ptr<Label> scaleInput;

        // Offset
        std::unique_ptr<Label> offsetLabel;
        std::unique_ptr<Label> offsetInput;

        // Parent node
        EphysSocket* node;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EphysSocketEditor);
    };
}

#endif