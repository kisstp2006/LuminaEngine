#pragma once
#include "Log/Log.h"


namespace Lumina
{
    class CObject;
}

namespace Lumina
{
    struct FDeleteAssetTransaction
    {
        CObject* Asset;

        void Undo()
        {
            LOG_INFO("Undo");
        }

        void Redo()
        {
            LOG_INFO("Redo");
        }
    };
}
