// Variable-length integer coding used for snapshot deltas: decode arbitrary bytes, and check round-trips.
#include <engine/shared/compression.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *pData, size_t Size)
{
	if(Size > 1 << 20)
		return -1;

	volatile unsigned Sink = 0;
	const unsigned char *pEnd = pData + Size;
	for(const unsigned char *p = pData; p && p < pEnd;)
	{
		int Value = 0;
		p = CVariableInt::Unpack(p, &Value, (int)(pEnd - p));
		Sink = Sink + (unsigned)Value;
	}

	if(Size >= sizeof(int))
	{
		int Value;
		memcpy(&Value, pData, sizeof(Value));
		unsigned char aPacked[CVariableInt::MAX_BYTES_PACKED];
		unsigned char *pPackedEnd = CVariableInt::Pack(aPacked, Value, sizeof(aPacked));
		int Back = 0;
		if(!pPackedEnd || CVariableInt::Unpack(aPacked, &Back, (int)(pPackedEnd - aPacked)) != pPackedEnd || Back != Value)
			abort();
	}

	static int s_aDecoded[16 * 1024];
	const long Bytes = CVariableInt::Decompress(pData, (int)Size, s_aDecoded, sizeof(s_aDecoded));
	if(Bytes < 0)
		return -1;

	static unsigned char s_aPacked[CVariableInt::MAX_BYTES_PACKED * 16 * 1024];
	const long Packed = CVariableInt::Compress(s_aDecoded, (int)Bytes, s_aPacked, sizeof(s_aPacked));
	if(Packed < 0)
		abort();
	static int s_aBack[16 * 1024];
	if(CVariableInt::Decompress(s_aPacked, (int)Packed, s_aBack, sizeof(s_aBack)) != Bytes || memcmp(s_aBack, s_aDecoded, Bytes) != 0)
		abort();
	return 0;
}
