#include "RFF_Reader.h"
#include "../EngineSettings.h"
#include "../ui/XL_Console.h"
#include "../memory/ScratchPad.h"
#include <string.h>
#include <stdio.h>

bool m_bEncrypt;

// File flags
enum
{
    FLAG_NONE      = 0,
    FLAG_ENCRYPTED = (1 << 4)  // 0x10
};

struct RffHeader
{
    u8  Magic[4];
    u32 Version;
    u32 DirOffset;
    u32 fileCount;
    u32 Unknown1;
    u32 Unknown2;
    u32 Unknown3;
    u32 Unknown4;
};

// Directory entry for a file
struct DirectoryEntry
{
    u8  Unknown0[16];
    u32 Offset;
    u32 Size;
    u32 Unknown1;
    u32 Time;           // Obtained with the "time" standard function
    u8  Flags;
    char Name[11];
    u32 Unknown2;       // ID ? Maybe for an enumeration function...
};

// Informations about a packed file
struct FileInfo
{
    char Name[13];
    u8  Flags;
    u32 Time;
    u32 Size;
    u32 Offset;
};

RFF_Reader::RFF_Reader() : Archive()
{
	m_CurFile = -1;
	m_pFile = NULL;
	m_pFileLocal = NULL;
	m_pFileList = NULL;
	m_uFileCount = 0;
	m_bEncrypt = false;
}

bool RFF_Reader::Open(const char *pszName)
{
	sprintf(m_szFileName, "%s%s", EngineSettings::GetGameDataDir(), pszName);

	FILE *f = fopen(m_szFileName, "rb");
	if ( f )
	{
		fseek(f, 0, SEEK_END);
		u32 len = ftell(f)+1;
		fseek(f, 0, SEEK_SET);
		ScratchPad::StartFrame();
		u8 *pBuffer = (u8 *)ScratchPad::AllocMem(len);

		RffHeader header;
		fread(&header, sizeof(RffHeader), 1, f);
		if ( (header.Magic[0] != 'R') || (header.Magic[1] != 'F') || (header.Magic[2] != 'F') || (header.Magic[3] != 0x1a) )
		{
			fclose(f);
			XL_Console::PrintF("^1Error: %s not a valid .RFF file", pszName);
			ScratchPad::FreeFrame();
			return false;
		}

		if ( header.Version == 0x301 ) 
			m_bEncrypt = true;
		else
			m_bEncrypt = false;

		u32 uOffset  = header.DirOffset;
		m_uFileCount = header.fileCount;

		m_pFileList = xlNew RFF_File[m_uFileCount];

		//Read directory.
		DirectoryEntry *pDirectory = (DirectoryEntry *)pBuffer;
		fseek(f, uOffset, SEEK_SET);
		fread(pBuffer, sizeof(DirectoryEntry), m_uFileCount, f);
		fclose(f);

		// Decrypt the directory (depend on the version)
		if ( m_bEncrypt )
		{
			u8 CryptoByte = (u8)header.DirOffset;
			for (u32 i = 0; i < m_uFileCount * sizeof(DirectoryEntry); i += 2)
			{
				pBuffer[i+0] ^= CryptoByte;
				pBuffer[i+1] ^= CryptoByte;
				CryptoByte++;
			}
		}

		DirectoryEntry *pDirectoryEntry = (DirectoryEntry *)pBuffer;

		//Now go through each file listing and fill out the RFF_File structure.
		char szFileName[9];
		char szFileExtension[4];
		for(u32 i=0, l=0; i<m_uFileCount; i++, l+=48)
		{
			strncpy(szFileExtension, pDirectoryEntry[i].Name,     3);
			strncpy(szFileName,      &pDirectoryEntry[i].Name[3], 8);
			szFileExtension[3] = 0;
			szFileName[8] = 0;

			m_pFileList[i].offset = pDirectoryEntry[i].Offset;
			m_pFileList[i].length = pDirectoryEntry[i].Size;
			m_pFileList[i].flags  = pDirectoryEntry[i].Flags;
			sprintf(m_pFileList[i].szName, "%s.%s", szFileName, szFileExtension);
		}

		m_bOpen = true;

		ScratchPad::FreeFrame();
		return true;
	}
	XL_Console::PrintF("^1Error: Failed to load %s", m_szFileName);

	return false;
}

void RFF_Reader::Close()
{
	CloseFile();
	if ( m_pFileList )
	{
		xlDelete [] m_pFileList;
	}
	m_bOpen = false;
}

FILE *OpenFile_Local(const char *pszFile)
{
	return fopen(pszFile, "rb");
}

//This is different then other archives, we're reading indices here...
bool RFF_Reader::OpenFile(const char *pszFile)
{
	if ( m_pFileLocal = OpenFile_Local(pszFile) )
	{
		m_pFile = NULL;
		return true;
	}

	m_pFile = fopen(m_szFileName, "rb");
	m_CurFile = -1;
    
	if ( m_pFile )
	{
		//search for this file.
		for (u32 i=0; i<m_uFileCount; i++)
		{
			if ( stricmp(pszFile, m_pFileList[i].szName) == 0 )
			{
				m_CurFile = i;
				break;
			}
		}

		if ( m_CurFile == -1 )
		{
			XL_Console::PrintF("^1Error: Failed to load %s from \"%s\"", pszFile, m_szFileName);
		}
	}

	return m_CurFile > -1 ? true : false;
}

void RFF_Reader::CloseFile()
{
	if ( m_pFileLocal )
	{
		fclose(m_pFileLocal);
		m_pFileLocal = NULL;
		return;
	}

	if ( m_pFile )
	{
		fclose(m_pFile);
		m_pFile = NULL;
	}
	m_CurFile = -1;
}

u32 RFF_Reader::GetFileLen()
{
	if ( m_pFileLocal )
	{
		fseek(m_pFileLocal, 0, SEEK_END);
		u32 len = ftell(m_pFileLocal)+1;
		fseek(m_pFileLocal, 0, SEEK_SET);

		return len;
	}

	return m_pFileList[m_CurFile].length;
}

bool RFF_Reader::ReadFile(void *pData, u32 uLength)
{
	if ( m_pFileLocal )
	{
		fread(pData, uLength, 1, m_pFileLocal);
		return true;
	}

	if ( !m_pFile ) { return false; }

	fseek(m_pFile, m_pFileList[m_CurFile].offset, SEEK_SET);
	if ( uLength == 0 )
		uLength = GetFileLen();

	//now reading from an RFF file is more involved due to the encryption.
	fread(pData, uLength, 1, m_pFile);

	if ( m_bEncrypt && (m_pFileList[m_CurFile].flags & FLAG_ENCRYPTED) )
	{
		// Decrypt the first bytes if they're encrypted (256 bytes max)
		for (u32 i = 0; i < 256 && i < uLength; i++)
		{
			((u8 *)pData)[i] ^= (i >> 1);
		}
	}

	return true;
}

//This is needed for this particular archive because it contains size info for the tile.
void *RFF_Reader::ReadFileInfo()
{
	return NULL;
}

s32 RFF_Reader::GetFileCount()
{
	return (s32)m_uFileCount;
}

const char *RFF_Reader::GetFileName(s32 nFileIdx)
{
	return m_pFileList[nFileIdx].szName;
}
