#include "EphysSocketEditor.h"
#include "EphysSocket.h"

#include <iostream>
#include <string>

using namespace EphysSocketNode;

EphysSocketEditor::EphysSocketEditor (GenericProcessor* parentNode, EphysSocket* socket) : GenericEditor (parentNode)
{
    node = socket;

    desiredWidth = 180;

    // Add connect button
    connectButton = std::make_unique<UtilityButton> (stringConnect);
    connectButton->setFont (FontOptions ("Small Text", 12, Font::bold));
    connectButton->setRadius (3.0f);
    connectButton->setBounds (15, 35, 70, 20);
    connectButton->addListener (this);
    addAndMakeVisible (connectButton.get());

    disconnectButton = std::make_unique<UtilityButton> (stringDisconnect);
    disconnectButton->setFont (FontOptions ("Small Text", 12, Font::bold));
    disconnectButton->setRadius (3.0f);
    disconnectButton->setBounds (15, 35, 70, 20);
    disconnectButton->addListener (this);
    addAndMakeVisible (disconnectButton.get());
    disconnectButton->setVisible (false);

    addTextBoxParameterEditor (Parameter::PROCESSOR_SCOPE, "port", 10, 60);
    addTextBoxParameterEditor (Parameter::PROCESSOR_SCOPE, "sample_rate", 10, 95);
    addTextBoxParameterEditor (Parameter::PROCESSOR_SCOPE, "data_scale", 95, 60);
    addTextBoxParameterEditor (Parameter::PROCESSOR_SCOPE, "data_offset", 95, 95);

    for (auto& ed : parameterEditors)
    {
        ed->setLayout (ParameterEditor::Layout::nameOnTop);
        ed->setBounds (ed->getX(), ed->getY(), 80, 30);
    }
}

void EphysSocketEditor::startAcquisition()
{
    disconnectButton->setEnabled (false);
    disconnectButton->setAlpha (0.2f);
}

void EphysSocketEditor::stopAcquisition()
{
    if (node->errorFlag())
    {
        node->disconnectSocket();
    }

    disconnectButton->setEnabled (true);
    disconnectButton->setAlpha (1.0f);
}

void EphysSocketEditor::buttonClicked (Button* button)
{
    if (button == connectButton.get() && ! acquisitionIsActive)
    {
        node->connectSocket();

        CoreServices::updateSignalChain (this);
    }
    else if (button == disconnectButton.get() && ! acquisitionIsActive)
    {
        node->disconnectSocket();
    }
}

void EphysSocketEditor::connected()
{
    connectButton->setVisible (false);
    disconnectButton->setVisible (true);
}

void EphysSocketEditor::disconnected()
{
    connectButton->setVisible (true);
    disconnectButton->setVisible (false);
}
