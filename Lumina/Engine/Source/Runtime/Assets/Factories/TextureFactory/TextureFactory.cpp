#include "TextureFactory.h"

#include "Assets/AssetHeader.h"
#include "Assets/AssetRegistry/AssetRegistry.h"
#include "Assets/AssetTypes/Textures/Texture.h"
#include "Core/Engine/Engine.h"
#include "Core/Object/Cast.h"
#include "Core/Object/Package/Package.h"
#include "Paths/Paths.h"
#include "Platform/Filesystem/FileHelper.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RendererUtils.h"
#include "Renderer/RenderTypes.h"
#include "Renderer/RHIGlobals.h"
#include "Renderer/RHIStaticStates.h"
#include "Tools/Import/ImportHelpers.h"

namespace Lumina
{
    CObject* CTextureFactory::CreateNew(const FName& Name, CPackage* Package)
    {
        return NewObject<CTexture>(Package, Name);
    }

    void CTextureFactory::TryImport(const FString& RawPath, const FString& DestinationPath)
    {
        FString FullPath = DestinationPath;
        Paths::AddPackageExtension(FullPath);
        FString VirtualPath = Paths::ConvertToVirtualPath(DestinationPath);
        FString FileName = Paths::FileName(DestinationPath, true);

        CTexture* NewTexture = Cast<CTexture>(TryCreateNew(DestinationPath));
        NewTexture->SetFlag(OF_NeedsPostLoad);

        NewTexture->TextureResource = MakeUniquePtr<FTextureResource>();

        TOptional<Import::Textures::FTextureImportResult> MaybeResult = Import::Textures::ImportTexture(RawPath, false);
        if (!MaybeResult.has_value())
        {
            return;
        }

        const Import::Textures::FTextureImportResult& Result = MaybeResult.value();
        
        TVector<uint8> Pixels = Result.Pixels;
        
        FRHIImageDesc ImageDescription;
        ImageDescription.Format                                 = Result.Format;
        ImageDescription.Extent                                 = Result.Dimensions;
        ImageDescription.Flags                                  .SetMultipleFlags(EImageCreateFlags::ShaderResource, EImageCreateFlags::Storage);
        ImageDescription.NumMips                                = (uint8)RenderUtils::CalculateMipCount(ImageDescription.Extent.x, ImageDescription.Extent.y);
        ImageDescription.InitialState                           = EResourceStates::ShaderResource;
        ImageDescription.bKeepInitialState                      = true;
        NewTexture->TextureResource->ImageDescription           = ImageDescription;

        NewTexture->TextureResource->Mips.resize(ImageDescription.NumMips);

        FRHIImageRef RHIImage = GRenderContext->CreateImage(ImageDescription);
        NewTexture->TextureResource->RHIImage = RHIImage;

        uint32 BytesPerBlock = RHI::Format::BytesPerBlock(ImageDescription.Format);
        const uint32 BaseWidth = ImageDescription.Extent.x;
        const uint32 BaseRowPitch = BaseWidth * BytesPerBlock;

        FRHIStagingImageRef StagingImage = GRenderContext->CreateStagingImage(RHIImage->GetDescription(), ERHIAccess::HostRead);

        // Generate the base mip.
        FRHICommandListRef CommandList = GRenderContext->CreateCommandList(FCommandListInfo::Compute());
        CommandList->Open();
        
        CommandList->WriteImage(RHIImage, 0, 0, Pixels.data(), BaseRowPitch, 1);
        CommandList->CopyImage(RHIImage, FTextureSlice(), StagingImage, FTextureSlice());

        // Intentionally starting at one to skip the first mip (base).
        for (uint8 i = 1; i < ImageDescription.NumMips; ++i)
        {
            LUMINA_PROFILE_SECTION_COLORED("Process Mip", tracy::Color::Yellow4);

            FRHIComputeShaderRef ComputeShader = FShaderLibrary::GetComputeShader("GenMipmaps.comp");
        
            FBindingSetDesc SetDesc;
            SetDesc.AddItem(FBindingSetItem::TextureSRV(0, RHIImage, nullptr, RHIImage->GetFormat(), FTextureSubresourceSet(i - 1, 1, 0, 1)));
            SetDesc.AddItem(FBindingSetItem::TextureUAV(1, RHIImage, RHIImage->GetFormat(), FTextureSubresourceSet(i, 1, 0, 1)));
            SetDesc.AddItem(FBindingSetItem::PushConstants(0, sizeof(glm::vec2)));

            FRHIBindingSetRef Set;
            FRHIBindingLayoutRef Layout;
            TBitFlags<ERHIShaderType> Visibility;
            Visibility.SetFlag(ERHIShaderType::Compute);
            GRenderContext->CreateBindingSetAndLayout(Visibility, 0, SetDesc, Layout, Set);
        
            FComputePipelineDesc PipelineDesc;
            PipelineDesc.AddBindingLayout(Layout);
            PipelineDesc.CS = ComputeShader;
            PipelineDesc.DebugName = "Gen Mip Maps";
        
            FRHIComputePipelineRef Pipeline = GRenderContext->CreateComputePipeline(PipelineDesc);
        
            FComputeState State;
            State.AddBindingSet(Set);
            State.SetPipeline(Pipeline);
        
            CommandList->SetComputeState(State);
            
            uint32 LevelWidth  = std::max(RenderUtils::GetMipDim(RHIImage->GetSizeX(), i), 1u);
            uint32 LevelHeight = std::max(RenderUtils::GetMipDim(RHIImage->GetSizeY(), i), 1u);
            
            glm::vec2 TexelSize = glm::vec2(1.0f / float(LevelWidth), 1.0f / float(LevelHeight));
        
            uint32_t GroupsX = RenderUtils::GetGroupCount(LevelWidth, 32);
            uint32_t GroupsY = RenderUtils::GetGroupCount(LevelHeight, 32);
            uint32_t GroupsZ = 1;
        
            CommandList->SetPushConstants(&TexelSize, sizeof(glm::vec2));
            CommandList->Dispatch(GroupsX, GroupsY, GroupsZ);

            CommandList->CopyImage(RHIImage, FTextureSlice().SetMipLevel(i), StagingImage, FTextureSlice().SetMipLevel(i));
        }
        
        

        CommandList->Close();
        GRenderContext->ExecuteCommandList(CommandList, ECommandQueue::Compute);
        
        for (uint8 i = 0; i < ImageDescription.NumMips; ++i)
        {
            FTextureResource::FMip& Mip = NewTexture->TextureResource->Mips[i];
            
            size_t RowPitch = 0;
            void* MappedMemory = GRenderContext->MapStagingTexture(StagingImage, FTextureSlice().SetMipLevel(i), ERHIAccess::HostRead, &RowPitch);
            if (!MappedMemory)
            {
                continue;
            }

            const uint32 Width  = RenderUtils::GetMipDim(RHIImage->GetDescription().Extent.x, i);
            const uint32 Height = RenderUtils::GetMipDim(RHIImage->GetDescription().Extent.y, i);

            Mip.Width       = Width;
            Mip.Height      = Height;
            Mip.RowPitch    = RowPitch;
            Mip.Depth       = 1;
            Mip.SlicePitch  = 1;
            

            Mip.Pixels.resize(RowPitch * Height);
            uint8* Dst = Mip.Pixels.data();
            uint8* Src = (uint8*)MappedMemory;

            for (uint32 Y = 0; Y < Height; ++Y)
            {
                Memory::Memcpy(Dst + Y * RowPitch, Src + Y * RowPitch, RowPitch);
            }
        }

        GRenderContext->UnMapStagingTexture(StagingImage);
        
        
        if (ImageDescription.Extent.x == 0 || ImageDescription.Extent.y == 0)
        {
            LOG_ERROR("Attempted to import an image with an invalid size: X: {} Y: {}", ImageDescription.Extent.x, ImageDescription.Extent.y);
        }
    }
}
