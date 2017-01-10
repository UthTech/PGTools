#ifndef CHUNKFILE_H
#define CHUNKFILE_H

#include <string>

#include "files.h"
#include "types.h"

namespace Alamo
{

typedef long ChunkType;

class ChunkReader
{
	static const int MAX_CHUNK_DEPTH = 256;

	IFile* m_file;
	long   m_position;
	long   m_size;
	long   m_offsets[ MAX_CHUNK_DEPTH ];
	long   m_miniSize;
	long   m_miniOffset;
	int    m_curDepth;

public:
	ChunkType   next();
	ChunkType   nextMini();
	void        skip();
	long        size();
	long        read(void* buffer, long size, bool check = true);

	float			readFloat();
	unsigned char   readByte();
	unsigned short	readShort();
	unsigned long	readInteger();
	std::string		readString();

	ChunkReader(IFile* file);
	~ChunkReader();
};

}
#endif