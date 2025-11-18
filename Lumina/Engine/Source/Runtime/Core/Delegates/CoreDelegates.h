#pragma once
#include "Delegate.h"


namespace Lumina
{
    struct FModuleInfo;

    struct FCoreDelegates
    {
        static TMulticastDelegate<void>		            OnPreEngineInit;
        static TMulticastDelegate<void>		            OnPostEngineInit;
        static TMulticastDelegate<void>                 PostWorldUnload;
        static TMulticastDelegate<void>		            OnPreEngineShutdown;
        static TMulticastDelegate<void, FModuleInfo*>   OnModuleLoaded;
        static TMulticastDelegate<void>                 OnModuleUnloaded;
    };
}
