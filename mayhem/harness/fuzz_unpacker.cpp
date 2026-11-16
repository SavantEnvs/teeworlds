// Message unpacker (CUnpacker) that parses every network message: a script of reads is run over the payload.
#include <engine/shared/packer.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *pData, size_t Size)
{
	if(Size < 1)
		return -1;

	// data[0] is the length of the read script that follows; the rest is the message payload
	const size_t ScriptSize = pData[0] < Size - 1 ? pData[0] : Size - 1;
	// CUnpacker::GetString sanitizes strings in place, so work on a private copy of the input
	std::vector<uint8_t> Copy(pData, pData + Size);
	const uint8_t *pScript = Copy.data() + 1;
	const uint8_t *pPayload = pScript + ScriptSize;
	const size_t PayloadSize = Size - 1 - ScriptSize;

	CUnpacker Unpacker;
	Unpacker.Reset(pPayload, (int)PayloadSize);
	volatile unsigned Sink = 0;
	for(size_t i = 0; i < ScriptSize; i++)
	{
		switch(pScript[i] % 6)
		{
		case 0:
			Sink = Sink + (unsigned)Unpacker.GetInt();
			break;
		case 1:
			Sink = Sink + (unsigned)strlen(Unpacker.GetString(CUnpacker::SANITIZE));
			break;
		case 2:
			Sink = Sink + (unsigned)strlen(Unpacker.GetString(CUnpacker::SANITIZE_CC | CUnpacker::SKIP_START_WHITESPACES));
			break;
		case 3:
			Sink = Sink + (unsigned)strlen(Unpacker.GetString(CUnpacker::SANITIZE_CC));
			break;
		case 4:
		{
			const int RawSize = i + 1 < ScriptSize ? pScript[++i] : 0;
			const unsigned char *pRaw = Unpacker.GetRaw(RawSize);
			if(pRaw && RawSize > 0)
				Sink = Sink + pRaw[RawSize - 1];
			break;
		}
		case 5:
			Sink = Sink + (unsigned)Unpacker.GetIntOrDefault(-1);
			break;
		}
		if(Unpacker.Error())
			break;
	}

	// Packer round trip: ints and a string written from the payload must read back identically.
	CPacker Packer;
	Packer.Reset();
	const size_t NumInts = PayloadSize / sizeof(int32_t) < 64 ? PayloadSize / sizeof(int32_t) : 64;
	int32_t aInts[64];
	memcpy(aInts, pPayload, NumInts * sizeof(int32_t));
	for(size_t i = 0; i < NumInts; i++)
		Packer.AddInt(aInts[i]);
	if(!Packer.Error())
	{
		CUnpacker Back;
		Back.Reset(Packer.Data(), Packer.Size());
		for(size_t i = 0; i < NumInts; i++)
			if(Back.GetInt() != aInts[i])
				abort();
		if(Back.Error() || Back.RemainingSize() != 0)
			abort();
	}
	return 0;
}
