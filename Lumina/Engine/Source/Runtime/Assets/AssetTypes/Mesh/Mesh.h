#pragma once

#include "Core/Math/AABB.h"
#include "Core/Object/Object.h"
#include "Core/Object/ObjectHandleTyped.h"
#include "Renderer/MeshData.h"
#include "Mesh.generated.h"

namespace Lumina
{
    class CMaterialInterface;
    class CMaterialInstance;
    class CMaterial;
}

namespace Lumina
{
    LUM_CLASS()
    class LUMINA_API CMesh : public CObject
    {
        GENERATED_BODY()
        
        friend class CStaticMeshFactory;
        
    public:
        
        void Serialize(FArchive& Ar) override;
        void Serialize(IStructuredArchive::FSlot Slot) override;
        void PostLoad() override;

        bool IsReadyForRender() const;

        void GenerateBoundingBox();
        void GenerateGPUBuffers();

        uint32 GetNumMaterials() const { return (uint32)Materials.size(); }
        CMaterialInterface* GetMaterialAtSlot(SIZE_T Slot) const;
        void SetMaterialAtSlot(SIZE_T Slot, CMaterialInterface* NewMaterial);
        
        FORCEINLINE const FGeometrySurface& GetSurface(SIZE_T Slot) { return MeshResources.GeometrySurfaces[Slot]; }
        FORCEINLINE const FMeshResource& GetMeshResource() const { return MeshResources; }

        void SetMeshResource(const FMeshResource& NewResource);
        
        FORCEINLINE SIZE_T GetNumVertices() const { return MeshResources.Vertices.size(); }
        FORCEINLINE SIZE_T GetNumIndices() const { return MeshResources.Indices.size(); }
        FORCEINLINE const FMeshResource::FMeshBuffers& GetMeshBuffers() { return MeshResources.MeshBuffers; }
        FORCEINLINE const FRHIBufferRef& GetVertexBuffer() const { return MeshResources.MeshBuffers.VertexBuffer; }
        FORCEINLINE const FRHIBufferRef& GetIndexBuffer() const { return MeshResources.MeshBuffers.IndexBuffer; }
        FORCEINLINE const FRHIBufferRef& GetShadowIndexBuffer() const { return MeshResources.MeshBuffers.ShadowIndexBuffer; }
        FORCEINLINE const FAABB& GetAABB() const { return BoundingBox; }
        
        template<typename TCallable>
        void ForEachSurface(TCallable&& Lambda) const;


        
        
        LUM_PROPERTY(Editable, Category = "Materials")
        TVector<TObjectHandle<CMaterialInterface>> Materials;

        FAABB BoundingBox;
        
    private:
        
        FMeshResource MeshResources;
    };


    
    template <typename TCallable>
    void CMesh::ForEachSurface(TCallable&& Lambda) const
    {
        uint32 Count = 0;
        for (const FGeometrySurface& Surface : MeshResources.GeometrySurfaces)
        {
            std::forward<TCallable>(Lambda)(Surface, Count);
            ++Count;
        }
    }
}
