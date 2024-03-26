import socket
import time

import numpy as np

# ---- SPECIFY THE SIGNAL PROPERTIES ---- #
totalDuration = 10   # the total duration of the signal
numChannels = 4      # number of channels to send
numSamples = 500     # size of the data buffer
Freq = 30000         # sample rate of the signal
testingValue1 = 100  # high value
testingValue2= -100  # low value

# ---- DEFINE HEADER VALUES ---- #
headerSize  = 22 # Specifies that there are 22 bytes in the header
offset      = 0 # Offset of bytes in this packet; only used for buffers > ~64 kB
dataType    = 2 # Enumeration value based on OpenCV.Mat data types
elementSize = 2 # Number of bytes per element. elementSize = 2 for U16
# Data types:   [ U8, S8, U16, S16, S32, F32, F64 ]
# Enum value:   [  0,  1,   2,   3,   4,   5,   6 ]
# Element Size: [  1,  1,   2,   2,   4,   4,   8 ]  
bytesPerBuffer = numChannels * numSamples * elementSize

header = np.array([offset, bytesPerBuffer], dtype='i4').tobytes() + \
         np.array([dataType], dtype='i2').tobytes() + \
         np.array([elementSize, numChannels, numSamples], dtype='i4').tobytes()

# ---- COMPUTE SOME USEFUL VALUES ---- #
buffersPerSecond = Freq / numSamples
bufferInterval = 1 / buffersPerSecond

# ---- GENERATE THE DATA ---- #
OpenEphysOffset = 100
convertedValue1 = OpenEphysOffset+(testingValue1)
convertedValue2 = OpenEphysOffset+(testingValue2)
intList_1 = (np.ones((int(Freq/2),)) * convertedValue1).astype('uint16') # if dataType == 2, use astype('uint16')
intList_2 = (np.ones((int(Freq/2),)) * convertedValue2).astype('uint16')
oneCycle = np.concatenate((intList_1, intList_2))
allData = np.tile(oneCycle, (numChannels, totalDuration)).T

# ---- CREATE THE SOCKET SERVER ---- #
tcpServer = socket.socket(family=socket.AF_INET, type=socket.SOCK_STREAM)
tcpServer.bind(('localhost', 9001))
tcpServer.listen(1)

# ---- WAIT FOR USER INPUT ---- #
value = input("Press enter key to start...")

print("Waiting for external connection to start...")
(tcpClient, address) = tcpServer.accept()
print("Connected.")

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
    rc = tcpClient.sendto(header + bytesToSend[bufferIndex:bufferIndex+bytesPerBuffer], address)
    t2 = currentTime()
    
    while ((t2 - t1) < bufferInterval):
        t2 = currentTime()
    
    bufferIndex += bytesPerBuffer

print("Done")
