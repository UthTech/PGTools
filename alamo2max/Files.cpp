#include "files.h"
#include "exceptions.h"
using namespace std;

unsigned long PhysicalFile::read(void* buffer, unsigned long size)
{
	SetFilePointer(hFile, m_position, NULL, FILE_BEGIN);
	if (!ReadFile(hFile, buffer, size, &size, NULL))
	{
		throw ReadException();
	}
	m_position = min(m_position + size, m_size);
	return size;
}

PhysicalFile::PhysicalFile(const string& filename) : m_name(filename)
{
	hFile = CreateFileA(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		DWORD error = GetLastError();
		if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
		{
			throw FileNotFoundException(filename);
		}
		throw IOException("Unable to open file:\n" + filename);
	}
	m_size     = GetFileSize(hFile, NULL);
	m_position = 0;
}

PhysicalFile::~PhysicalFile()
{
	CloseHandle(hFile);
}
