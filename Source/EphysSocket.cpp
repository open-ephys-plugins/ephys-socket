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

EphysSocket::EphysSocket(SourceNode* sn) : DataThread(sn),
    port(DEFAULT_PORT),
    num_channels(DEFAULT_NUM_CHANNELS),
    num_samp(DEFAULT_NUM_SAMPLES),
    data_offset(DEFAULT_DATA_OFFSET),
    data_scale(DEFAULT_DATA_SCALE),
    sample_rate(DEFAULT_SAMPLE_RATE)
{
    total_samples = DEFAULT_TOTAL_SAMPLES;
    eventState = DEFAULT_EVENT_STATE;

    full_flag = false;
    stop_flag = false;
    error_flag = false;
    buffer_flag = false;

    sourceBuffers.add(new DataBuffer(num_channels, 10000)); // start with 2 channels and automatically resize
    recvbuf0.reserve(num_channels * num_samp);
    recvbuf1.reserve(num_channels * num_samp);
    convbuf.reserve(num_channels * num_samp);

    depth_strings = { "UINT8", "INT8", "UINT16", "INT16", "INT32", "FLOAT32", "FLOAT64" };
}

std::unique_ptr<GenericEditor> EphysSocket::createEditor(SourceNode* sn)
{
    std::unique_ptr<EphysSocketEditor> editor = std::make_unique<EphysSocketEditor>(sn, this);

    return editor;
}

EphysSocket::~EphysSocket()
{
}

void  EphysSocket::tryToConnect()
{
	if (socket != nullptr)
	{
		socket->shutdown();
        socket.reset();
	}
    
    socket = std::make_unique<DatagramSocket>();
    
    bool bound = socket->bindToPort(port);

    if (bound)
    {
        LOGC("EphysSocket bound to port ", port);
        connected = (socket->waitUntilReady(true, 500) == 1);
    }
    else {
        LOGC("EphysSocket could not bind socket to port ", port);
    }

    if (connected)
    {
        LOGC("EphysSocket connected.");

    }
    else {
        LOGC("EphysSocket failed to connect");
    }
}

void EphysSocket::resizeBuffers()
{
    sourceBuffers[0]->resize(num_channels, 10000);
    recvbuf0.reserve(num_channels * num_samp);
    recvbuf1.reserve(num_channels * num_samp);
    convbuf.reserve(num_channels * num_samp);
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

    if (socket == nullptr)
    {
        tryToConnect(); // connect after settings have been loaded
    }

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

void EphysSocket::runBufferThread()
{
    const int header_size = sizeof(int) * 2;
    const int total_samples = num_channels * num_samp;
    const int total_packet_size = total_samples * sizeof(uint16_t);
    const int packet_ratio = ((total_packet_size + header_size) / MAX_PACKET_SIZE) + 1;
    const int packet_size = (total_packet_size / packet_ratio) + header_size;

    std::vector<uint16_t> read_buffer;
    read_buffer.resize(total_samples + 4);

    int rc = 0, offset = 0, bytes_sent = 0;

    while (!stop_flag)
    {
        rc = socket->read(read_buffer.data(), packet_size, true);

        if (rc == -1)
        {
            CoreServices::sendStatusMessage("Ephys Socket: Data shape mismatch");
            error_flag = true;
            return;
        }

        offset = read_buffer.at(1) << 16 | read_buffer.at(0);
        bytes_sent = read_buffer.at(3) << 16 | read_buffer.at(2);

        if (!buffer_flag) recvbuf0.insert(recvbuf0.begin() + (offset / sizeof(uint16_t)), read_buffer.begin() + 4, read_buffer.end()); // This might error if packets are lost
        else              recvbuf1.insert(recvbuf1.begin() + (offset / sizeof(uint16_t)), read_buffer.begin() + 4, read_buffer.end());
        
        if (packet_ratio == 1 || offset + bytes_sent == total_packet_size)
        {
            full_flag = true;
            buffer_flag = !buffer_flag;
        }
    }

    return;
}

bool EphysSocket::updateBuffer()
{
    if (full_flag)
    {
        convbuf.clear();

        int k = 0;
        for (int i = 0; i < num_samp; i++) {
            for (int j = 0; j < num_channels; j++) {
                if (buffer_flag)
                    convbuf.insert(convbuf.begin() + k++, data_scale * (float)(recvbuf0.at(j * num_samp + i) - data_offset));
                else
                    convbuf.insert(convbuf.begin() + k++, data_scale * (float)(recvbuf1.at(j * num_samp + i) - data_offset));
            }
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

        if (buffer_flag)  recvbuf0.clear();
        else              recvbuf1.clear();
    }

    if (error_flag)
        return false;
    
    else
        return true;
}
