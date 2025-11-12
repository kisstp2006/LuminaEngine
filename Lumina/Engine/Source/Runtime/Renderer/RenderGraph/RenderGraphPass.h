#pragma once
#include "RenderGraphEvent.h"
#include "RenderGraphTypes.h"
#include "Renderer/RenderTypes.h"
#include "Renderer/RHIFwd.h"


namespace Lumina
{
    struct FRGContext;
}

namespace Lumina
{
    class FRGPassDescriptor;
    enum class ERGPassFlags : uint16;
    enum class ERHIPipeline : uint8;
}

namespace Lumina
{
    class LUMINA_API FRenderGraphPass
    {
    public:

        FRenderGraphPass(FRGEvent&& InEvent, ERGPassFlags Flags, const FRGPassDescriptor* InDescriptor)
            : Event(std::move(InEvent))
            , QueueType(Flags == ERGPassFlags::Compute ? ECommandQueue::Compute : ECommandQueue::Graphics)
            , Parameters(InDescriptor)
        {}
        

        virtual void Execute(ICommandList& CommandList) = 0;
        ECommandQueue GetQueueType() const { return QueueType; }
        const FRGPassDescriptor* GetDescriptor() const { return Parameters; }
        const FRGEvent& GetEvent() const { return Event; }
    
    protected:
        
        FRGEvent Event;
        ECommandQueue QueueType;
        const FRGPassDescriptor* Parameters;
    };



    template <typename ExecutorType>
    requires(sizeof(ExecutorType) <= 1024)
    class TRGPass : public FRenderGraphPass
    {
    public:
        
        TRGPass(FRGEvent&& InEvent, ERGPassFlags Flags, const FRGPassDescriptor* InParams, ExecutorType&& Executor)
            : FRenderGraphPass(std::move(InEvent), Flags, InParams)
            , ExecutionLambda(std::move(Executor))
        {}


        void Execute(ICommandList& CommandList) override
        {
            ExecutionLambda(CommandList);
        }


    private:
        
        ExecutorType ExecutionLambda;
    };
}
