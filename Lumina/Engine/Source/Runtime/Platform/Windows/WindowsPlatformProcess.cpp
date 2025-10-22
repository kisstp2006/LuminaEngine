#ifdef LE_PLATFORM_WINDOWS
#include "Containers/Array.h"
#include "Containers/String.h"
#include "Paths/Paths.h"
#include "Platform/Process/PlatformProcess.h"
#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <tchar.h>
#include <PathCch.h>  // For PathFindFileName
#include <psapi.h>
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "PathCch.lib")

namespace Lumina::Platform
{
    namespace
    {
        static TVector<FString> GDLLSearchPaths;
    }
    
    void* GetDLLHandle(const TCHAR* Filename)
    {
        FWString WideString = Filename;
        TVector<FString> SearchPaths = GDLLSearchPaths;

        
        return LoadLibraryWithSearchPaths(StringUtils::FromWideString(WideString), SearchPaths);
    }

    bool FreeDLLHandle(void* DLLHandle)
    {
        return FreeLibrary((HMODULE)DLLHandle);
    }

    void* GetDLLExport(void* DLLHandle, const TCHAR* ProcName)
    {
        FWString WideString = ProcName;

        return (void*)::GetProcAddress((HMODULE)DLLHandle, StringUtils::FromWideString(WideString).c_str());
    }

    void AddDLLDirectory(const FString& Directory)
    {
        GDLLSearchPaths.push_back(Directory);
    }

    void PushDLLDirectory(const TCHAR* Directory)
    {
        SetDllDirectory(Directory);
        
        GDLLSearchPaths.push_back(StringUtils::FromWideString(Directory));

        LOG_WARN("Pushing DLL Search Path: {0}", StringUtils::FromWideString(Directory));
    }

    void PopDLLDirectory()
    {
        GDLLSearchPaths.pop_back();

        if (GDLLSearchPaths.empty())
        {
            SetDllDirectory(L"");
        }
        else
        {
            SetDllDirectory(StringUtils::ToWideString(GDLLSearchPaths.back()).c_str());
        }
    }

    uint32 GetCurrentProcessID()
    {
        return 0;
    }

    uint32 GetCurrentCoreNumber()
    {
        return 0;
    }

    FString GetCurrentProcessPath()
    {
        char buffer[MAX_PATH];
        DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
        if (length == 0)
        {
            return "";
        }
        
        return FString(buffer, length);
    }

    int LaunchProcess(const TCHAR* URL, const TCHAR* Params, bool bLaunchDetached)
    {
        if (!URL)
        {
            return -1;
        }

        FWString URLString(URL);
        eastl::replace(URLString.begin(), URLString.end(), '/', '\\');
        
        STARTUPINFOW si{};
        PROCESS_INFORMATION pi{};

        si.cb = sizeof(si);

        if (Params)
        {
            URLString += L" ";
            URLString += Params;
        }

        DWORD creationFlags = 0;
        if (bLaunchDetached)
        {
            creationFlags |= DETACHED_PROCESS | CREATE_NEW_CONSOLE;
        }

        // CreateProcessW modifies the command line string, so make a writable buffer
        TVector<wchar_t> cmdBuffer(URLString.begin(), URLString.end());
        cmdBuffer.push_back(L'\0');

        BOOL result = CreateProcessW(
            nullptr,                  // lpApplicationName
            cmdBuffer.data(),         // lpCommandLine
            nullptr,                  // lpProcessAttributes
            nullptr,                  // lpThreadAttributes
            FALSE,                    // bInheritHandles
            creationFlags,            // dwCreationFlags
            nullptr,                  // lpEnvironment
            nullptr,                  // lpCurrentDirectory
            &si,                      // lpStartupInfo
            &pi                       // lpProcessInformation
        );

        if (!result)
        {
            return static_cast<int>(GetLastError());
        }

        // Optionally detach from our process
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);

        return 0; // Success
    }
    
    void LaunchURL(const TCHAR* URL)
    {
        ShellExecuteW(nullptr, TEXT("open"), URL, nullptr, nullptr, SW_SHOWNORMAL);
    }

    const TCHAR* ExecutableName(bool bRemoveExtension)
    {
        static TCHAR ExecutablePath[MAX_PATH];
    
        if (GetModuleFileName(NULL, ExecutablePath, MAX_PATH) == 0)
        {
            return nullptr;
        }

        TCHAR* ExecutableName = PathFindFileName(ExecutablePath);

        // If bRemoveExtension is true, remove the file extension
        if (bRemoveExtension)
        {
            PathCchRemoveExtension(ExecutableName, MAX_PATH);
        }

        return ExecutableName;
    }

    SIZE_T GetProcessMemoryUsageBytes()
    {
        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
        {
            return pmc.PrivateUsage;
        }
        return 0;
    }

    SIZE_T GetProcessMemoryUsageMegaBytes()
    {
        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
        {
            return pmc.PrivateUsage / (1024 * 1024);
        }
        return 0;
    }


    const TCHAR* BaseDir()
    {
        static TCHAR Buffer[MAX_PATH] = {};
        if (Buffer[0] == 0)
        {
            GetModuleFileNameW(nullptr, Buffer, MAX_PATH);
        }
        return Buffer;
    }

    FVoidFuncPtr LumGetProcAddress(void* Handle, const char* Procedure)
    {
        return reinterpret_cast<FVoidFuncPtr>(GetProcAddress((HMODULE)Handle, (LPCSTR)Procedure));
    }

    void* LoadLibraryWithSearchPaths(const FString& Filename, const TVector<FString>& SearchPaths)
    {
        FWString Wide = StringUtils::ToWideString(Filename);
        if (void* Handle = GetModuleHandleW(Wide.c_str()))
        {
            return Handle;
        }

        if (void* Handle = LoadLibraryW(Wide.c_str()))
        {
            return Handle;
        }

        for (const FString& Path : SearchPaths)
        {
            FString FullPath = Paths::Combine(Path.c_str(), Filename.c_str());
            if (Paths::Exists(FullPath))
            {
                FWString Wide = StringUtils::ToWideString(FullPath);
                if (void* Handle = LoadLibraryW(Wide.c_str()))
                {
                    return Handle;
                }
            }
        }

        return nullptr;
    }

    bool OpenFileDialogue(FString& OutFile, const char* Title, const char* Filter, const char* InitialDir)
    {
        OPENFILENAMEA ofn{};
        char Buffer[MAX_PATH] = {};

        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = nullptr;
        ofn.lpstrFilter = Filter;
        ofn.lpstrFile = Buffer;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = Title;
        if (InitialDir)
        {
            ofn.lpstrInitialDir = InitialDir;
        }
        else
        {
            ofn.lpstrInitialDir = GetCurrentProcessPath().c_str();
        }
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

        if (GetOpenFileNameA(&ofn))
        {
            OutFile = Buffer;
            eastl::replace(OutFile.begin(), OutFile.end(), '\\', '/');
            return true;
        }

        return false;
    }
}


#endif