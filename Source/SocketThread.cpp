#ifdef _WIN32
#include <Windows.h>
#endif

#include "SocketThread.h"

using namespace EphysSocketNode;

SocketThread::SocketThread(String name)
	: Thread(name), editor(NULL)
{
	lastPacketReceived = time(nullptr);

	socket = nullptr;

	previousPort = -1;

	depth = DEFAULT_DEPTH;
	element_size = DEFAULT_ELEMENT_SIZE;
	num_bytes = DEFAULT_NUM_BYTES;
	num_channels = DEFAULT_NUM_CHANNELS;
	num_samp = DEFAULT_NUM_SAMPLES;

	error_flag = false;
	connected = false;
	shouldReconnect = false;
	acquiring = false;
}

SocketThread::~SocketThread()
{
	stopThread(500);

	if (socket != nullptr)
	{
		socket->close();
		socket.reset();
	}
}

void SocketThread::setEditor(EphysSocketEditor* ed)
{
	editor = ed;
}

void SocketThread::startAcquisition()
{
	acquiring = true;
}

void SocketThread::stopAcquisition()
{
	acquiring = false;
}

bool SocketThread::connectSocket(int port, bool printOutput)
{
	if (port == -1)
	{
		if (previousPort != -1) {
			port = previousPort;
		}
		else {
			LOGE("Must give a valid port number for the first connection.");
			return false;
		}
	}

	std::lock_guard<std::mutex> lock(socketMutex);

	if (socket != nullptr && socket->isConnected())
	{
		LOGE("Attempting to connect to an already active socket.");
		return false;
	}

	error_flag = false;

	socket = std::make_unique<StreamingSocket>();
	connected = socket->connect("localhost", port, 100);

	if (connected)
	{
		std::vector<std::byte> header_bytes(HEADER_SIZE);

		LOGD("Reading header...");
		int rc;

		for (int i = 0; i < 5; i++) {
			rc = socket->read(header_bytes.data(), HEADER_SIZE, false);

			if (rc == HEADER_SIZE) break;
			else sleep(100);
		}

		if (rc != HEADER_SIZE)
		{
			if (printOutput)
			{
				LOGC("EphysSocket failed to connect; could not read header from stream.");
				CoreServices::sendStatusMessage("Ephys Socket: Could not read stream.");
			}

			socket->close();
			socket.reset();

			connected = false;

			return false;
		}

		EphysSocketHeader tmp_header = EphysSocketHeader(header_bytes);
		LOGD("Header read and parsed correctly.");

		num_bytes = tmp_header.num_bytes;
		element_size = tmp_header.element_size;
		depth = tmp_header.depth;
		num_samp = tmp_header.num_samp;
		num_channels = tmp_header.num_channels;

		const int matrix_size = num_channels * num_samp * element_size;
		header_bytes.reserve(matrix_size);
		socket->read(header_bytes.data(), matrix_size, true); // NB: Realign stream to the beginning of a packet

		lastPacketReceived = time(nullptr);

		previousPort = port;

		if (printOutput)
		{
			LOGC("EphysSocket connected.");
			CoreServices::sendStatusMessage("Ephys Socket: Socket connected.");
		}

		read_buffer.resize(num_channels * num_samp * element_size + HEADER_SIZE);

		shouldReconnect = false;

		bool result = startThread();

		return true;
	}
	else
	{
		if (printOutput)
		{
			LOGC("EphysSocket failed to connect");
			CoreServices::sendStatusMessage("Ephys Socket: Could not connect.");
		}

		return false;
	}
}

void SocketThread::disconnectSocket()
{
	std::lock_guard<std::mutex> lock(socketMutex);

	if (socket != nullptr)
	{
		LOGD("Disconnecting socket.");

		socket->close();
		socket.reset();

		CoreServices::sendStatusMessage("Ephys Socket: Disconnected.");
	}

	connected = false;
	shouldReconnect = false;
}

bool SocketThread::isConnected()
{
	return connected;
}

bool SocketThread::isError() const
{
	return error_flag;
}

bool SocketThread::compareHeaders(EphysSocketHeader header) const
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

void SocketThread::attemptToReconnect()
{
	auto previousHeader = EphysSocketHeader(num_bytes, depth, element_size, num_samp, num_channels);

	if (connectSocket(previousPort, false))
	{
		shouldReconnect = false;

		if (!compareHeaders(previousHeader))
		{
			disconnectSocket();
			LOGE("Mismatched header, disconnecting socket.");
			CoreServices::sendStatusMessage("Ephys Socket: Invalid header, disconnecting.");
			error_flag = true;
			return;
		}

		LOGD("Successfully reconnected the socket.");
		CoreServices::sendStatusMessage("Ephys Socket: Socket reconnected.");
		lastPacketReceived = time(nullptr);
	}
}

void SocketThread::run()
{
	while (!threadShouldExit())
	{
		if (connected)
		{
			if (error_flag) {
				disconnectSocket();
				const MessageManagerLock mmLock;
				editor->enableInputs();
				return;
			}

			std::lock_guard<std::mutex> lock(socketMutex);

			const int bytes_expected = num_channels * num_samp * element_size + HEADER_SIZE;

			int rc;
			EphysSocketHeader header;

			if (socket != nullptr && socket->isConnected()) {
				rc = socket->read(read_buffer.data(), bytes_expected, false);
			}
			else {
				rc = 0; // NB: This will attempt to reconnect the socket below
			}

			if (rc == -1)
			{
				if (socket->getRawSocketHandle() == -1)
				{
					CoreServices::sendStatusMessage("Ephys Socket: Socket handle invalid.");
					LOGE("Ephys Socket: Socket handle is invalid");
					error_flag = true;
					continue;
				}
				else
				{
					CoreServices::sendStatusMessage("Ephys Socket: Socket read error");
					LOGE("Ephys Socket: Reading from socket did not complete");
					error_flag = true;
					continue;
				}
			}
			else if (rc == 0)
			{
				if (difftime(time(nullptr), lastPacketReceived) >= 2)
				{
					LOGD("Last packet was too old.");

					socket->close();
					socket.reset();

					connected = false;
					shouldReconnect = true;

					LOGC("EphysSocket has been disconnected. Attempting to reconnect now.");
					CoreServices::sendStatusMessage("Ephys Socket: Attempting to reconnect...");
				}

				continue;
			}
			else if (rc != bytes_expected)
			{
				int bytes_received = rc;

				while (bytes_received < bytes_expected)
				{
					rc = socket->read(read_buffer.data() + bytes_received, bytes_expected - bytes_received, false);

					if (rc != 0) {
						bytes_received += rc;
					}

					if (threadShouldExit()) {
						return;
					}
				}
			}

			header = EphysSocketHeader(read_buffer);

			if (!compareHeaders(header))
			{
				CoreServices::sendStatusMessage("Ephys Socket: Invalid header");
				LOGE("Ephys Socket: Header values have changed since first connecting");
				error_flag = true;
				continue;
			}

			lastPacketReceived = time(nullptr);

			if (acquiring) {
				data.add(read_buffer);
			}
		}
		else if (shouldReconnect) {
			attemptToReconnect();

			if (error_flag) {
				const MessageManagerLock mmLock;
				editor->enableInputs();
				return;
			}
		}
	}
}
