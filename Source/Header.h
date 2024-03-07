#ifndef __EPHYSHEADERH__
#define __EPHYSHEADERH__

#include <DataThreadHeaders.h>

namespace EphysSocketNode
{
	enum Depth { U8, S8, U16, S16, S32, F32, F64 };

	class Header
	{
	public:

		Header();

		Header(std::vector<std::byte>& header_bytes);

		Header(std::vector<std::byte>& header_bytes, int offset);

		int offset;
		int num_bytes;
		Depth depth;
		int element_size;
		int num_samp;
		int num_channels;
	};
}

#endif
