#pragma once
#include "RenderGraphEvent.h"
#include "RenderGraphTypes.h"
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
            , PipelineType(Flags == ERGPassFlags::Compute ? EPipelineType::Compute : EPipelineType::Graphics)
            , Parameters(InDescriptor)
        {}
        

        virtual void Execute(ICommandList& CommandList) = 0;
        EPipelineType GetPipelineType() const { return PipelineType; }
        const FRGPassDescriptor* GetDescriptor() const { return Parameters; }
        const FRGEvent& GetEvent() const { return Event; }
    
    protected:
        
        FRGEvent Event;
        EPipelineType PipelineType;
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
