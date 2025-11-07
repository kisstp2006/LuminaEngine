#include "Mesh.h"

#include "Renderer/RenderContext.h"
#include "Assets/AssetTypes/Material/Material.h"
#include "assets/assettypes/material/materialinstance.h"
#include "Core/Object/Cast.h"
#include "Renderer/RHIGlobals.h"
#include "Renderer/TypedBuffer.h"


namespace Lumina
{
    void CMesh::Serialize(FArchive& Ar)
    {
        Super::Serialize(Ar);

        if (!MeshResources)
        {
            MeshResources = MakeUniquePtr<FMeshResource>();
        }
        
        Ar << *MeshResources;
    }

    void CMesh::Serialize(IStructuredArchive::FSlot Slot)
    {
        CObject::Serialize(Slot);
    }

    void CMesh::PostLoad()
    {
        GenerateBoundingBox();
        GenerateGPUBuffers();
    }

    CMaterialInterface* CMesh::GetMaterialAtSlot(SIZE_T Slot) const
    {
        return Materials.empty() ? nullptr : Materials[Slot].Get();
    }

    void CMesh::SetMaterialAtSlot(SIZE_T Slot, CMaterialInterface* NewMaterial)
    {
        if (Materials.size() <= Slot)
        {
            Materials.push_back(NewMaterial);
        }
        else
        {
            Materials[Slot] = NewMaterial;
        }  
    }

    void CMesh::SetMeshResource(TUniquePtr<FMeshResource>&& NewResource)
    {
        MeshResources = eastl::move(NewResource);
        GenerateBoundingBox();
        GenerateGPUBuffers();
    }

    bool CMesh::IsReadyForRender() const
    {
        LUMINA_PROFILE_SCOPE();

        if (HasAnyFlag(OF_NeedsLoad))
        {
            return false;
        }

        for (CMaterialInterface* Material : Materials)
        {
            if (Material == nullptr)
            {
                return false;
            }

            if (Material->IsReadyForRender() == false)
            {
                return false;
            }
        }

        return !Materials.empty();
    }

    void CMesh::GenerateBoundingBox()
    {
        BoundingBox.Min = { FLT_MAX, FLT_MAX, FLT_MAX };
        BoundingBox.Max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
        
        for (SIZE_T i = 0; i < MeshResources->Vertices.size(); ++i)
        {
            const FVertex& Vertex = MeshResources->Vertices[i];
            
            BoundingBox.Min.x = glm::min(Vertex.Position.x, BoundingBox.Min.x);
            BoundingBox.Min.y = glm::min(Vertex.Position.y, BoundingBox.Min.y);
            BoundingBox.Min.z = glm::min(Vertex.Position.z, BoundingBox.Min.z);
        
            BoundingBox.Max.x = glm::max(Vertex.Position.x, BoundingBox.Max.x);
            BoundingBox.Max.y = glm::max(Vertex.Position.y, BoundingBox.Max.y);
            BoundingBox.Max.z = glm::max(Vertex.Position.z, BoundingBox.Max.z);
        }
    }

    void CMesh::GenerateGPUBuffers()
    {
        auto Info = FCommandListInfo::Graphics();
        Info.UploadChunkSize = 10'024;
        FRHICommandListRef CommandList = GRenderContext->CreateCommandList(Info);
        CommandList->Open();
        
        MeshResources->MeshBuffers.VertexBuffer = FRHITypedVertexBuffer<FVertex>::Create(CommandList, MeshResources->Vertices.data(), MeshResources->Vertices.size());
        CommandList->SetPermanentBufferState(MeshResources->MeshBuffers.VertexBuffer, EResourceStates::VertexBuffer);
        

        FRHIBufferDesc IndexBufferDesc;
        IndexBufferDesc.Size = sizeof(uint32) * MeshResources->Indices.size();
        IndexBufferDesc.Usage.SetMultipleFlags(BUF_IndexBuffer);
        IndexBufferDesc.InitialState = EResourceStates::CopyDest;
        IndexBufferDesc.DebugName = GetName().ToString() + "Index Buffer";
        MeshResources->MeshBuffers.IndexBuffer = GRenderContext->CreateBuffer(IndexBufferDesc);

        CommandList->BeginTrackingBufferState(MeshResources->MeshBuffers.IndexBuffer, EResourceStates::CopyDest);
        CommandList->WriteBuffer(MeshResources->MeshBuffers.IndexBuffer, MeshResources->Indices.data(), 0, MeshResources->Indices.size() * sizeof(uint32));
        CommandList->SetPermanentBufferState(MeshResources->MeshBuffers.IndexBuffer, EResourceStates::IndexBuffer);
        

        if (!MeshResources->ShadowIndices.empty())
        {
            FRHIBufferDesc ShadowIndexBufferDesc;
            ShadowIndexBufferDesc.Size = sizeof(uint32) * MeshResources->ShadowIndices.size();
            ShadowIndexBufferDesc.Stride = sizeof(uint32);
            ShadowIndexBufferDesc.Usage.SetMultipleFlags(BUF_IndexBuffer);
            ShadowIndexBufferDesc.InitialState = EResourceStates::IndexBuffer;
            ShadowIndexBufferDesc.bKeepInitialState = true;
            ShadowIndexBufferDesc.DebugName = GetName().ToString() + "Index Buffer";
            MeshResources->MeshBuffers.ShadowIndexBuffer = GRenderContext->CreateBuffer(ShadowIndexBufferDesc);
            CommandList->WriteBuffer(MeshResources->MeshBuffers.ShadowIndexBuffer, MeshResources->ShadowIndices.data(), 0, MeshResources->ShadowIndices.size() * sizeof(uint32));
            CommandList->SetPermanentBufferState(MeshResources->MeshBuffers.ShadowIndexBuffer, EResourceStates::IndexBuffer);
        }
        else
        {
            MeshResources->MeshBuffers.ShadowIndexBuffer = MeshResources->MeshBuffers.IndexBuffer;
        }

        
        CommandList->Close();
        GRenderContext->ExecuteCommandList(CommandList, ECommandQueue::Graphics);
    }
}
