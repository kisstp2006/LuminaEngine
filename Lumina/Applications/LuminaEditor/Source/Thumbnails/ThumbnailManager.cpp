#include "ThumbnailManager.h"
#include "Assets/AssetTypes/Mesh/StaticMesh/StaticMesh.h"
#include "Renderer/RHIIncl.h"
#include "World/Scene/RenderScene/SceneMeshes.h"


namespace Lumina
{

    CThumbnailManager* CThumbnailManager::ThumbnailManagerSingleton = nullptr;

    CThumbnailManager::CThumbnailManager()
    {
    }

    void CThumbnailManager::Initialize()
    {
        {
            TUniquePtr<FMeshResource> Resource = MakeUniquePtr<FMeshResource>();
            PrimitiveMeshes::GenerateCube(Resource->Vertices, Resource->Indices);
            
            FGeometrySurface Surface;
            Surface.ID = "CubeMesh";
            Surface.IndexCount = (uint32)Resource->Indices.size();
            Surface.StartIndex = 0;
            Surface.MaterialIndex = 0;
            Resource->GeometrySurfaces.push_back(Surface);

            CubeMesh = NewObject<CStaticMesh>();
            CubeMesh->Materials.resize(1);
            CubeMesh->SetMeshResource(eastl::move(Resource));
        }

        {
            TUniquePtr<FMeshResource> Resource = MakeUniquePtr<FMeshResource>();
            PrimitiveMeshes::GenerateSphere(Resource->Vertices, Resource->Indices);
            
            FGeometrySurface Surface;
            Surface.ID = "SphereMesh";
            Surface.IndexCount = (uint32)Resource->Indices.size();
            Surface.StartIndex = 0;
            Surface.MaterialIndex = 0;
            Resource->GeometrySurfaces.push_back(Surface);

            SphereMesh = NewObject<CStaticMesh>();
            SphereMesh->Materials.resize(1);
            SphereMesh->SetMeshResource(eastl::move(Resource));
        }

        {
            TUniquePtr<FMeshResource> Resource = MakeUniquePtr<FMeshResource>();
            PrimitiveMeshes::GeneratePlane(Resource->Vertices, Resource->Indices);
            
            FGeometrySurface Surface;
            Surface.ID = "PlaneMesh";
            Surface.IndexCount = (uint32)Resource->Indices.size();
            Surface.StartIndex = 0;
            Surface.MaterialIndex = 0;
            Resource->GeometrySurfaces.push_back(Surface);

            PlaneMesh = NewObject<CStaticMesh>();
            PlaneMesh->Materials.resize(1);
            PlaneMesh->SetMeshResource(eastl::move(Resource));
        }
    }

    CThumbnailManager& CThumbnailManager::Get()
    {
        if (ThumbnailManagerSingleton == nullptr)
        {
            ThumbnailManagerSingleton = NewObject<CThumbnailManager>();
            ThumbnailManagerSingleton->Initialize();
        }
        
        return *ThumbnailManagerSingleton;
    }

    void CThumbnailManager::GetOrLoadThumbnailsForPackages(TSpan<FString> Packages)
    {
        for (const FString& PackagePath : Packages)
        {
            FName PackageName = PackagePath;
            
        }
    }
}
