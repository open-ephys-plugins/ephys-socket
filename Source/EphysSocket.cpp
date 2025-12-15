#ifdef _WIN32
#include <Windows.h>
#endif

#include "EphysSocket.h"
#include "EphysSocketEditor.h"

using namespace EphysSocketNode;

DataThread* EphysSocket::createDataThread (SourceNode* sn)
{
    return new EphysSocket (sn);
}

EphysSocket::EphysSocket (SourceNode* sn) : DataThread (sn), socket ("socket_thread", this)
{
    port = DEFAULT_PORT;
    sample_rate = DEFAULT_SAMPLE_RATE;

    total_samples = 0;
    eventState = 0;

    data_scale = DEFAULT_DATA_SCALE;
    data_offset = DEFAULT_DATA_OFFSET;

    sourceBuffers.add (new DataBuffer (socket.num_channels, sample_rate * bufferSizeInSeconds)); // start with 2 channels and automatically resize
}

std::unique_ptr<GenericEditor> EphysSocket::createEditor (SourceNode* sn)
{
    std::unique_ptr<EphysSocketEditor> editor = std::make_unique<EphysSocketEditor> (sn, this);
    return editor;
}

EphysSocket::~EphysSocket()
{
}

void EphysSocket::registerParameters()
{
    addIntParameter (Parameter::PROCESSOR_SCOPE, "port", "Port", "Port number to connect to", DEFAULT_PORT, MIN_PORT, MAX_PORT);
    addFloatParameter (Parameter::PROCESSOR_SCOPE, "sample_rate", "Sample Rate", "Sample rate of incoming data", "Hz", DEFAULT_SAMPLE_RATE, MIN_SAMPLE_RATE, MAX_SAMPLE_RATE, 1.0f);
    addFloatParameter (Parameter::PROCESSOR_SCOPE, "data_scale", "Scale", "Scale of incoming data", "", DEFAULT_DATA_SCALE, MIN_DATA_SCALE, MAX_DATA_SCALE, 0.1f);
    addFloatParameter (Parameter::PROCESSOR_SCOPE, "data_offset", "Offset", "Offset of incoming data", "", DEFAULT_DATA_OFFSET, MIN_DATA_OFFSET, MAX_DATA_OFFSET, 1.0f);
}

void EphysSocket::disconnectSocket()
{
    socket.signalThreadShouldExit();
    socket.waitForThreadToExit (1000);
    socket.disconnectSocket();

    getParameter ("port")->setEnabled (true);
    getParameter ("sample_rate")->setEnabled (true);
    getParameter ("data_scale")->setEnabled (true);
    getParameter ("data_offset")->setEnabled (true);

    if (sn->getEditor() != nullptr) // check if headless
        static_cast<EphysSocketEditor*> (sn->getEditor())->disconnected();
}

bool EphysSocket::connectSocket (bool printOutput)
{
    if (socket.connectSocket (port, printOutput))
    {
        getParameter ("port")->setEnabled (false);
        getParameter ("sample_rate")->setEnabled (false);
        getParameter ("data_scale")->setEnabled (false);
        getParameter ("data_offset")->setEnabled (false);

        if (sn->getEditor() != nullptr) // check if headless
            static_cast<EphysSocketEditor*> (sn->getEditor())->connected();

        return true;
    }

    return false;
}

bool EphysSocket::errorFlag()
{
    return socket.isError();
}

void EphysSocket::resizeBuffers()
{
    sourceBuffers[0]->resize (socket.num_channels, sample_rate * bufferSizeInSeconds);
    convbuf.resize (socket.num_channels * socket.num_samp);
    sampleNumbers.resize (socket.num_samp);
    timestamps.clear();
    timestamps.insertMultiple (0, 0.0, socket.num_samp);
    ttlEventWords.resize (socket.num_samp);
}

void EphysSocket::updateSettings (OwnedArray<ContinuousChannel>* continuousChannels,
                                  OwnedArray<EventChannel>* eventChannels,
                                  OwnedArray<SpikeChannel>* spikeChannels,
                                  OwnedArray<DataStream>* sourceStreams,
                                  OwnedArray<DeviceInfo>* devices,
                                  OwnedArray<ConfigurationObject>* configurationObjects)
{
    continuousChannels->clear();
    eventChannels->clear();
    devices->clear();
    spikeChannels->clear();
    configurationObjects->clear();
    sourceStreams->clear();

    DataStream::Settings settings {
        "EphysSocketStream",
        "Data acquired via network stream",
        "ephyssocket.data",

        sample_rate

    };

    sourceStreams->add (new DataStream (settings));
    sourceBuffers[0]->resize (socket.num_channels, sample_rate * bufferSizeInSeconds);

    for (int ch = 0; ch < socket.num_channels; ch++)
    {
        ContinuousChannel::Settings settings {
            ContinuousChannel::Type::ELECTRODE,
            "CH" + String (ch + 1),
            "Channel acquired via network stream",
            "ephyssocket.continuous",

            data_scale,

            sourceStreams->getFirst()
        };

        continuousChannels->add (new ContinuousChannel (settings));
    }

    EventChannel::Settings eventSettings {
        EventChannel::Type::TTL,
        "Events",
        "Events acquired via network stream",
        "ephyssocket.events",
        sourceStreams->getFirst(),
        1
    };

    eventChannels->add (new EventChannel (eventSettings));
}

bool EphysSocket::foundInputSource()
{
    return socket.isConnected();
}

bool EphysSocket::isReady()
{
    return socket.isConnected();
}

void EphysSocket::parameterValueChanged (Parameter* parameter)
{
    if (parameter->getName() == "port")
    {
        port = (int) parameter->getValue();
    }
    else if (parameter->getName() == "sample_rate")
    {
        sample_rate = (float) parameter->getValue();
        CoreServices::updateSignalChain (sn); // Update the signal chain to reflect the new sample rate
    }
    else if (parameter->getName() == "data_scale")
    {
        data_scale = (float) parameter->getValue();
        CoreServices::updateSignalChain (sn); // Update the signal chain to reflect the new data scale
    }
    else if (parameter->getName() == "data_offset")
    {
        data_offset = (float) parameter->getValue();
    }
}

bool EphysSocket::startAcquisition()
{
    resizeBuffers();

    total_samples = 0;
    eventState = 0;

    socket.startAcquisition();

    socket.startThread();
    startThread();

    return true;
}

bool EphysSocket::stopAcquisition()
{
    if (isThreadRunning())
    {
        signalThreadShouldExit();
    }

    socket.stopAcquisition();

    sourceBuffers[0]->clear();
    return true;
}

template <typename T>
void EphysSocket::convertData (std::vector<std::byte> buffer)
{
    T* buf = (T*) (buffer.data() + HEADER_SIZE);

    for (int i = 0; i < socket.num_samp * socket.num_channels; i++)
    {
        convbuf[i] = data_scale * ((float) (buf[i]) - data_offset);
    }
}

bool EphysSocket::updateBuffer()
{
    if (socket.isError())
    {
        return false;
    }

    if (socket.data.isEmpty())
    {
        return true;
    }

    std::vector<std::byte> data = socket.data.removeAndReturn (0);

    if (socket.depth == U8)
    {
        convertData<uint8_t> (data);
    }
    else if (socket.depth == S8)
    {
        convertData<int8_t> (data);
    }
    else if (socket.depth == U16)
    {
        convertData<uint16_t> (data);
    }
    else if (socket.depth == S16)
    {
        convertData<int16_t> (data);
    }
    else if (socket.depth == S32)
    {
        convertData<int32_t> (data);
    }
    else if (socket.depth == F32)
    {
        convertData<float_t> (data);
    }
    else if (socket.depth == F64)
    {
        convertData<double_t> (data);
    }

    for (int i = 0; i < socket.num_samp; i++)
    {
        sampleNumbers.set (i, total_samples++);
        ttlEventWords.set (i, eventState);
    }

    sourceBuffers[0]->addToBuffer (convbuf.data(),
                                   sampleNumbers.getRawDataPointer(),
                                   timestamps.getRawDataPointer(),
                                   ttlEventWords.getRawDataPointer(),
                                   socket.num_samp);

    return true;
}

String EphysSocket::handleConfigMessage (const String& msg)
{
    // Available commands:
    // ES INFO                      - Returns info on current variables that can be modified over HTTP
    // ES SCALE <data_scale>        - Updates the data scale to data_scale
    // ES OFFSET <data_offset>      - Updates the offset to data_offset
    // ES PORT <port>               - Updates the port number that EphysSocket connects to
    // ES FREQUENCY <sample_rate>   - Updates the sampling rate
    // ES CONNECTION_STATE          - Returns the connection state (CONNECTED/DISCONNECTED)
    // ES CONNECT                   - Connect the socket
    // ES DISCCONNECT               - Disconnect the socket

    if (CoreServices::getAcquisitionStatus())
    {
        return "Ephys Socket plugin cannot update settings while acquisition is active.";
    }

    StringArray parts = StringArray::fromTokens (msg, " ", "");

    if (parts.size() > 0)
    {
        if (parts[0].equalsIgnoreCase ("ES"))
        {
            if (parts.size() == 3)
            {
                if (socket.isConnected())
                {
                    return "Ephys Socket plugin cannot update settings while connected to an active socket.";
                }

                if (parts[1].equalsIgnoreCase ("SCALE"))
                {
                    float scale = parts[2].getFloatValue();

                    if (scale > MIN_DATA_SCALE && scale < MAX_DATA_SCALE)
                    {
                        getParameter ("data_scale")->setNextValue (scale);
                        LOGC ("Scale updated to: ", scale);
                        return "SUCCESS";
                    }

                    return "Invalid scale requested. Scale can be set between '" + String (MIN_DATA_SCALE) + "' and '" + String (MAX_DATA_SCALE) + "'";
                }
                else if (parts[1].equalsIgnoreCase ("OFFSET"))
                {
                    float offset = parts[2].getFloatValue();

                    if (offset >= MIN_DATA_OFFSET && offset < MAX_DATA_OFFSET)
                    {
                        getParameter ("data_offset")->setNextValue (offset);
                        LOGC ("Offset updated to: ", offset);
                        return "SUCCESS";
                    }

                    return "Invalid offset requested. Offset can be set between '" + String (MIN_DATA_OFFSET) + "' and '" + String (MAX_DATA_OFFSET) + "'";
                }
                else if (parts[1].equalsIgnoreCase ("PORT"))
                {
                    float _port = parts[2].getFloatValue();

                    if (_port > MIN_PORT && _port < MAX_PORT)
                    {
                        getParameter ("port")->setNextValue (_port);
                        LOGC ("Port updated to: ", _port);
                        return "SUCCESS";
                    }

                    return "Invalid port requested. Port can be set between '" + String (MIN_PORT) + "' and '" + String (MAX_PORT) + "'";
                }
                else if (parts[1].equalsIgnoreCase ("FREQUENCY"))
                {
                    float frequency = parts[2].getFloatValue();

                    if (frequency > MIN_SAMPLE_RATE && frequency < MAX_SAMPLE_RATE)
                    {
                        getParameter ("sample_rate")->setNextValue (frequency);
                        LOGC ("Frequency updated to: ", sample_rate);
                        return "SUCCESS";
                    }

                    return "Invalid frequency requested. Frequency can be set between '" + String (MIN_SAMPLE_RATE) + "' and '" + String (MAX_SAMPLE_RATE) + "'";
                }
                else
                {
                    return "ES command " + parts[1] + "not recognized.";
                }
            }
            else if (parts.size() == 2)
            {
                if (parts[1].equalsIgnoreCase ("INFO"))
                {
                    return "Port = " + String (port) + ". Sample rate = " + String (sample_rate) + "Scale = " + String (data_scale) + ". Offset = " + String (data_offset) + ".";
                }
                else if (parts[1].equalsIgnoreCase ("CONNECTION_STATUS"))
                {
                    return socket.isConnected() ? CONNECTION_STATE_CONNECTED : CONNECTION_STATE_DISCONNECTED;
                }
                else if (parts[1].equalsIgnoreCase ("CONNECT"))
                {
                    LOGC ("Request socket connect");
                    const auto connected = connectSocket();
                    LOGC (connected ? "Connection success" : "Connection failed");
                    return connected ? CONNECTION_STATE_CONNECTED : CONNECTION_STATE_DISCONNECTED;
                }
                else if (parts[1].equalsIgnoreCase ("DISCONNECT"))
                {
                    disconnectSocket();
                    LOGC ("Socket disconnected");
                    return CONNECTION_STATE_DISCONNECTED;
                }
                else
                {
                    return "ES command " + parts[1] + "not recognized.";
                }
            }

            return "Unknown number of inputs given.";
        }

        return "Command not recognized.";
    }

    return "Empty message";
}
