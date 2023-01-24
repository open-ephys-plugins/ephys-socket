import socket
import time

import numpy as np

# ---- SPECIFY THE SIGNAL PROPERTIES ---- #
totalDuration = 10   # the total duration of the signal
numChannels = 4      # number of channels to send
bufferSize = 500     # size of the data buffer
Freq = 30000         # sample rate of the signal
testingValue1 = 100  # high value
testingValue2= -100  # low value

# ---- COMPUTE SOME USEFUL VALUES ---- #
bytesPerBuffer = bufferSize * 2 * numChannels
buffersPerSecond = Freq / bufferSize
bufferInterval = 1 / buffersPerSecond

# ---- GENERATE THE DATA ---- #
OpenEphysOffset = 32768
convertedValue1 = OpenEphysOffset+(testingValue1/0.195)
convertedValue2 = OpenEphysOffset+(testingValue2/0.195)
intList_1 = (np.ones((int(Freq/2),)) * convertedValue1).astype('uint16')
intList_2 = (np.ones((int(Freq/2),)) * convertedValue2).astype('uint16')
oneCycle = np.concatenate((intList_1, intList_2))
allData = np.tile(oneCycle, (numChannels, totalDuration)).T

# ---- SPECIFY THE IP AND PORT ---- #
serverAddressPort = ("127.0.0.1", 9001)

# ---- CREATE THE SOCKET OBJECT ---- #
UDPClientSocket = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)

# ---- WAIT FOR USER INPUT ---- #
value = input("Press enter key to start...")

# ---- CONVERT DATA TO BYTES ---- #
bytesToSend = allData.flatten().tobytes()
totalBytes = len(bytesToSend)
bufferIndex = 0

print("Starting transmission")

def currentTime():
    return time.time_ns() / (10 ** 9)

# ---- STREAM DATA ---- #
while (bufferIndex < totalBytes):
    t1 = currentTime()
    UDPClientSocket.sendto(bytesToSend[bufferIndex:bufferIndex+bytesPerBuffer], serverAddressPort)
    t2 = currentTime()

    while ((t2 - t1) < bufferInterval):
        t2 = currentTime()

    bufferIndex += bytesPerBuffer

print("Done")
