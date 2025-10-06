#pragma once
#include "RenderContext.h"
#include "RHIFwd.h"
#include "RHIGlobals.h"


namespace Lumina
{
    template<typename TBufferStruct>
    class FRHITypedBufferRef : public FRHIBufferRef
    {
    public:

        static FRHITypedBufferRef CreateEmptyUniformBuffer(bool bKeepInitialState = false, uint16 NumVersions = 0)
        {
            FRHIBufferDesc Desc;
            Desc.DebugName = FString("TypedBuffer: ") + typeid(TBufferStruct).name();
            Desc.Size = sizeof(TBufferStruct);
            Desc.Stride = sizeof(TBufferStruct);
            Desc.Usage.SetFlag(EBufferUsageFlags::UniformBuffer);

            if (NumVersions > 0)
            {
                Desc.Usage.SetFlag(EBufferUsageFlags::Dynamic);
            }

            Desc.MaxVersions = NumVersions;
            Desc.InitialState = EResourceStates::ConstantBuffer;
            Desc.bKeepInitialState = bKeepInitialState;
            return FRHITypedBufferRef(GRenderContext->CreateBuffer(Desc));
        }
        
        static FRHITypedBufferRef CreateUniformBuffer(ICommandList* CommandList, const TBufferStruct& Value, bool bKeepInitialState = false, uint16 NumVersions = 0)
        {
            FRHIBufferDesc Desc;
            Desc.DebugName = "TypedBuffer: " + typeid(TBufferStruct).name();
            Desc.Size = sizeof(TBufferStruct);
            Desc.Stride = sizeof(TBufferStruct);
            Desc.Usage.SetFlag(EBufferUsageFlags::UniformBuffer);

            if (NumVersions > 0)
            {
                Desc.Usage.SetFlag(EBufferUsageFlags::Dynamic);
            }

            Desc.MaxVersions = NumVersions;
            Desc.InitialState = EResourceStates::ConstantBuffer;
            Desc.bKeepInitialState = bKeepInitialState;
            return FRHITypedBufferRef(GRenderContext->CreateBuffer(CommandList, &Value, Desc));
        }

        void UpdateBuffer(ICommandList* CommandList, const TBufferStruct& Value)
        {
            CommandList->WriteBuffer<TBufferStruct>(GetReference(), &Value);
        }
    };

    template<typename TVertexLayout>
    class FRHITypedVertexBuffer : public FRHIBufferRef
    {
    public:

        static FRHITypedVertexBuffer CreateEmpty(uint32 NumVertices)
        {
            FRHIBufferDesc Desc;
            Desc.DebugName = FString("TypedVertexBuffer: ") + typeid(TVertexLayout).name();
            Desc.Size = sizeof(TVertexLayout) * NumVertices;
            Desc.Usage.SetFlag(BUF_VertexBuffer);
            Desc.InitialState = EResourceStates::VertexBuffer;
            Desc.bKeepInitialState = true;
            return FRHITypedVertexBuffer(GRenderContext->CreateBuffer(Desc));
        }
        
        static FRHITypedVertexBuffer Create(ICommandList* CommandList, const TVertexLayout* VertexData, uint32 NumVertices)
        {
            FRHIBufferDesc Desc;
            Desc.DebugName = FString("TypedVertexBuffer: ") + typeid(TVertexLayout).name();
            Desc.Size = sizeof(TVertexLayout) * NumVertices;
            Desc.Usage.SetFlag(BUF_VertexBuffer);
            Desc.InitialState = EResourceStates::CopyDest;
            return FRHITypedVertexBuffer(GRenderContext->CreateBuffer(CommandList, VertexData, Desc));
        }

        void UpdateBuffer(ICommandList* CommandList, const TVertexLayout* VertexData, uint32 NumVertices)
        {
            CommandList->WriteBuffer(GetReference(), VertexData, 0, NumVertices);
        }
    };
}
