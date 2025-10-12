#pragma once
#include "Core/LuminaMacros.h"
#include "Platform/GenericPlatform.h"
#include "Types/BitFlags.h"

namespace Lumina
{

    enum class EPendingCommandState : uint8
    {
        Idle,
        Recording,
        RenderPass,
        DynamicBufferWrites,
        AutomaticBarriers,
        HasBarriers,
    };
    
    ENUM_CLASS_FLAGS(EPendingCommandState);

    class FPendingCommandState
    {
    public:
        
        FORCEINLINE void AddPendingState(EPendingCommandState State) { PendingCommandState.SetFlag(State); }
        FORCEINLINE void ClearPendingState(EPendingCommandState State) { PendingCommandState.ClearFlag(State); }
        FORCEINLINE void Reset() { PendingCommandState.ClearAllFlags(); }
        

        FORCEINLINE NODISCARD bool IsInState(EPendingCommandState State) const { return PendingCommandState.IsFlagSet(State); }
        FORCEINLINE NODISCARD bool HasBarriers(EPendingCommandState State) const { return PendingCommandState.IsFlagSet(EPendingCommandState::HasBarriers); }
        FORCEINLINE NODISCARD bool IsRecording() const { return PendingCommandState.IsFlagSet(EPendingCommandState::Recording); }
        FORCEINLINE NODISCARD bool IsIdle() const { return PendingCommandState.IsFlagSet(EPendingCommandState::Idle); }
        
    protected:

        TBitFlags<EPendingCommandState> PendingCommandState;
    };
    
}
