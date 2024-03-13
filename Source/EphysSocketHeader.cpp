#include "EphysSocketHeader.h"

using namespace EphysSocketNode;

EphysSocketHeader::EphysSocketHeader()
{
    offset = 0;
    num_bytes = 1024;
    depth = U16;
    element_size = 2;
    num_channels = 1;
    num_samp = 512;
}

EphysSocketHeader::EphysSocketHeader(std::vector<std::byte>& header_bytes)
{
    offset = (int)header_bytes[3] << 24 | (int)header_bytes[2] << 16 | (int)header_bytes[1] << 8 | (int)header_bytes[0];
    num_bytes = (int)header_bytes[7] << 24 | (int)header_bytes[6] << 16 | (int)header_bytes[5] << 8 | (int)header_bytes[4];
    depth = (Depth)((int)header_bytes[9] << 8 | (int)header_bytes[8]);
    element_size = (int)header_bytes[13] << 24 | (int)header_bytes[12] << 16 | (int)header_bytes[11] << 8 | (int)header_bytes[10];
    num_channels = (int)header_bytes[17] << 24 | (int)header_bytes[16] << 16 | (int)header_bytes[15] << 8 | (int)header_bytes[14];
    num_samp = (int)header_bytes[21] << 24 | (int)header_bytes[20] << 16 | (int)header_bytes[19] << 8 | (int)header_bytes[18];
}

EphysSocketHeader::EphysSocketHeader(std::vector<std::byte>& header_bytes, int _offset)
{
    offset = (int)header_bytes[_offset + 3] << 24 | (int)header_bytes[_offset + 2] << 16 | (int)header_bytes[_offset + 1] << 8 | (int)header_bytes[_offset + 0];
    num_bytes = (int)header_bytes[_offset + 7] << 24 | (int)header_bytes[_offset + 6] << 16 | (int)header_bytes[_offset + 5] << 8 | (int)header_bytes[_offset + 4];
    depth = (Depth)((int)header_bytes[_offset + 9] << 8 | (int)header_bytes[_offset + 8]);
    element_size = (int)header_bytes[_offset + 13] << 24 | (int)header_bytes[_offset + 12] << 16 | (int)header_bytes[_offset + 11] << 8 | (int)header_bytes[_offset + 10];
    num_channels = (int)header_bytes[_offset + 17] << 24 | (int)header_bytes[_offset + 16] << 16 | (int)header_bytes[_offset + 15] << 8 | (int)header_bytes[_offset + 14];
    num_samp = (int)header_bytes[_offset + 21] << 24 | (int)header_bytes[_offset + 20] << 16 | (int)header_bytes[_offset + 19] << 8 | (int)header_bytes[_offset + 18];
}
