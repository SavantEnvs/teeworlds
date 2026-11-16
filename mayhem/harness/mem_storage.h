#ifndef MAYHEM_MEM_STORAGE_H
#define MAYHEM_MEM_STORAGE_H

#include <engine/storage.h>

#include <stdio.h>

// IStorage whose only file is the fuzzer's input, served from memory; nothing touches the filesystem.
class CMemStorage : public IStorage
{
	const void *m_pData;
	size_t m_Size;

public:
	CMemStorage(const void *pData, size_t Size) : m_pData(pData), m_Size(Size) {}

	virtual void ListDirectory(int Type, const char *pPath, FS_LISTDIR_CALLBACK pfnCallback, void *pUser) {}
	virtual void ListDirectoryFileInfo(int Type, const char *pPath, FS_LISTDIR_CALLBACK_FILEINFO pfnCallback, void *pUser) {}
	virtual IOHANDLE OpenFile(const char *pFilename, int Flags, int Type, char *pBuffer = 0, int BufferSize = 0, FCheckCallback pfnCheckCB = 0, const void *pCheckCBData = 0)
	{
		if((Flags & IOFLAG_READ) == 0 || m_Size == 0)
			return 0;
		return (IOHANDLE)fmemopen(const_cast<void *>(m_pData), m_Size, "rb");
	}
	virtual bool ReadFile(const char *pFilename, int Type, void **ppResult, unsigned *pResultLen) { return false; }
	virtual char *ReadFileStr(const char *pFilename, int Type) { return 0; }
	virtual bool FindFile(const char *pFilename, const char *pPath, int Type, char *pBuffer, int BufferSize) { return false; }
	virtual bool FindFile(const char *pFilename, const char *pPath, int Type, char *pBuffer, int BufferSize, const SHA256_DIGEST *pWantedSha256, unsigned WantedCrc, unsigned WantedSize) { return false; }
	virtual bool RemoveFile(const char *pFilename, int Type) { return false; }
	virtual bool RenameFile(const char *pOldFilename, const char *pNewFilename, int Type) { return false; }
	virtual bool CreateFolder(const char *pFoldername, int Type) { return false; }
	virtual void GetCompletePath(int Type, const char *pDir, char *pBuffer, unsigned BufferSize) {}
	virtual bool GetHashAndSize(const char *pFilename, int StorageType, SHA256_DIGEST *pSha256, unsigned *pCrc, unsigned *pSize) { return false; }
	virtual bool GetFileTime(const char *pFilename, int StorageType, time_t *pCreated, time_t *pModified) { return false; }
};

#endif
