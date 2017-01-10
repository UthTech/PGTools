#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include "MegaFile.h"

#include <windows.h>
#include <shlwapi.h>
#ifndef NDEBUG
#include <crtdbg.h>
#endif
using namespace std;

typedef vector<FileInfo> FileList;

static void ParseDirectory(FileList& files, FILETIME& latest_modified, const wstring& dir, const TCHAR* filter, bool recurse = true)
{
    const wstring filter_path = dir + filter;
    WIN32_FIND_DATA wfd;
    HANDLE hFind = FindFirstFile(filter_path.c_str(), &wfd);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (wfd.cFileName[0] != L'.') // Ignore "hidden" files
            {
                if (~wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    FileInfo info;
                    info.name = dir + wfd.cFileName;
                    info.size = wfd.nFileSizeLow;
                    transform(info.name.begin(), info.name.end(), info.name.begin(), toupper);
                    files.push_back(info);

                    if (CompareFileTime(&wfd.ftLastWriteTime, &latest_modified) > 0)
                    {
                        latest_modified = wfd.ftLastWriteTime;
                    }
                }
                else if (recurse && wcscmp(wfd.cFileName, L".") != 0 && wcscmp(wfd.cFileName, L"..") != 0)
                {
                    ParseDirectory(files, latest_modified, dir + wfd.cFileName + L"\\", L"*");
                }
        }
        } while (FindNextFile(hFind, &wfd));
        FindClose(hFind);
    }
}

enum
{
    OPT_RECURSIVE     = 1,
    OPT_FORCE_REBUILD = 2,
    OPT_QUIET         = 4,
};

struct Arguments
{
    wstring         output;
    vector<wstring> inputs;
    int             options;
};

static void PrintUsage(const wchar_t* name)
{
    wcerr << "Usage: " << name << " [options] <megafile> [files...]" << endl
          << endl
          << "Takes all specified files and writes them into the target megafile." << endl
          << "Note that the specified file paths can contain wildcards." << endl
          << endl
          << "Options:" << endl
          << "-h    Shows this help" << endl
          << "-f    Force rebuild. Do not check modified dates and file counts and names" << endl
          << "-r    Recursive. When a directory matches the specified file filter, all its" << endl
          << "-q    Quiet. Prints nothing when all goes well" << endl;
}

static bool ParseArguments(Arguments& args, int argc, const TCHAR* const *argv)
{
    if (argc <= 2) {
        PrintUsage(argv[0]);
        return false;
    }

    args.options = 0;

    // Parse options
    int i;
    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] != '-' && argv[i][0] != '/')
        {
            break;
        }

        if (wcscmp(argv[i] + 1, L"h") == 0) {
            PrintUsage(argv[0]);
            return false;
        }

        if (wcscmp(argv[i] + 1, L"r") == 0) {
            args.options |= OPT_RECURSIVE;
        } else if (wcscmp(argv[i] + 1, L"f") == 0) {
            args.options |= OPT_FORCE_REBUILD;
        } else if (wcscmp(argv[i] + 1, L"q") == 0) {
            args.options |= OPT_QUIET;
        } else {
            wcerr << "Unknown option '" << argv[i] << "'" << endl;
            return false;
        }
    }
    
    if (i >= argc) {
        cerr << "no destination file specified" << endl;
        return false;
    }
    args.output = argv[i++];

    for (; i < argc; i++)
    {
        args.inputs.push_back(argv[i]);
    }
    
    if (args.inputs.empty()) {
        cerr << "no source files specified" << endl;
        return false;
    }
    return true;
}

int main()
{
#ifndef NDEBUG
    // In debug mode we turn on memory checking
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
#endif

    try
    {
        Arguments args;

        // Parse the command-line arguments
        int argc;
        const wchar_t* const * argv = CommandLineToArgvW(GetCommandLine(), &argc);
        if (!ParseArguments(args, argc, argv))
        {
            LocalFree((HGLOBAL)argv);
            return 1;
        }
        LocalFree((HGLOBAL)argv);

        // Read the source files
        FileList source_files;              // List of source files that match the specified filters
        FILETIME source_modified = {0,0};   // Time of latest modified for any of the source_files
        for (vector<wstring>::const_iterator s = args.inputs.begin(); s != args.inputs.end(); ++s)
        {
            TCHAR* filename = PathFindFileName(s->c_str());
            ParseDirectory(source_files, source_modified, s->substr(0, filename - s->c_str()), filename, args.options & OPT_RECURSIVE);
        }

        bool needs_rebuild = true;
        if (~args.options & OPT_FORCE_REBUILD)
        {
            // Unless we force a rebuild, we need to compare the source files against the destination files
            FileList dest_files;
            FILETIME dest_modified = {0,0};

            // Read the destination file to see what it contains
            HANDLE hFile = CreateFile(args.output.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE)
            {
                if (MegaFile::Load(hFile, dest_files))
                {
                    GetFileTime(hFile, NULL, NULL, &dest_modified);
                }
                CloseHandle(hFile);
            }

            // Now check the source files against the destination files
            if (dest_files.size() == source_files.size() &&              // counts are equal, AND
                CompareFileTime(&source_modified, &dest_modified) <= 0)  // no source file is newer
            {
                struct filesorter
                {
                    bool operator()(const FileInfo& l, const FileInfo& r) const {
                        return l.name < r.name;
                    }
                };

                // Compare the files' names and sizes
                sort(dest_files  .begin(), dest_files  .end(), filesorter());
                sort(source_files.begin(), source_files.end(), filesorter());

                needs_rebuild = false;
                for (FileList::const_iterator s = source_files.begin(), d = dest_files.begin(); d != dest_files.end(); ++d, ++s)
                {
                    if (d->name != s->name || d->size != s->size) {
                        // At least this file differs, we need to rebuild!
                        needs_rebuild = true;
                        break;
                    }
                }
            }
        }

        if (needs_rebuild)
        {
            if (~args.options & OPT_QUIET) {
                wcout << "Rebuilding " << args.output << "..." << endl; 
            }

            HANDLE hFile = CreateFile(args.output.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE)
            {
                MegaFile::Save(hFile, source_files);
                CloseHandle(hFile);
            }
        }
    }
    catch (exception& e)
    {
        cerr << e.what() << endl;
        return 1;
    }
    return 0;
}