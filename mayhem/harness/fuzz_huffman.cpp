// Huffman codec used on every network packet: decode arbitrary bytes, and check encode/decode round-trips.
#include <engine/shared/huffman.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static CHuffman s_Huffman;

extern "C" int LLVMFuzzerInitialize(int *pArgc, char ***pppArgv)
{
	s_Huffman.Init();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *pData, size_t Size)
{
	if(Size > 1 << 20)
		return -1;

	static unsigned char s_aDecoded[64 * 1024];
	s_Huffman.Decompress(pData, (int)Size, s_aDecoded, sizeof(s_aDecoded));

	const int OutSize = (int)Size * 4 + 64;
	unsigned char *pPacked = (unsigned char *)malloc(OutSize);
	unsigned char *pBack = (unsigned char *)malloc(Size + 1);
	const int Packed = s_Huffman.Compress(pData, (int)Size, pPacked, OutSize);
	if(Packed < 0)
		abort();
	const int Back = s_Huffman.Decompress(pPacked, Packed, pBack, (int)Size + 1);
	if(Back != (int)Size || memcmp(pBack, pData, Size) != 0)
		abort();
	free(pPacked);
	free(pBack);
	return 0;
}
