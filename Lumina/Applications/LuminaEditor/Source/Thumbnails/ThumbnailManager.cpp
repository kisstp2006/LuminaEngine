#include "ThumbnailManager.h"
#include "Assets/AssetTypes/Mesh/StaticMesh/StaticMesh.h"
#include "Core/Object/Package/Package.h"
#include "Core/Object/Package/Thumbnail/PackageThumbnail.h"
#include "Paths/Paths.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RHIGlobals.h"
#include "Renderer/RHIIncl.h"
#include "TaskSystem/TaskSystem.h"
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

    void CThumbnailManager::GetOrLoadThumbnailsForPackage(const FString& PackagePath)
    {
        FString ActualPackagePath = PackagePath;
        if (!Paths::HasExtension(ActualPackagePath, "lasset"))
        {
            Paths::AddPackageExtension(ActualPackagePath);
        }
        if (CPackage* Package = CPackage::LoadPackage(ActualPackagePath))
        {
            TSharedPtr<FPackageThumbnail> Thumbnail = Package->GetPackageThumbnail();
        
            if (!Thumbnail->bDirty || Thumbnail->ImageData.empty())
            {
                return;
            }
        
        
            FRHIImageDesc ImageDesc;
            ImageDesc.Dimension = EImageDimension::Texture2D;
            ImageDesc.Extent = {256, 256};
            ImageDesc.Format = EFormat::BGRA8_UNORM;
            ImageDesc.Flags.SetFlag(EImageCreateFlags::ShaderResource);
            ImageDesc.InitialState = EResourceStates::ShaderResource;
            ImageDesc.bKeepInitialState = true;
            FRHIImageRef Image = GRenderContext->CreateImage(ImageDesc);
        
            FRHICommandListRef CommandList = GRenderContext->CreateCommandList(FCommandListInfo::Graphics());
            CommandList->Open();
        
            constexpr SIZE_T BytesPerPixel = 4;
            constexpr SIZE_T RowBytes = 256 * BytesPerPixel;

            if (Thumbnail->bFlip)
            {
                TVector<uint8> FlippedData(Thumbnail->ImageData.size());
                for (uint32 Y = 0; Y < 256; ++Y)
                {
                    const uint32 FlippedY = 255 - Y;
                    memcpy(FlippedData.data() + (FlippedY * RowBytes), Thumbnail->ImageData.data() + (Y * RowBytes), RowBytes);
                }
        
                constexpr SIZE_T RowPitch = RowBytes;
                constexpr SIZE_T DepthPitch = 0;
        
                CommandList->WriteImage(Image, 0, 0, FlippedData.data(), RowPitch, DepthPitch);
            }
            else
            {
                constexpr SIZE_T RowPitch = RowBytes;
                constexpr SIZE_T DepthPitch = 0;
        
                CommandList->WriteImage(Image, 0, 0, Thumbnail->ImageData.data(), RowPitch, DepthPitch);
            }

            
            CommandList->Close();
        
            GRenderContext->ExecuteCommandList(CommandList);
        
            Thumbnail->LoadedImage = Image;
            Thumbnail->bDirty = false;
        }
    }
}
