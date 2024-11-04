#include "EphysSocketEditor.h"
#include "EphysSocket.h"

#include <string>
#include <iostream>

using namespace EphysSocketNode;

EphysSocketEditor::EphysSocketEditor(GenericProcessor* parentNode, EphysSocket* socket) : GenericEditor(parentNode)
{
	node = socket;

	desiredWidth = 180;

	// Add connect button
	connectButton = std::make_unique<UtilityButton>(stringConnect, Font("Small Text", 12, Font::bold));
	connectButton->setRadius(3.0f);
	connectButton->setBounds(15, 35, 70, 20);
	connectButton->addListener(this);
	addAndMakeVisible(connectButton.get());

	disconnectButton = std::make_unique<UtilityButton>(stringDisconnect, Font("Small Text", 12, Font::bold));
	disconnectButton->setRadius(3.0f);
	disconnectButton->setBounds(15, 35, 70, 20);
	disconnectButton->addListener(this);
	addAndMakeVisible(disconnectButton.get());
	disconnectButton->setVisible(false);

	// Port
	portLabel = std::make_unique<Label>("Port", "Port");
	portLabel->setFont(Font("Small Text", 10, Font::plain));
	portLabel->setBounds(10, 63, 70, 8);
	portLabel->setColour(Label::textColourId, Colours::darkgrey);
	addAndMakeVisible(portLabel.get());

	portInput = std::make_unique<Label>("Port", String(node->port));
	portInput->setFont(Font("Small Text", 10, Font::plain));
	portInput->setColour(Label::backgroundColourId, Colours::lightgrey);
	portInput->setEditable(true);
	portInput->addListener(this);
	portInput->setBounds(15, 73, 70, 15);
	addAndMakeVisible(portInput.get());

	// Frequency
	sampleRateLabel = std::make_unique<Label>("FREQ (HZ)", "Freq (Hz)");
	sampleRateLabel->setFont(Font("Small Text", 10, Font::plain));
	sampleRateLabel->setBounds(10, 98, 85, 8);
	sampleRateLabel->setColour(Label::textColourId, Colours::darkgrey);
	addAndMakeVisible(sampleRateLabel.get());

	sampleRateInput = std::make_unique<Label>("Fs (Hz)", String((int)node->sample_rate));
	sampleRateInput->setFont(Font("Small Text", 10, Font::plain));
	sampleRateInput->setBounds(15, 108, 70, 15);
	sampleRateInput->setEditable(true);
	sampleRateInput->setColour(Label::backgroundColourId, Colours::lightgrey);
	sampleRateInput->addListener(this);
	addAndMakeVisible(sampleRateInput.get());

	// Scale
	scaleLabel = std::make_unique<Label>("Scale", "Scale");
	scaleLabel->setFont(Font("Small Text", 10, Font::plain));
	scaleLabel->setBounds(95, 63, 85, 8);
	scaleLabel->setColour(Label::textColourId, Colours::darkgrey);
	addAndMakeVisible(scaleLabel.get());

	scaleInput = std::make_unique<Label>("Scale", String(node->data_scale));
	scaleInput->setFont(Font("Small Text", 10, Font::plain));
	scaleInput->setBounds(100, 73, 70, 15);
	scaleInput->setEditable(true);
	scaleInput->setColour(Label::backgroundColourId, Colours::lightgrey);
	scaleInput->addListener(this);
	addAndMakeVisible(scaleInput.get());

	// Offset
	offsetLabel = std::make_unique<Label>("Offset", "Offset");
	offsetLabel->setFont(Font("Small Text", 10, Font::plain));
	offsetLabel->setBounds(95, 98, 85, 8);
	offsetLabel->setColour(Label::textColourId, Colours::darkgrey);
	addAndMakeVisible(offsetLabel.get());

	offsetInput = std::make_unique<Label>("Offset", String(node->data_offset));
	offsetInput->setFont(Font("Small Text", 10, Font::plain));
	offsetInput->setBounds(100, 108, 70, 15);
	offsetInput->setEditable(true);
	offsetInput->setColour(Label::backgroundColourId, Colours::lightgrey);
	offsetInput->addListener(this);
	addAndMakeVisible(offsetInput.get());
}

void EphysSocketEditor::labelTextChanged(Label* label)
{
	if (label == sampleRateInput.get())
	{
		float sampleRate = sampleRateInput->getText().getFloatValue();

		if (sampleRate > node->MIN_SAMPLE_RATE && sampleRate < node->MAX_SAMPLE_RATE)
		{
			node->sample_rate = sampleRate;
			CoreServices::updateSignalChain(this);
		}
		else {
			sampleRateInput->setText(String(node->sample_rate), dontSendNotification);
		}
	}
	else if (label == portInput.get())
	{
		int port = portInput->getText().getIntValue();

		if (port > node->MIN_PORT && port < node->MAX_PORT)
		{
			node->port = port;
		}
		else {
			portInput->setText(String(node->port), dontSendNotification);
		}
	}
	else if (label == scaleInput.get())
	{
		float scale = scaleInput->getText().getFloatValue();

		if (scale > node->MIN_DATA_SCALE && scale < node->MAX_DATA_SCALE)
		{
			node->data_scale = scale;
			CoreServices::updateSignalChain(this);
		}
		else {
			scaleInput->setText(String(node->data_scale), dontSendNotification);
		}
	}
	else if (label == offsetInput.get())
	{
		int offset = offsetInput->getText().getIntValue();

		if (offset >= node->MIN_DATA_OFFSET && offset < node->MAX_DATA_OFFSET)
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
	disconnectButton->setEnabled(false);
	disconnectButton->setAlpha(0.2f);
}

void EphysSocketEditor::stopAcquisition()
{
	if (node->errorFlag())
	{
		node->disconnectSocket();
		enableInputs();
	}

	disconnectButton->setEnabled(true);
	disconnectButton->setAlpha(1.0f);
}

void EphysSocketEditor::disableInputs()
{
	connectButton->setVisible(false);
	disconnectButton->setVisible(true);

	portInput->setEnabled(false);
	sampleRateInput->setEnabled(false);
	scaleInput->setEnabled(false);
	offsetInput->setEnabled(false);
}

void EphysSocketEditor::enableInputs()
{
	connectButton->setVisible(true);
	disconnectButton->setVisible(false);

	portInput->setEnabled(true);
	sampleRateInput->setEnabled(true);
	scaleInput->setEnabled(true);
	offsetInput->setEnabled(true);
}

void EphysSocketEditor::buttonClicked(Button* button)
{
	if (button == connectButton.get() && !acquisitionIsActive)
	{
		node->port = portInput->getText().getIntValue();

		if (node->connectSocket())
		{
			disableInputs();
		}

		CoreServices::updateSignalChain(this);
	}
	else if (button == disconnectButton.get() && !acquisitionIsActive)
	{
		node->disconnectSocket();
		enableInputs();
	}
}

void EphysSocketEditor::saveCustomParametersToXml(XmlElement* xmlNode)
{
	XmlElement* parameters = xmlNode->createNewChildElement("PARAMETERS");

	parameters->setAttribute("port", portInput->getText());
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
			node->port = subNode->getIntAttribute("port", node->DEFAULT_PORT);

			sampleRateInput->setText(subNode->getStringAttribute("fs", ""), dontSendNotification);
			node->sample_rate = subNode->getDoubleAttribute("fs", node->DEFAULT_SAMPLE_RATE);

			scaleInput->setText(subNode->getStringAttribute("scale", ""), dontSendNotification);
			node->data_scale = subNode->getDoubleAttribute("scale", node->DEFAULT_DATA_SCALE);

			offsetInput->setText(subNode->getStringAttribute("offset", ""), dontSendNotification);
			node->data_offset = subNode->getIntAttribute("offset", node->DEFAULT_DATA_OFFSET);
		}
	}
}

