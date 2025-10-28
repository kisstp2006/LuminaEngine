#include "AssetEditorTool.h"
#include "Core/Object/Package/Package.h"
#include "Core/Object/Package/Thumbnail/PackageThumbnail.h"
#include "Paths/Paths.h"
#include "Platform/Filesystem/FileHelper.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RHIGlobals.h"
#include "Tools/UI/ImGui/ImGuiX.h"
#include "World/Scene/RenderScene/RenderScene.h"

namespace Lumina
{
    void FAssetEditorTool::OnInitialize()
    {
        if (ShouldGenerateThumbnailOnLoad())
        {
            GenerateThumbnailOnLoad();
        }
    }

    void FAssetEditorTool::Deinitialize(const FUpdateContext& UpdateContext)
    {
        FEditorTool::Deinitialize(UpdateContext);
    }

    FString FAssetEditorTool::GetToolName() const
    {
        return Asset->GetPathName();
    }

    void FAssetEditorTool::Update(const FUpdateContext& UpdateContext)
    {
        if (!bAssetLoadBroadcasted && Asset != nullptr)
        {
            OnAssetLoadFinished();
            bAssetLoadBroadcasted = true;
        }
    }

    void FAssetEditorTool::OnSave()
    {
        GenerateThumbnailOnLoad();
        
        FString FullPath = Paths::ResolveVirtualPath(Asset->GetPathName());
        Paths::AddPackageExtension(FullPath);
        
        if (CPackage::SavePackage(Asset->GetPackage(), FullPath.c_str()))
        {
            GetEngineSystem<FAssetRegistry>().AssetSaved(Asset);
            ImGuiX::Notifications::NotifySuccess("Successfully saved package: \"%s\"", Asset->GetPathName().c_str());
        }
        else
        {
            ImGuiX::Notifications::NotifyError("Failed to save package: \"%s\"", Asset->GetPathName().c_str());
        }
    }

    void FAssetEditorTool::GenerateThumbnailOnLoad()
    {
        if (!World)
        {
            return;
        }
        
        FRHIImageRef RenderTarget = World->GetRenderer()->GetRenderTarget();
    
        FRHICommandListRef CommandList = GRenderContext->CreateCommandList(FCommandListInfo::Graphics());
        CommandList->Open();
    
        FRHIStagingImageRef StagingImage = GRenderContext->CreateStagingImage(RenderTarget->GetDescription(), ERHIAccess::HostRead);
        CommandList->CopyImage(RenderTarget, FTextureSlice(), StagingImage, FTextureSlice());
    
        CommandList->Close();
        GRenderContext->ExecuteCommandList(CommandList);
    
        size_t RowPitch = 0;
        void* MappedMemory = GRenderContext->MapStagingTexture(StagingImage, FTextureSlice(), ERHIAccess::HostRead, &RowPitch);
        if (!MappedMemory)
        {
            return;
        }
    
        const uint32 SourceWidth  = RenderTarget->GetDescription().Extent.X;
        const uint32 SourceHeight = RenderTarget->GetDescription().Extent.Y;
        
        CPackage* AssetPackage = Asset->GetPackage();
    
        // Thumbnail dimensions
        constexpr uint32 ThumbWidth = 256;
        constexpr uint32 ThumbHeight = 256;
        
        AssetPackage->PackageThumbnail->ImageWidth = ThumbWidth;
        AssetPackage->PackageThumbnail->ImageHeight = ThumbHeight;

        constexpr size_t BytesPerPixel = 4;
        constexpr size_t TotalBytes = ThumbWidth * ThumbHeight * BytesPerPixel;
        
        AssetPackage->PackageThumbnail->ImageData.resize(TotalBytes);
        AssetPackage->PackageThumbnail->bDirty = true;
        
        const uint8* SourceData = static_cast<const uint8*>(MappedMemory);
        uint8* DestData = AssetPackage->PackageThumbnail->ImageData.data();
        
        // Downsample with bilinear filtering
        const float ScaleX = static_cast<float>(SourceWidth) / ThumbWidth;
        const float ScaleY = static_cast<float>(SourceHeight) / ThumbHeight;
        
        for (uint32 DestY = 0; DestY < ThumbHeight; ++DestY)
        {
            for (uint32 DestX = 0; DestX < ThumbWidth; ++DestX)
            {
                // Calculate source position with flipped Y
                const float SrcX = DestX * ScaleX;
                const float SrcY = (ThumbHeight - 1 - DestY) * ScaleY;
        
                // Get integer coordinates
                const uint32 X0 = static_cast<uint32>(SrcX);
                const uint32 Y0 = static_cast<uint32>(SrcY);
                const uint32 X1 = Math::Min(X0 + 1, SourceWidth - 1);
                const uint32 Y1 = Math::Min(Y0 + 1, SourceHeight - 1);
        
                // Get fractional parts for interpolation
                const float FracX = SrcX - X0;
                const float FracY = SrcY - Y0;
        
                // Sample 4 pixels
                const uint8* P00 = SourceData + (Y0 * RowPitch) + (X0 * BytesPerPixel);
                const uint8* P10 = SourceData + (Y0 * RowPitch) + (X1 * BytesPerPixel);
                const uint8* P01 = SourceData + (Y1 * RowPitch) + (X0 * BytesPerPixel);
                const uint8* P11 = SourceData + (Y1 * RowPitch) + (X1 * BytesPerPixel);
        
                // Write to destination
                uint8* DestPixel = DestData + (DestY * ThumbWidth + DestX) * BytesPerPixel;
        
                for (uint32 Channel = 0; Channel < BytesPerPixel; ++Channel)
                {
                    const float Top     = Math::Lerp(static_cast<float>(P00[Channel]), static_cast<float>(P10[Channel]), FracX);
                    const float Bottom  = Math::Lerp(static_cast<float>(P01[Channel]), static_cast<float>(P11[Channel]), FracX);
                    const float Result  = Math::Lerp(Top, Bottom, FracY);
            
                    DestPixel[Channel] = static_cast<uint8>(Result + 0.5f);
                }
            }
        }
        
        GRenderContext->UnMapStagingTexture(StagingImage);
    }
}
