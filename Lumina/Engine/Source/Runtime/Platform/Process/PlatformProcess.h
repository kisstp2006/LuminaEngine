#pragma once

#include "Containers/Array.h"
#include "Containers/String.h"
#include "Platform/GenericPlatform.h"
#include "Platform/Platform.h"


namespace Lumina::Platform
{
    struct FFileDialogResult
    {
        bool    bSuccess = false;
        FString FilePath;
    };
    
    LUMINA_API void* GetDLLHandle(const TCHAR* Filename);
    LUMINA_API bool FreeDLLHandle(void* DLLHandle);
    LUMINA_API void* GetDLLExport(void* DLLHandle, const TCHAR* ProcName);
    LUMINA_API void AddDLLDirectory(const FString& Directory);

    LUMINA_API void PushDLLDirectory(const TCHAR* Directory);
    LUMINA_API void PopDLLDirectory();

    LUMINA_API uint32 GetCurrentCoreNumber();
    LUMINA_API FString GetCurrentProcessPath();

	LUMINA_API int LaunchProcess(const TCHAR* URL, const TCHAR* Params = nullptr, bool bLaunchDetached = true);
    LUMINA_API void LaunchURL(const TCHAR* URL);

    LUMINA_API const TCHAR* ExecutableName(bool bRemoveExtension = true);

    LUMINA_API SIZE_T GetProcessMemoryUsageBytes();
    LUMINA_API SIZE_T GetProcessMemoryUsageMegaBytes();
    
    LUMINA_API const TCHAR* BaseDir();
    
    LUMINA_API FVoidFuncPtr LumGetProcAddress(void* Handle, const char* Procedure);
    LUMINA_API void* LoadLibraryWithSearchPaths(const FString& Filename, const TVector<FString>& SearchPaths);

    LUMINA_API bool OpenFileDialogue(FString& OutFile, const char* Title = "Open File", const char* Filter = "*.*", const char* InitialDir = nullptr);
    
    
    template<typename TCall>
    requires(std::is_pointer_v<TCall> && std::is_function_v<std::remove_pointer_t<TCall>>)
    TCall LumGetProcAddress(void* Handle, const char* Procedure)
    {
        return reinterpret_cast<TCall>(LumGetProcAddress(Handle, Procedure));
    }
}
