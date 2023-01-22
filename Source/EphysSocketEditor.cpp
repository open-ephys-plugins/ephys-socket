#include "EphysSocketEditor.h"
#include "EphysSocket.h"

#include <string>
#include <iostream>

using namespace EphysSocketNode;

EphysSocketEditor::EphysSocketEditor(GenericProcessor* parentNode, EphysSocket *socket) : GenericEditor(parentNode)
{
    node = socket;

    desiredWidth = 240;

    // Add connect button
    connectButton = new UtilityButton("CONNECT", Font("Small Text", 12, Font::bold));
    connectButton->setRadius(3.0f);
    connectButton->setBounds(10, 35, 70, 20);
    connectButton->addListener(this);
    addAndMakeVisible(connectButton);

    // Port
    portLabel = new Label("Port", "Port");
    portLabel->setFont(Font("Small Text", 10, Font::plain));
    portLabel->setBounds(5, 60, 65, 8);
    portLabel->setColour(Label::textColourId, Colours::darkgrey);
    addAndMakeVisible(portLabel);

    portInput = new Label("Port", String(node->port));
    portInput->setFont(Font("Small Text", 10, Font::plain));
    portInput->setColour(Label::backgroundColourId, Colours::lightgrey);
    portInput->setEditable(true);
    portInput->addListener(this);
    portInput->setBounds(10, 70, 65, 15);
    addAndMakeVisible(portInput);

    //---
    bufferSizeMainLabel = new Label("BUFFER SIZE", "BUFFER SIZE");
    bufferSizeMainLabel->setFont(Font("Small Text", 12, Font::plain));
    bufferSizeMainLabel->setBounds(114, 30, 95, 15);
    bufferSizeMainLabel->setColour(Label::textColourId, Colours::darkgrey);
    addAndMakeVisible(bufferSizeMainLabel);

    // Num chans
    channelCountLabel = new Label("CHANNELS", "CHANNELS");
    channelCountLabel->setFont(Font("Small Text", 10, Font::plain));
    channelCountLabel->setBounds(92, 48, 65, 8);
    channelCountLabel->setColour(Label::textColourId, Colours::darkgrey);
    addAndMakeVisible(channelCountLabel);

    channelCountInput = new Label("Channel count", String(node->num_channels));
    channelCountInput->setFont(Font("Small Text", 10, Font::plain));
    channelCountInput->setBounds(100, 60, 50, 15);
    channelCountInput->setColour(Label::backgroundColourId, Colours::lightgrey);
    channelCountInput->setEditable(true);
    channelCountInput->addListener(this);
    addAndMakeVisible(channelCountInput);

    xLabel = new Label("X", "X");
    xLabel->setFont(Font("Small Text", 15, Font::plain));
    xLabel->setBounds(149, 53, 30, 30);
    xLabel->setColour(Label::textColourId, Colours::darkgrey);
    addAndMakeVisible(xLabel);


    // Num samples
    bufferSizeLabel = new Label("SAMPLES", "SAMPLES");
    bufferSizeLabel->setFont(Font("Small Text", 10, Font::plain));
    bufferSizeLabel->setBounds(164, 48, 65, 8);
    bufferSizeLabel->setColour(Label::textColourId, Colours::darkgrey);
    addAndMakeVisible(bufferSizeLabel);

    bufferSizeInput = new Label ("Buffer Size", String(node->num_samp));
    bufferSizeInput->setFont(Font("Small Text", 10, Font::plain));
    bufferSizeInput->setBounds(170, 60, 50, 15);
    bufferSizeInput->setEditable(true);
    bufferSizeInput->setColour(Label::backgroundColourId, Colours::lightgrey);
    bufferSizeInput->addListener(this);
    addAndMakeVisible(bufferSizeInput);

    // Fs
    sampleRateLabel = new Label("FREQ (HZ)", "FREQ (HZ)");
    sampleRateLabel->setFont(Font("Small Text", 10, Font::plain));
    sampleRateLabel->setBounds(5, 92, 85, 8);
    sampleRateLabel->setColour(Label::textColourId, Colours::darkgrey);
    addAndMakeVisible(sampleRateLabel);

    sampleRateInput = new Label("Fs (Hz)", String((int) node->sample_rate));
    sampleRateInput->setFont(Font("Small Text", 10, Font::plain));
    sampleRateInput->setBounds(10, 105, 65, 15);
    sampleRateInput->setEditable(true);
    sampleRateInput->setColour(Label::backgroundColourId, Colours::lightgrey);
    sampleRateInput->addListener(this);
    addAndMakeVisible(sampleRateInput);

    //---

    // Scale
    scaleLabel = new Label("Scale", "Scale");
    scaleLabel->setFont(Font("Small Text", 10, Font::plain));
    scaleLabel->setBounds(85, 92, 65, 8);
    scaleLabel->setColour(Label::textColourId, Colours::darkgrey);
    addAndMakeVisible(scaleLabel);

    scaleInput = new Label("Scale", String(node->data_scale));
    scaleInput->setFont(Font("Small Text", 10, Font::plain));
    scaleInput->setBounds(90, 105, 50, 15);
    scaleInput->setEditable(true);
    scaleInput->setColour(Label::backgroundColourId, Colours::lightgrey);
    scaleInput->addListener(this);
    addAndMakeVisible(scaleInput);

    // Offset
    offsetLabel = new Label("Offset", "Offset");
    offsetLabel->setFont(Font("Small Text", 10, Font::plain));
    offsetLabel->setBounds(150, 92, 65, 8);
    offsetLabel->setColour(Label::textColourId, Colours::darkgrey);
    addAndMakeVisible(offsetLabel);

    offsetInput = new Label("Offset", String(node->data_offset));
    offsetInput->setFont(Font("Small Text", 10, Font::plain));
    offsetInput->setBounds(155, 105, 50, 15);
    offsetInput->setEditable(true);
    offsetInput->setColour(Label::backgroundColourId, Colours::lightgrey);
    offsetInput->addListener(this);
    addAndMakeVisible(offsetInput);

    transposeButton.setBounds(155, 95, 65, 20);
    transposeButton.setClickingTogglesState(true);
    transposeButton.setToggleState(true, false);
   // addAndMakeVisible(transposeButton);
}

void EphysSocketEditor::labelTextChanged(Label* label)
{

    if (label == channelCountInput)
    {

        int num_channels = channelCountInput->getText().getIntValue();

        if (num_channels > 0 && num_channels < 1000)
        {
            node->num_channels = num_channels;
            CoreServices::updateSignalChain(this);
        }
        else {
            channelCountInput->setText(String(node->num_channels), dontSendNotification);
        }
        
    }
    else if (label == sampleRateInput)
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
    else if (label == bufferSizeInput)
    {
        int bufferSize = bufferSizeInput->getText().getIntValue();

        if (bufferSize > 0 && bufferSize < 2048)
        {
            node->num_samp = bufferSize;
        }
        else {
            bufferSizeInput->setText(String(node->num_samp), dontSendNotification);
        }
    }
    else if (label == scaleInput)
    {
        float scale = scaleInput->getText().getFloatValue();

        if (scale > 0.0f && scale < 9999.9f)
        {
            node->data_scale = scale;
        }
        else {
            scaleInput->setText(String(node->data_scale), dontSendNotification);
        }
    }
    else if (label == offsetInput)
    {
        int offset = offsetInput->getText().getIntValue();

        if (offset > 0 && offset < 65536)
        {
            node->data_offset = offset;
        }
        else {
            offsetInput->setText(String(node->data_offset), dontSendNotification);
        }
    }
}

void EphysSocketEditor::startAcquisition()
{
    // Disable the whole UI
    portInput->setEnabled(false);
    channelCountInput->setEnabled(false);
    sampleRateInput->setEnabled(false);
    bufferSizeInput->setEnabled(false);
    scaleInput->setEnabled(false);
    offsetInput->setEnabled(false);
    connectButton->setEnabled(false);
    transposeButton.setEnabled(false);

    // Set the channels etc
    //node->data_scale = scaleInput->getText().getFloatValue();
   // node->data_offset = offsetInput->getText().getIntValue();
    //node->transpose = transposeButton.getToggleState();

    //node->resizeBuffers();
}

void EphysSocketEditor::stopAcquisition()
{
    // Reenable the whole UI
    portInput->setEnabled(true);
    channelCountInput->setEnabled(true);
    sampleRateInput->setEnabled(true);
    bufferSizeInput->setEnabled(true);
    scaleInput->setEnabled(true);
    offsetInput->setEnabled(true);
    connectButton->setEnabled(true);
    transposeButton.setEnabled(true);
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
    parameters->setAttribute("numchan", channelCountInput->getText());
    parameters->setAttribute("numsamp", bufferSizeInput->getText());
    parameters->setAttribute("fs", sampleRateInput->getText());
    parameters->setAttribute("scale", scaleInput->getText());
    parameters->setAttribute("offset", offsetInput->getText());
}

void EphysSocketEditor::loadCustomParametersFromXml(XmlElement* xmlNode)
{
    forEachXmlChildElement(*xmlNode, subNode)
    {
        if (subNode->hasTagName("PARAMETERS"))
        {
            portInput->setText(subNode->getStringAttribute("port", ""), dontSendNotification);
            node->port = subNode->getIntAttribute("port", DEFAULT_PORT);

            channelCountInput->setText(subNode->getStringAttribute("numchan", ""), dontSendNotification);
            node->num_channels = subNode->getIntAttribute("numchan", DEFAULT_NUM_CHANNELS);

            bufferSizeInput->setText(subNode->getStringAttribute("numsamp", ""), dontSendNotification);
            node->num_samp = subNode->getIntAttribute("numsamp", DEFAULT_NUM_SAMPLES);

            sampleRateInput->setText(subNode->getStringAttribute("fs", ""), dontSendNotification);
            node->sample_rate = subNode->getDoubleAttribute("fs", DEFAULT_SAMPLE_RATE);

            scaleInput->setText(subNode->getStringAttribute("scale", ""), dontSendNotification);
            node->data_scale = subNode->getDoubleAttribute("scale", DEFAULT_DATA_SCALE);

            offsetInput->setText(subNode->getStringAttribute("offset", ""), dontSendNotification);
            node->data_offset = subNode->getIntAttribute("offset", DEFAULT_DATA_OFFSET);
        }
    }
}

