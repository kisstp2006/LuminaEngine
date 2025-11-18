#pragma once

#include "Renderer/RenderResource.h"
#include "Core/LuminaMacros.h"
#include "Renderer/RHIFwd.h"


namespace Lumina
{
    class FRGImage;
    class FRGBuffer;
    class FRGResource;
}


namespace Lumina
{

    enum class ERGExecutionFlags : uint8
    {
        None            = 0,

        /** Any async is a promise to the graph that this pass is operating in a dependency-free environment from the time of scheduling onward */
        Async        = 1 << 0,
    };

    ENUM_CLASS_FLAGS(ERGExecutionFlags);
    
    class FRGPassDescriptor
    {
        friend class FRGPassAnalyzer;
        
    public:
        
        void AddBinding(FRHIBindingSet* Binding);
        void AddRawWrite(const IRHIResource* InResource);
        void AddRawRead(const IRHIResource* InResource);

        void SetFlag(ERGExecutionFlags Flag) { EnumAddFlags(ExecutionFlags, Flag); }
        bool HasAnyFlag(ERGExecutionFlags Flag) const { return EnumHasAnyFlags(ExecutionFlags, Flag); }
        bool HasAllFlags(ERGExecutionFlags Flags) const { return EnumHasAllFlags(ExecutionFlags, Flags); }
        
    private:

        ERGExecutionFlags ExecutionFlags = ERGExecutionFlags::None;
        TFixedVector<FRHIBindingSet*, 2> Bindings;
        TFixedVector<const IRHIResource*, 4> RawWrites;
        TFixedVector<const IRHIResource*, 4> RawReads;

    };
}
