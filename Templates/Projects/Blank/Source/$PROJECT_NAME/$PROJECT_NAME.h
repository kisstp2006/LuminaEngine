#pragma once
#include "Core/Module/ModuleInterface.h"

// This is your core game module that the engine loads.
class F$PROJECT_NAME : public Lumina::IModuleInterface
{
    void StartupModule() override
    {
		// This code will execute after your module is loaded into memory.
    }

    void ShutdownModule() override
    {
        // This function may be called during shutdown to clean up your module.
    }
};