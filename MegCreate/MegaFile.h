#ifndef MEGAFILE_H
#define MEGAFILE_H

#include <windows.h>
#include <string>
#include <vector>

struct FileInfo
{
    std::wstring  name;
    unsigned long size;
};

namespace MegaFile
{
    bool Load(HANDLE hFile, std::vector<FileInfo>& files);
    bool Save(HANDLE hFile, const std::vector<FileInfo>& files);
};

#endif