// Datafile container reader (maps, config packages): parse the header and walk every item and data block.
#include "mem_storage.h"

#include <engine/shared/datafile.h>

#include <stdint.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *pData, size_t Size)
{
	CMemStorage Storage(pData, Size);
	CDataFileReader Reader;
	if(!Reader.Open(&Storage, "fuzz.map", IStorage::TYPE_ALL))
		return -1;

	volatile unsigned Sink = 0;

	// the hash Open() took must verify against a fresh read of the same file
	const SHA256_DIGEST Digest = Reader.Sha256();
	if(IOHANDLE Again = Storage.OpenFile("fuzz.map", IOFLAG_READ, IStorage::TYPE_ALL))
	{
		Sink = Sink + (unsigned)CDataFileReader::CheckSha256(Again, &Digest);
		io_close(Again);
	}

	for(int i = 0; i < Reader.NumItems(); i++)
	{
		int Type, ID;
		const void *pItem = Reader.GetItem(i, &Type, &ID);
		Sink = Sink + (unsigned)Reader.GetItemSize(i) + (unsigned)Type + (unsigned)ID + (pItem != 0);
		Reader.FindItem(Type, ID);
		int Start, Num;
		Reader.GetType(Type, &Start, &Num);
	}
	for(int i = 0; i < Reader.NumData(); i++)
	{
		const int DataSize = Reader.GetDataSize(i);
		const unsigned char *pBlock = (const unsigned char *)Reader.GetData(i);
		if(pBlock && DataSize > 0)
			Sink = Sink + pBlock[0] + pBlock[DataSize - 1];
		Reader.GetDataSwapped(i);
		Reader.UnloadData(i);
	}
	if(Reader.NumData() > 0)
	{
		char *pReplacement = (char *)mem_alloc(8);
		mem_zero(pReplacement, 8);
		Reader.ReplaceData(0, pReplacement, 8);
		Sink = Sink + (unsigned)Reader.GetDataSize(0);
	}
	Sink = Sink + Reader.Crc();
	return 0;
}
