#ifndef __EPHYSSOCKETH__
#define __EPHYSSOCKETH__

#include <DataThreadHeaders.h>

#include "EphysSocketHeader.h"

namespace EphysSocketNode
{

    class EphysSocket : public DataThread
    {

    public:

        /** Default parameters */
        const int DEFAULT_PORT = 9001;
        const float DEFAULT_SAMPLE_RATE = 30000.0f;
        const float DEFAULT_DATA_SCALE = 0.195f;
        const float DEFAULT_DATA_OFFSET = 32768.0f;

        /** Parameter limits */
        const float MIN_DATA_SCALE = 0.0f;
        const float MAX_DATA_SCALE = 9999.9f;
        const float MIN_DATA_OFFSET = 0;
        const float MAX_DATA_OFFSET = 65536;
        const float MIN_PORT = 1023;
        const float MAX_PORT = 65535;
        const float MIN_SAMPLE_RATE = 0;
        const float MAX_SAMPLE_RATE = 50000.0f;

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

        /** Disconnects the socket */
        void disconnectSocket();

        /** Attempts to connect to the socket */
        bool tryToConnect();

        /** Returns if any errors were thrown during acquisition, such as invalid headers or unable to read from socket */
        bool errorFlag();

        /** Runs the Buffer Thread to acquire data */
        void runBufferThread();

        /** Network stream parameters (must match features of incoming data) */
        int port;
        float sample_rate;
        int num_samp;
        int num_channels;
        Depth depth;
        float data_scale;
        float data_offset;

    private:

        /** Default socket parameters */
        const int DEFAULT_NUM_SAMPLES = 256;
        const int DEFAULT_NUM_CHANNELS = 64;
        const Depth DEFAULT_DEPTH = U16;
        const int DEFAULT_ELEMENT_SIZE = 2;
        const int DEFAULT_NUM_BYTES = 32678; // NB: 256 * 64 * 2

        /** Default parameters */
        const int DEFAULT_TOTAL_SAMPLES = 0;
        const int DEFAULT_EVENT_STATE = 0;

        /** Variables that are part of the incoming header */
        int num_bytes;
        int element_size;

        /** Receives data from network and pushes it to the DataBuffer */
        bool updateBuffer() override;

        /** Resets variables and starts thread*/
        bool startAcquisition() override;

        /** Stops thread */
        bool stopAcquisition()  override;

        /** Handles incoming HTTP messages */
        String handleConfigMessage(String msg) override;

        /** Compares a newly parsed header to existing variables */
        bool compareHeaders(EphysSocketHeader header) const;

        /** Template function to convert data */
        template <typename T>
        void convertData();

        /** Sample index counter */
        int64 total_samples;
        
        /** Local event state variable */
        uint64 eventState;

        /** True if socket is connected */
        bool connected = false;

        /** TCP Socket object */
        std::unique_ptr<StreamingSocket> socket;

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