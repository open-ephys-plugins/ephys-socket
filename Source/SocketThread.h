/*
	------------------------------------------------------------------

	This file is part of the Open Ephys GUI
	Copyright (C) 2020 Allen Institute for Brain Science and Open Ephys

	------------------------------------------------------------------

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef __SOCKET_H__
#define __SOCKET_H__

#include <DataThreadHeaders.h>
#include "EphysSocketHeader.h"
#include "EphysSocketEditor.h"

#include <atomic>

namespace EphysSocketNode
{
	class SocketThread : public Thread
	{
	public:

		SocketThread(String name);

		~SocketThread();

		/** Starts probe data streaming */
		void startAcquisition();

		/** Stops probe data streaming*/
		void stopAcquisition();

		/** Attempts to connect to the socket */
		bool connectSocket(int port, bool printOutput = true);

		/** Disconnects the socket */
		void disconnectSocket();

		/** Returns if any errors were thrown during acquisition, such as invalid headers or unable to read from socket */
		bool isError() const;

		bool isConnected();

		void setEditor(EphysSocketEditor* ed);

		Array<std::vector<std::byte>, CriticalSection, 10> data;

		/** Variables that are part of the incoming header */
		int num_bytes;
		int element_size;
		Depth depth;
		int num_samp;
		int num_channels;

	private:

		/** Default socket parameters */
		const int DEFAULT_NUM_SAMPLES = 256;
		const int DEFAULT_NUM_CHANNELS = 64;
		const Depth DEFAULT_DEPTH = U16;
		const int DEFAULT_NUM_BYTES = 32678; // NB: 256 * 64 * 2
		const int DEFAULT_ELEMENT_SIZE = 2;

		void run() override;

		/** Compares a newly parsed header to existing variables */
		bool compareHeaders(EphysSocketHeader header) const;

		void attemptToReconnect();

		/** Pointer to the editor */
		EphysSocketEditor* editor;

		/** TCP Socket object */
		std::unique_ptr<StreamingSocket> socket;

		/** Mutex for thread safety */
		std::mutex socketMutex;

		/** Internal buffers */
		std::vector<std::byte> read_buffer;

		std::atomic<bool> connected;
		std::atomic<bool> shouldReconnect;
		std::atomic<bool> acquiring;
		std::atomic<bool> error_flag;

		std::time_t lastPacketReceived;

		int previousPort;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SocketThread);
	};
}

#endif // !__SOCKET_H__
