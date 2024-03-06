#ifndef __EPHYSSOCKETH__
#define __EPHYSSOCKETH__

#include <DataThreadHeaders.h>

#include "Header.h"

namespace EphysSocketNode
{

    class EphysSocket : public DataThread
    {

    public:

        /** Constructor */
        EphysSocket(SourceNode* sn);
        
        /** Destructor */
        ~EphysSocket();

        /** Creates custom editor */
        std::unique_ptr<GenericEditor> createEditor(SourceNode* sn);

        /** Create the DataThread object*/
        static DataThread* createDataThread(SourceNode* sn);

        /** Returns true if socket is connected */
        bool foundInputSource() override;

        /** Sets info about available channels */
        void updateSettings(OwnedArray<ContinuousChannel>* continuousChannels,
            OwnedArray<EventChannel>* eventChannels,
            OwnedArray<SpikeChannel>* spikeChannels,
            OwnedArray<DataStream>* sourceStreams,
            OwnedArray<DeviceInfo>* devices,
            OwnedArray<ConfigurationObject>* configurationObjects);

        /** Resizes buffers when input parameters are changed*/
        void resizeBuffers();

        /** Parse the header */
        Header parseHeader(std::vector<std::byte> header_bytes);

        /** Attempts to reconnect to the socket */
        void tryToConnect();

        /** Runs the Buffer Thread to acquire data */
        void runBufferThread();

        /** Network stream parameters (must match features of incoming data) */
        int port;
        float sample_rate;
        float data_scale;
        float data_offset;
        int num_samp;
        int num_channels;
        Depth depth;

    private:

        /** Constant parameters and enumeration definitions */
        const int DEFAULT_PORT = 9001;
        const float DEFAULT_SAMPLE_RATE = 30000.0f;
        const float DEFAULT_DATA_SCALE = 0.195f;
        const uint16_t DEFAULT_DATA_OFFSET = 32768;
        const int DEFAULT_NUM_SAMPLES = 256;
        const int DEFAULT_NUM_CHANNELS = 64;
        const int DEFAULT_TOTAL_SAMPLES = 0;
        const int DEFAULT_EVENT_STATE = 0;
        const int MAX_PACKET_SIZE = 65506;
        const int HEADER_SIZE = 22;

        /** Variables that are part of the incoming header */
        int num_bytes;
        int element_size;

        /** Receives data from network and pushes it to the DataBuffer */
        bool updateBuffer() override;

        /** Resets variables and starts thread*/
        bool startAcquisition() override;

        /** Stops thread */
        bool stopAcquisition()  override;

        /** Compares a newly parsed header to existing variables */
        bool compareHeaders(Header header) const;

        /** Compares a newly parsed header to existing variables */
        bool compareHeaders(std::vector<std::byte>& header_bytes) const;

        /** Template function to convert data */
        template <typename T>
        void convertData();

        /** Sample index counter */
        int64 total_samples;
        
        /** Local event state variable */
        uint64 eventState;

        /** True if socket is connected */
        bool connected = false;

        /** UPD socket object */
        std::unique_ptr<DatagramSocket> socket;

        /** Internal buffers */
        std::vector<std::byte> recvbuf0;
        std::vector<std::byte> recvbuf1;
        std::vector<float> convbuf;

        /** Atomic booleans for handling multithreading */
        std::atomic<bool> full_flag;
        std::atomic<bool> stop_flag;
        std::atomic<bool> error_flag;
        std::atomic<bool> buffer_flag;

        Array<int64> sampleNumbers;
        Array<double> timestamps;
        Array<uint64> ttlEventWords;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EphysSocket);
    };
}
#endif