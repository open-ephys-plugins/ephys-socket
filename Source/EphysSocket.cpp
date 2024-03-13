#ifdef _WIN32
#include <Windows.h>
#endif

#include "EphysSocket.h"
#include "EphysSocketEditor.h"

using namespace EphysSocketNode;

DataThread* EphysSocket::createDataThread(SourceNode *sn)
{
    return new EphysSocket(sn);
}

EphysSocket::EphysSocket(SourceNode* sn) : DataThread(sn)
{
    port = DEFAULT_PORT;
    sample_rate = DEFAULT_SAMPLE_RATE;

    total_samples = DEFAULT_TOTAL_SAMPLES;
    eventState = DEFAULT_EVENT_STATE;

    num_channels = DEFAULT_NUM_CHANNELS;
    num_samp = DEFAULT_NUM_SAMPLES;
    depth = DEFAULT_DEPTH;
    element_size = DEFAULT_ELEMENT_SIZE;
    num_bytes = DEFAULT_NUM_BYTES;

    data_scale = DEFAULT_DATA_SCALE;
    data_offset = DEFAULT_DATA_OFFSET;

    full_flag = false;
    stop_flag = false;
    error_flag = false;
    buffer_flag = false;

    sourceBuffers.add(new DataBuffer(num_channels, 10000)); // start with 2 channels and automatically resize
}

std::unique_ptr<GenericEditor> EphysSocket::createEditor(SourceNode* sn)
{
    std::unique_ptr<EphysSocketEditor> editor = std::make_unique<EphysSocketEditor>(sn, this);

    return editor;
}

EphysSocket::~EphysSocket()
{
}

Header EphysSocket::parseHeader(std::vector<std::byte> header_bytes)
{
    return Header(header_bytes);
}

void EphysSocket::tryToConnect()
{
	if (socket != nullptr)
	{
        //socket->shutdown(); // UDP
        socket->close();
        socket.reset();
	}

    /* UDP
    socket = std::make_unique<DatagramSocket>();

    bool bound = socket->bindToPort(port);

    if (bound)
    {
        LOGC("EphysSocket bound to port ", port);
        connected = (socket->waitUntilReady(true, 500) == 1);
    }
    else {
        LOGC("EphysSocket could not bind socket to port ", port);
    }*/

    socket = std::make_unique<StreamingSocket>();
    connected = socket->connect("localhost", port, 500);

    if (connected)
    {
        LOGC("EphysSocket connected.");

        std::vector<std::byte> header_bytes(HEADER_SIZE);

        LOGD("Reading header...");
        int rc = socket->read(header_bytes.data(), HEADER_SIZE, true);

        Header tmp_header = parseHeader(header_bytes);
        LOGD("Header read and parsed correctly.");

        num_bytes = tmp_header.num_bytes;
        depth = tmp_header.depth;
        element_size = tmp_header.element_size;
        num_samp = tmp_header.num_samp;
        num_channels = tmp_header.num_channels;

        const int matrix_size = num_channels * num_samp * element_size;
        header_bytes.reserve(matrix_size);
        socket->read(header_bytes.data(), matrix_size, true); // NB: Realign stream to the beginning of a packet
    }
    else {
        LOGC("EphysSocket failed to connect");
    }
}

void EphysSocket::resizeBuffers()
{
    sourceBuffers[0]->resize(num_channels, 10000);
    recvbuf0.resize(num_channels * num_samp * element_size);
    recvbuf1.resize(num_channels * num_samp * element_size);
    convbuf.resize(num_channels * num_samp);
    sampleNumbers.resize(num_samp);
    timestamps.clear();
    timestamps.insertMultiple(0, 0.0, num_samp);
    ttlEventWords.resize(num_samp);
}

void EphysSocket::updateSettings(OwnedArray<ContinuousChannel>* continuousChannels,
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

    DataStream::Settings settings
    {
        "EphysSocketStream",
        "Data acquired via network stream",
        "ephyssocket.data",

        sample_rate

    };

    sourceStreams->add(new DataStream(settings));
    sourceBuffers[0]->resize(num_channels, 10000);

    for (int ch = 0; ch < num_channels; ch++)
    {

        ContinuousChannel::Settings settings{
            ContinuousChannel::Type::ELECTRODE,
            "CH" + String(ch + 1),
            "Channel acquired via network stream",
            "ephyssocket.continuous",

            data_scale,

            sourceStreams->getFirst()
        };

        continuousChannels->add(new ContinuousChannel(settings));
    }

    EventChannel::Settings eventSettings{
           EventChannel::Type::TTL,
           "Events",
           "Events acquired via network stream",
           "ephyssocket.events",
           sourceStreams->getFirst(),
           1
    };

    eventChannels->add(new EventChannel(eventSettings));
}

bool EphysSocket::foundInputSource()
{
    //LOGD("Checking foundInputSource(); connected = ", connected);
    return connected;
}

bool EphysSocket::startAcquisition()
{
    resizeBuffers();

    total_samples = 0;
    eventState = 0;

    error_flag = false;
    stop_flag = false;
    full_flag = false;
    buffer_flag = false;

    Thread::launch([this] {runBufferThread(); });
    startThread();

    return true;
}

bool EphysSocket::stopAcquisition()
{
    if (isThreadRunning())
    {
        signalThreadShouldExit();
    }

    stop_flag = true;

    waitForThreadToExit(500);

    sourceBuffers[0]->clear();
    return true;
}

bool EphysSocket::compareHeaders(Header header) const
{
    if (header.depth != depth ||
        header.element_size != element_size ||
        header.num_channels != num_channels ||
        header.num_samp != num_samp)
    {
        return false;
    }

    return true;
}

bool EphysSocket::compareHeaders(std::vector<std::byte>& header_bytes) const
{
    Header header = Header(header_bytes);
    return compareHeaders(header);
}

template <typename T>
void EphysSocket::convertData()
{
    T* buf;

    if (!buffer_flag) {
        buf = (T*)recvbuf0.data();
    }
    else {
        buf = (T*)recvbuf1.data();
    }

    int k = 0;
    for (int i = 0; i < num_samp; i++) {
        for (int j = 0; j < num_channels; j++) {
            convbuf[k++] = data_scale * (float)(buf[j * num_samp + i]) - data_offset;
        }
    }
}

void EphysSocket::runBufferThread()
{
    const int matrix_size = num_channels * num_samp * element_size;
    const int num_expected_packets = 1;
    //const int num_expected_packets = (matrix_size / MAX_PACKET_SIZE) + 1; // UDP
    //const int max_data_size = num_expected_packets == 1 ? matrix_size : MAX_PACKET_SIZE - HEADER_SIZE; // UDP

    std::vector<std::byte> read_buffer;
    read_buffer.resize(matrix_size + HEADER_SIZE);

    int rc;
    Header header;

    while (!stop_flag)
    {
        rc = socket->read(read_buffer.data(), matrix_size + HEADER_SIZE, true);

        if (rc == -1)
        {
            if (socket->getRawSocketHandle() == -1)
            {
                CoreServices::sendStatusMessage("Ephys Socket: Socket handle invalid.");
                LOGE("Ephys Socket: Socket handle is invalid, returns -1");
                error_flag = true;
                return;
            }

            CoreServices::sendStatusMessage("Ephys Socket: Data shape mismatch");
            LOGE("Ephys Socket: Socket read did not complete, returns -1");
            error_flag = true;
            return;
        }

        for (int i = 0; i < num_expected_packets; i++)
        {
            //header = Header(read_buffer, i * (max_data_size + HEADER_SIZE)); // UDP
            header = Header(read_buffer);

            if (!compareHeaders(header))
            {
                CoreServices::sendStatusMessage("Ephys Socket: Header mismatch");
                LOGE("Ephys Socket: Header values have changed since first connecting");
                error_flag = true;
                return;
            }

            if (i == 0 && header.offset != 0)
            {
                CoreServices::sendStatusMessage("Ephys Socket: Packets were dropped");
                LOGE("Ephys Socket: [UDP] First packet does not have offset of 0; data is misaligned");
                error_flag = true;
                return;
            }
        
            int current_offset = HEADER_SIZE * (i + 1) + header.offset; 

            if (!buffer_flag) {
                std::copy(read_buffer.begin() + current_offset, read_buffer.begin() + current_offset + header.num_bytes, recvbuf0.begin() + header.offset);
            }
            else {
                std::copy(read_buffer.begin() + current_offset, read_buffer.begin() + current_offset + header.num_bytes, recvbuf1.begin() + header.offset);
            }
        }
        
        if (depth == U8) {
            convertData<uint8_t>();
        }
        else if (depth == S8) {
            convertData<int8_t>();
        }
        else if (depth == U16) {
            convertData<uint16_t>();
        }
        else if (depth == S16) {
            convertData<int16_t>();
        }
        else if (depth == S32) {
            convertData<int32_t>();
        }
        else if (depth == F32) {
            convertData<float_t>();
        }
        else if (depth == F64) {
            convertData<double_t>();
        }

        full_flag = true;
        buffer_flag = !buffer_flag;
    }

    return;
}

bool EphysSocket::updateBuffer()
{
    if (full_flag)
    {
        for (int i = 0; i < num_samp; i++) {
            sampleNumbers.set(i, total_samples++);
            ttlEventWords.set(i, eventState);
        }

        sourceBuffers[0]->addToBuffer(convbuf.data(),
            sampleNumbers.getRawDataPointer(),
            timestamps.getRawDataPointer(),
            ttlEventWords.getRawDataPointer(),
            num_samp,
            1);

        full_flag = false;
    }

    if (error_flag)
        return false;
    
    else
        return true;
}

String EphysSocket::handleConfigMessage(String msg)
{
    // Available commands:
    // ES INFO - Returns info on current variables that can be modified over HTTP
    // ES SCALE <data_scale>        - Updates the data scale to data_scale
    // ES OFFSET <data_offset>      - Updates the offset to data_offset
    // ES PORT <port>               - Updates the port number that EphysSocket connects to
    // ES FREQUENCY <sample_rate>   - Updates the sampling rate

    if (CoreServices::getAcquisitionStatus()) {
        return "Ephys Socket plugin cannot update settings while acquisition is active.";
    }

    StringArray parts = StringArray::fromTokens(msg, " ", "");

    if (parts.size() > 0)
    {
        if (parts[0].equalsIgnoreCase("ES"))
        {
            if (parts.size() == 3)
            {
                if (parts[1].equalsIgnoreCase("SCALE"))
                {
                    float scale = parts[2].getFloatValue();

                    if (scale > MIN_DATA_SCALE && scale < MAX_DATA_SCALE)
                    {
                        data_scale = scale;
                        LOGC("Scale updated to: ", scale);
                        return "SUCCESS";
                    }

                    return "Invalid scale requested. Scale can be set between '" + String(MIN_DATA_SCALE) + "' and '" + String(MAX_DATA_SCALE) + "'";
                }
                else if (parts[1].equalsIgnoreCase("OFFSET"))
                {
                    float offset = parts[2].getFloatValue();

                    if (offset >= MIN_DATA_OFFSET && offset < MAX_DATA_OFFSET)
                    {
                        data_offset = offset;
                        LOGC("Offset updated to: ", offset);
                        return "SUCCESS";
                    }

                    return "Invalid offset requested. Offset can be set between '" + String(MIN_DATA_OFFSET) + "' and '" + String(MAX_DATA_OFFSET) + "'";
                }
                else if (parts[1].equalsIgnoreCase("PORT"))
                {
                    float _port = parts[2].getFloatValue();

                    if (_port > MIN_PORT && _port < MAX_PORT)
                    {
                        port = _port;
                        LOGC("Port updated to: ", _port);
                        return "SUCCESS";
                    }

                    return "Invalid port requested. Port can be set between '" + String(MIN_PORT) + "' and '" + String(MAX_PORT) + "'";
                }
                else if (parts[1].equalsIgnoreCase("FREQUENCY"))
                {
                    float frequency = parts[2].getFloatValue();

                    if (frequency > MIN_SAMPLE_RATE && frequency < MAX_SAMPLE_RATE)
                    {
                        sample_rate = frequency;
                        LOGC("Frequency updated to: ", sample_rate);
                        return "SUCCESS";
                    }

                    return "Invalid frequency requested. Frequency can be set between '" + String(MIN_SAMPLE_RATE) + "' and '" + String(MAX_SAMPLE_RATE) + "'";
                }
                else
                {
                    return "ES command " + parts[1] + "not recognized.";
                }
            }
            else if (parts.size() == 2)
            {
                if (parts[1].equalsIgnoreCase("INFO"))
                {
                    return "Port = " + String(port) + ". Sample rate = " + String(sample_rate) + 
                        "Scale = " + String(data_scale) + ". Offset = " + String(data_offset) + ".";
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
}
