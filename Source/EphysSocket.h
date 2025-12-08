#ifndef __EPHYSSOCKETH__
#define __EPHYSSOCKETH__

#include <DataThreadHeaders.h>

#include "EphysSocketHeader.h"
#include "SocketThread.h"

namespace EphysSocketNode
{
class EphysSocket : public DataThread
{
public:
    /** Connection states */
    static const constexpr char const* CONNECTION_STATE_CONNECTED { "CONNECTED" };
    static const constexpr char const* CONNECTION_STATE_DISCONNECTED { "DISCONNECTED" };

    /** Default parameters */
    static constexpr int DEFAULT_PORT { 9001 };
    static constexpr float DEFAULT_SAMPLE_RATE { 30000.0f };
    static constexpr float DEFAULT_DATA_SCALE { 1.0f }; // 0.195f for Intan devices
    static constexpr float DEFAULT_DATA_OFFSET { 0.0f }; // 32768.0f for Intan devices
    static constexpr char const* DEFAULT_CONNECTION_STATE { CONNECTION_STATE_DISCONNECTED };

    /** Parameter limits */
    static constexpr float MIN_DATA_SCALE { 0.0f };
    static constexpr float MAX_DATA_SCALE { 9999.9f };
    static constexpr float MIN_DATA_OFFSET { 0 };
    static constexpr float MAX_DATA_OFFSET { 65536 };
    static constexpr float MIN_PORT { 1023 };
    static constexpr float MAX_PORT { 65535 };
    static constexpr float MIN_SAMPLE_RATE { 0 };
    static constexpr float MAX_SAMPLE_RATE { 50000.0f };

    /** Constructor */
    EphysSocket (SourceNode* sn);

    /** Destructor */
    ~EphysSocket();

    /** Creates custom editor */
    std::unique_ptr<GenericEditor> createEditor (SourceNode* sn);

    /** Create the DataThread object*/
    static DataThread* createDataThread (SourceNode* sn);

    /** Registers the parameters for the DataThread */
    void registerParameters() override;

    /** Returns true if socket is connected */
    bool foundInputSource() override;

    /** Sets info about available channels */
    void updateSettings (OwnedArray<ContinuousChannel>* continuousChannels,
                         OwnedArray<EventChannel>* eventChannels,
                         OwnedArray<SpikeChannel>* spikeChannels,
                         OwnedArray<DataStream>* sourceStreams,
                         OwnedArray<DeviceInfo>* devices,
                         OwnedArray<ConfigurationObject>* configurationObjects);

    /** Handles parameter value changes */
    void parameterValueChanged (Parameter* parameter) override;

    /** Resizes buffers when input parameters are changed*/
    void resizeBuffers();

    /** Disconnects the socket */
    void disconnectSocket();

    /** Attempts to connect to the socket */
    bool connectSocket (bool printOutput = true);

    /** Returns if any errors were thrown during acquisition, such as invalid headers or unable to read from socket */
    bool errorFlag();

    /** Network stream parameters (must match features of incoming data) */
    int port;
    float sample_rate;
    float data_scale;
    float data_offset;

private:
    const int bufferSizeInSeconds = 10;

    /** Receives data from network and pushes it to the DataBuffer */
    bool updateBuffer() override;

    bool isReady() override;

    /** Resets variables and starts thread*/
    bool startAcquisition() override;

    /** Stops thread */
    bool stopAcquisition() override;

    /** Handles incoming HTTP messages */
    String handleConfigMessage (const String& msg) override;

    /** Template function to convert data */
    template <typename T>
    void convertData (std::vector<std::byte> buffer);

    /** Sample index counter */
    int64 total_samples;

    /** Local event state variable */
    uint64 eventState;

    SocketThread socket;

    std::vector<float> convbuf;

    Array<int64> sampleNumbers;
    Array<double> timestamps;
    Array<uint64> ttlEventWords;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EphysSocket);
};
} // namespace EphysSocketNode
#endif