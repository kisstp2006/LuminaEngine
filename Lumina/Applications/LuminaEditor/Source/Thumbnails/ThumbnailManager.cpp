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
        CubeMesh = NewObject<CStaticMesh>();
        
        TUniquePtr<FMeshResource> Resource = MakeUniquePtr<FMeshResource>();
        GenerateCube(Resource->Vertices, Resource->Indices);
        
        
        FGeometrySurface Surface;
        Surface.ID = "Mesh";
        Surface.IndexCount = Resource->Indices.size();
        Surface.StartIndex = 0;
        Surface.MaterialIndex = 0;
        Resource->GeometrySurfaces.push_back(Surface);

        CubeMesh->Materials.resize(1);
        CubeMesh->SetMeshResource(eastl::move(Resource));
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
