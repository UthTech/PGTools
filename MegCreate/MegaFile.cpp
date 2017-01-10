#include "MegaFile.h"
#include "ExactTypes.h"
#include <algorithm>
#include <iostream>
using namespace std;

#pragma pack(1)
struct Header
{
    uint32_t nFilenames;
    uint32_t nFiles;
};

struct FileDesc
{
    uint32_t crc;
    uint32_t index;
    uint32_t size;
    uint32_t start;
    uint32_t name;
};
#pragma pack()

// Calculates the 32-bit Cyclic Redundancy Checksum (CRC-32) of a block of data
static unsigned long CRC32(const void *data, size_t size)
{
    static unsigned long lookupTable[256];
    static bool          validTable = false;

	if (!validTable)
	{
        // Initialize table
	    for (int i = 0; i < 256; i++)
        {
		    unsigned long crc = i;
            for (int j = 0; j < 8; j++)
		    {
			    crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320 : (crc >> 1);
		    }
            lookupTable[i] = crc & 0xFFFFFFFF;
	    }
	    validTable = true;
	}

	unsigned long crc = 0xFFFFFFFF;
	for (size_t j = 0; j < size; j++)
	{
		crc = ((crc >> 8) & 0x00FFFFFF) ^ lookupTable[ (crc ^ ((const char*)data)[j]) & 0xFF ];
	}
	return crc ^ 0xFFFFFFFF;
}

static bool Read(HANDLE hFile, void* buffer, size_t size)
{
    DWORD read = size;
    return ReadFile(hFile, buffer, read, &read, NULL) && read == size;
}

static bool Write(HANDLE hFile, const void* buffer, size_t size)
{
    DWORD written = size;
    return WriteFile(hFile, buffer, written, &written, NULL) && written == size;
}

bool MegaFile::Load(HANDLE hFile, vector<FileInfo>& files)
{
    files.clear();

    Header header;
    if (!Read(hFile, &header, sizeof header))
    {
        return false;
    }

    vector<char>                    filenames;
    vector<pair<size_t, size_t> >   offsets(letohl(header.nFilenames));

    filenames.reserve(offsets.size() * 40); // Assume an average of 40 chars per file

    for (unsigned long i = 0; i < offsets.size(); i++)
    {
        uint16_t size;
        if (!Read(hFile, &size, sizeof size)) {
            return false;
        }
        offsets[i].first  = filenames.size();
        offsets[i].second = letohl(size);
        filenames.resize(filenames.size() + offsets[i].second);
        if (!Read(hFile, &filenames[offsets[i].first], offsets[i].second )) {
            return false;
        }
    }

    files.resize(letohl(header.nFiles));
    for (unsigned long i = 0; i < files.size(); i++)
    {
        FileDesc desc;
        if (!Read(hFile, &desc, sizeof desc)) {
            return false;
        }
        unsigned long name = letohl(desc.name);
        files[i].size = letohl(desc.size);
        files[i].name.resize(offsets[name].second);
        std::copy(&filenames[offsets[name].first],
                  &filenames[offsets[name].first] + offsets[name].second,
                  files[i].name.begin());
    }
    return true;
}

bool MegaFile::Save(HANDLE hFile, const vector<FileInfo>& files)
{
    struct Info
    {
        unsigned long crc;
        size_t        name;
        unsigned long size;
        unsigned long start;

        bool operator < (const Info& rhs) const {
            return crc < rhs.crc;
        }
    };

    vector<string> filenames(files.size());
    vector<Info>   index(files.size());

    // Base of file data
    unsigned long base  = sizeof(Header) + files.size() * sizeof(FileDesc);
    unsigned long start = 0; // Start of next file, relative to base

    for (size_t i = 0; i < files.size(); i++)
    {
        filenames[i].resize(files[i].name.length());
        transform(files[i].name.begin(), files[i].name.end(), filenames[i].begin(), string::traits_type::to_char_type);

        index[i].crc   = CRC32(filenames.back().c_str(), filenames.back().length());
        index[i].name  = i;
        index[i].size  = files[i].size;
        index[i].start = start;

        start += files[i].size;
        base  += sizeof(uint16_t) + filenames[i].length();
    }
    sort(index.begin(), index.end());

    // Write header
    Header header;
    header.nFiles     = htolel(index.size());
    header.nFilenames = htolel(index.size());
    if (!Write(hFile, &header, sizeof header))
    {
        return false;
    }

    // Write filenames
    for (size_t i = 0; i < filenames.size(); i++)
    {
        uint16_t length = htolel(filenames[i].length());
        if (!Write(hFile, &length, sizeof length) ||
            !Write(hFile, filenames[i].c_str(), filenames[i].length()))
        {
            return false;
        }
    }

    // Write file index
    for (size_t i = 0; i < index.size(); i++)
    {
        FileDesc desc;
        desc.crc   = htolel(index[i].crc);
        desc.index = htolel(i);
        desc.name  = htolel(index[i].name);
        desc.size  = htolel(index[i].size);
        desc.start = htolel(base + index[i].start);
        if (!Write(hFile, &desc, sizeof desc))
        {
            return false;
        }
    }

    // Copy files
    for (size_t i = 0; i < files.size(); i++)
    {
        static const unsigned int BUFFER_SIZE = 512*1024;
        static char buffer[BUFFER_SIZE];

        HANDLE hSource = CreateFile(files[i].name.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hSource == INVALID_HANDLE_VALUE)
        {
            return false;
        }
        
        DWORD size = GetFileSize(hSource, NULL);
        while (size > 0)
        {
            DWORD read;
            if (!ReadFile(hSource, &buffer, min(size, BUFFER_SIZE), &read, NULL) || read == 0)
            {
                CloseHandle(hSource);
                return false;
            }
            
            for (DWORD written, to_write = read; to_write > 0; to_write -= written)
            {
                if (!WriteFile(hFile, &buffer, to_write, &written, NULL) || written == 0)
                {
                    CloseHandle(hSource);
                    return false;
                }
            }
            size -= read;
        }
        CloseHandle(hSource);
    }

    return true;
}