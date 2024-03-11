#include "EphysSocketEditor.h"
#include "EphysSocket.h"

#include <string>
#include <iostream>

using namespace EphysSocketNode;

EphysSocketEditor::EphysSocketEditor(GenericProcessor* parentNode, EphysSocket *socket) : GenericEditor(parentNode)
{
    node = socket;

    desiredWidth = 120;

    // Add connect button
    connectButton = new UtilityButton("CONNECT", Font("Small Text", 12, Font::bold));
    connectButton->setRadius(3.0f);
    connectButton->setBounds(25, 35, 70, 20);
    connectButton->addListener(this);
    addAndMakeVisible(connectButton);

    // Port
    portLabel = new Label("Port", "Port");
    portLabel->setFont(Font("Small Text", 10, Font::plain));
    portLabel->setBounds(20, 63, 70, 8);
    portLabel->setColour(Label::textColourId, Colours::darkgrey);
    addAndMakeVisible(portLabel);

    portInput = new Label("Port", String(node->port));
    portInput->setFont(Font("Small Text", 10, Font::plain));
    portInput->setColour(Label::backgroundColourId, Colours::lightgrey);
    portInput->setEditable(true);
    portInput->addListener(this);
    portInput->setBounds(25, 73, 70, 15);
    addAndMakeVisible(portInput);

    // Frequency
    sampleRateLabel = new Label("FREQ (HZ)", "Freq (Hz)");
    sampleRateLabel->setFont(Font("Small Text", 10, Font::plain));
    sampleRateLabel->setBounds(20, 98, 85, 8);
    sampleRateLabel->setColour(Label::textColourId, Colours::darkgrey);
    addAndMakeVisible(sampleRateLabel);

    sampleRateInput = new Label("Fs (Hz)", String((int) node->sample_rate));
    sampleRateInput->setFont(Font("Small Text", 10, Font::plain));
    sampleRateInput->setBounds(25, 108, 70, 15);
    sampleRateInput->setEditable(true);
    sampleRateInput->setColour(Label::backgroundColourId, Colours::lightgrey);
    sampleRateInput->addListener(this);
    addAndMakeVisible(sampleRateInput);
}

void EphysSocketEditor::labelTextChanged(Label* label)
{
    if (label == sampleRateInput)
    {
        float sampleRate = sampleRateInput->getText().getFloatValue();

        if (sampleRate > 0 && sampleRate < 50000.0f)
        {
            node->sample_rate = sampleRate;
            CoreServices::updateSignalChain(this);
        }
        else {
            sampleRateInput->setText(String(node->sample_rate), dontSendNotification);
        }
    }
    else if (label == portInput)
    {
        int port = portInput->getText().getIntValue();

        if (port > 1023 && port < 65535)
        {
            node->port = port;
        }
        else {
            portInput->setText(String(node->port), dontSendNotification);
        }
    }
}

void EphysSocketEditor::startAcquisition()
{
    // Disable the whole UI
    portInput->setEnabled(false);
    sampleRateInput->setEnabled(false);
    connectButton->setEnabled(false);
}

void EphysSocketEditor::stopAcquisition()
{
    // Reenable the whole UI
    portInput->setEnabled(true);
    sampleRateInput->setEnabled(true);
    connectButton->setEnabled(true);
}

void EphysSocketEditor::buttonClicked(Button* button)
{
    if (button == connectButton && !acquisitionIsActive)
    {
        node->port = portInput->getText().getIntValue();
        node->tryToConnect();

        CoreServices::updateSignalChain(this);
    }
}

void EphysSocketEditor::saveCustomParametersToXml(XmlElement* xmlNode)
{
    XmlElement* parameters = xmlNode->createNewChildElement("PARAMETERS");

    parameters->setAttribute("port", portInput->getText());
    parameters->setAttribute("fs", sampleRateInput->getText());
}

void EphysSocketEditor::loadCustomParametersFromXml(XmlElement* xmlNode)
{
    forEachXmlChildElement(*xmlNode, subNode)
    {
        if (subNode->hasTagName("PARAMETERS"))
        {
            portInput->setText(subNode->getStringAttribute("port", ""), dontSendNotification);
            node->port = subNode->getIntAttribute("port", node->DEFAULT_PORT);

            sampleRateInput->setText(subNode->getStringAttribute("fs", ""), dontSendNotification);
            node->sample_rate = subNode->getDoubleAttribute("fs", node->DEFAULT_SAMPLE_RATE);
        }
    }
}

