#pragma once
#include "Containers/String.h"
#include "Module/API.h"
#include "Platform/GenericPlatform.h"


namespace Lumina::ObjectRename
{
    enum class EObjectRenameResult : uint8
    {
        Success,
        Failure,
        Exists,
    };
    
    LUMINA_API EObjectRenameResult RenameObject(const FString& OldPath, FString NewPath);
}
