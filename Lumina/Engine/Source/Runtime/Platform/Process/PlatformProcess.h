#pragma once

#include "Containers/Array.h"
#include "Containers/String.h"
#include "Platform/GenericPlatform.h"
#include "Platform/Platform.h"


namespace Lumina::Platform
{
    
    void* GetDLLHandle(const TCHAR* Filename);
    bool FreeDLLHandle(void* DLLHandle);
    void* GetDLLExport(void* DLLHandle, const TCHAR* ProcName);
    void AddDLLDirectory(const FString& Directory);

    void PushDLLDirectory(const TCHAR* Directory);
    void PopDLLDirectory();

    uint32 GetCurrentProcessID();
    uint32 GetCurrentCoreNumber();


    

    const TCHAR* ExecutableName(bool bRemoveExtension = true);

    LUMINA_API SIZE_T GetProcessMemoryUsageBytes();
    
    const TCHAR* BaseDir();
    
    FVoidFuncPtr LumGetProcAddress(void* Handle, const char* Procedure);
    void* LoadLibraryWithSearchPaths(const FString& Filename, const TVector<FString>& SearchPaths);

    
    template<typename TCall>
    requires(std::is_pointer_v<TCall> && std::is_function_v<std::remove_pointer_t<TCall>>)
    TCall LumGetProcAddress(void* Handle, const char* Procedure)
    {
        return reinterpret_cast<TCall>(LumGetProcAddress(Handle, Procedure));
    }
}
