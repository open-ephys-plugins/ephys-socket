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
    num_channels = DEFAULT_NUM_CHANNELS;
    num_samp = DEFAULT_NUM_SAMPLES;
    data_offset = DEFAULT_DATA_OFFSET;
    data_scale = DEFAULT_DATA_SCALE;
    sample_rate = DEFAULT_SAMPLE_RATE;
    total_samples = DEFAULT_TOTAL_SAMPLES;
    eventState = DEFAULT_EVENT_STATE;

    depth = U16;
    element_size = 2;
    num_bytes = 32678;

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

        std::vector<std::byte> header_bytes(HEADER_SIZE);

        int rc = socket->read(header_bytes.data(), HEADER_SIZE, true);

        Header tmp_header = parseHeader(header_bytes);

        num_bytes = tmp_header.num_bytes;
        depth = tmp_header.depth;
        element_size = tmp_header.element_size;
        num_samp = tmp_header.num_samp;
        num_channels = tmp_header.num_channels;
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
    if (header.num_bytes != num_bytes ||
        header.depth != depth ||
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
    const int packet_ratio = ((matrix_size + HEADER_SIZE) / MAX_PACKET_SIZE) + 1;
    const int packet_size = (matrix_size / packet_ratio) + HEADER_SIZE;

    std::vector<std::byte> read_buffer;
    read_buffer.resize(packet_size);

    int rc;
    Header header;

    while (!stop_flag)
    {
        rc = socket->read(read_buffer.data(), packet_size, true);

        if (rc == -1)
        {
            CoreServices::sendStatusMessage("Ephys Socket: Data shape mismatch");
            error_flag = true;
            return;
        }

        header = Header(read_buffer);

        if (!compareHeaders(header))
        {
            CoreServices::sendStatusMessage("Ephys Socket: Header mismatch");
            error_flag = true;
            return;
        }

        if (!buffer_flag) {
            std::copy(read_buffer.begin() + HEADER_SIZE, read_buffer.end(), recvbuf0.begin() + header.offset);
        }
        else {
            std::copy(read_buffer.begin() + HEADER_SIZE, read_buffer.end(), recvbuf1.begin() + header.offset);
        }
        
        if (packet_ratio == 1 || header.offset + header.num_bytes == matrix_size)
        {
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
