#ifndef FILES_H
#define FILES_H

#include <string>
#include "types.h"

class IFile
{
public:
	virtual bool          eof()  const = 0;
	virtual unsigned long size() const = 0;
	virtual unsigned long tell() const = 0;
	virtual void          seek(unsigned long offset) = 0;
	virtual unsigned long read(void* buffer, unsigned long size) = 0;
	virtual const std::string& name() const = 0;
};

class PhysicalFile : public IFile
{
private:
	HANDLE        hFile;
	unsigned long m_position;
	unsigned long m_size;
	std::string   m_name;

public:
	bool          eof()  const               { return m_position == m_size; }
	unsigned long size() const               { return m_size; }
	unsigned long tell() const               { return m_position; }
	void          seek(unsigned long offset) { m_position = min(offset, m_size); }
	unsigned long read(void* buffer, unsigned long size);
	const std::string& name() const			{ return m_name; }

	PhysicalFile(const std::string& name);
	~PhysicalFile();
};

#endif