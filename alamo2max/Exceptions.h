#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include "types.h"
#include <string>
#include <stdexcept>

class IOException : public std::runtime_error
{
public:
	IOException(const std::string& message) : std::runtime_error(message) {}
};

class FileNotFoundException : public IOException
{
public:
	FileNotFoundException(const std::string filename) : IOException("Unable to find file:\n" + filename) {}
};

class ReadException : public IOException
{
public:
	ReadException() : IOException("Unable to read file") {}
};

class BadFileException : public IOException
{
public:
	BadFileException(const std::string& filename) : IOException("Bad or corrupted file:\n" + filename) {}
};

#endif