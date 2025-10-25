﻿



#if LE_PLATFORM_WINDOWS
#include <windows.h>

#include "Memory/Memory.h"


BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        {
            Lumina::Memory::InitializeThreadHeap();
        }
        break;

    case DLL_THREAD_ATTACH:
        {
            Lumina::Memory::InitializeThreadHeap();
        }
        break;

    case DLL_THREAD_DETACH:
        {
            Lumina::Memory::ShutdownThreadHeap();
        }
        break;

    case DLL_PROCESS_DETACH:
        {
            Lumina::Memory::ShutdownThreadHeap();
        }
        break;
    }

    return TRUE;
}




#endif