#ifndef __EPHYSSOCKETH__
#define __EPHYSSOCKETH__

#include <DataThreadHeaders.h>

const int DEFAULT_PORT = 9001;
const float DEFAULT_SAMPLE_RATE = 30000.0f;
const float DEFAULT_DATA_SCALE = 0.195f;
const uint16_t DEFAULT_DATA_OFFSET = 32768;
const int DEFAULT_NUM_SAMPLES = 256;
const int DEFAULT_NUM_CHANNELS = 64;

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

        /** Attempts to reconnect to the socket */
        void tryToConnect();

        /** Network stream parameters (must match features of incoming data) */
        int port;
        float sample_rate;
        float data_scale;
        uint16_t data_offset;
        bool transpose = true;
        int num_samp;
        int num_channels;

    private:

        /** Receives data from network and pushes it to the DataBuffer */
        bool updateBuffer() override;

        /** Resets variables and starts thread*/
        bool startAcquisition() override;

        /** Stops thread */
        bool stopAcquisition()  override;

        /** Sample index counter */
        int64 total_samples;
        
        /** Local event state variable */
        uint64 eventState;

        /** True if socket is connected */
        bool connected = false;

        /** UPD socket object */
        std::unique_ptr<DatagramSocket> socket;

        /** Internal buffers */
        uint16_t *recvbuf;
        float *convbuf;

        Array<int64> sampleNumbers;
        Array<double> timestamps;
        Array<uint64> ttlEventWords;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EphysSocket);
    };
}
#endif