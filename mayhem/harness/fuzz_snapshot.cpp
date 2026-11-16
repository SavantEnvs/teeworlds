// Snapshot delta decoding as done by the client: variable-int decompress, UnpackDelta against a known
// previous snapshot, then use of the result.
#include <engine/shared/compression.h>
#include <engine/shared/snapshot.h>

#include <stdint.h>
#include <string.h>

static const int s_aStaticSizes[][2] = {{1, 40}, {2, 16}, {3, 8}, {4, 24}};

alignas(16) static char s_aFromBuffer[CSnapshot::MAX_SIZE];

extern "C" int LLVMFuzzerInitialize(int *pArgc, char ***pppArgv)
{
	CSnapshotBuilder Builder;
	Builder.Init();
	const int aKeys[][2] = {{1, 0}, {1, 1}, {2, 0}, {3, 5}, {4, 2}, {9, 0}};
	for(const auto &Key : aKeys)
	{
		int Size = Key[0] == 9 ? 12 : 0;
		for(const auto &Static : s_aStaticSizes)
			if(Static[0] == Key[0])
				Size = Static[1];
		int *pItem = (int *)Builder.NewItem(Key[0], Key[1], Size);
		for(int i = 0; i < Size / 4; i++)
			pItem[i] = Key[0] * 100 + Key[1] * 10 + i;
	}
	Builder.Finish(s_aFromBuffer);
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *pData, size_t Size)
{
	static int s_aDelta[CSnapshot::MAX_SIZE / sizeof(int)];
	alignas(16) static char s_aTo[CSnapshot::MAX_SIZE];

	// UnpackDelta is handed the client's fixed-size decompression buffer, which starts zeroed; the output
	// snapshot is reset too, so a read of stale bytes cannot depend on an earlier input
	memset(s_aDelta, 0, sizeof(s_aDelta));
	memset(s_aTo, 0, sizeof(s_aTo));
	const void *pDeltaData = 0;
	int DeltaSize = 0;
	CSnapshotDelta *pDelta = new CSnapshotDelta();
	for(const auto &Static : s_aStaticSizes)
		pDelta->SetStaticsize(Static[0], Static[1]);
	if(Size == 0)
	{
		pDeltaData = pDelta->EmptyDelta();
		DeltaSize = sizeof(int) * 3;
	}
	else
	{
		const long Bytes = CVariableInt::Decompress(pData, (int)Size, s_aDelta, sizeof(s_aDelta));
		if(Bytes < 0)
		{
			delete pDelta;
			return -1;
		}
		pDeltaData = s_aDelta;
		DeltaSize = (int)Bytes;
	}

	const int SnapSize = pDelta->UnpackDelta((const CSnapshot *)s_aFromBuffer, (CSnapshot *)s_aTo, pDeltaData, DeltaSize);
	delete pDelta;
	if(SnapSize < 0)
		return -1;

	const CSnapshot *pSnap = (const CSnapshot *)s_aTo;
	volatile unsigned Sink = (unsigned)pSnap->Crc();
	for(int i = 0; i < pSnap->NumItems(); i++)
	{
		const CSnapshotItem *pItem = pSnap->GetItem(i);
		Sink = Sink + (unsigned)pSnap->GetItemSize(i) + (unsigned)pItem->Type() + (unsigned)pItem->ID();
		if(pSnap->GetItemIndex(pItem->Key()) != i)
			return 0;
	}

	CSnapshotStorage Storage;
	Storage.Init();
	Storage.Add(1, 0, SnapSize, pSnap, true);
	Storage.Add(2, 0, SnapSize, pSnap, false);
	CSnapshot *pGot;
	CSnapshot *pAlt;
	if(Storage.Get(1, 0, &pGot, &pAlt) != SnapSize)
		return 0;
	Sink = Sink + (unsigned)pGot->NumItems();
	Storage.PurgeUntil(2);
	return 0;
}
