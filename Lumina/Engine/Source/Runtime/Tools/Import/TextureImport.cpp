
#include "ImportHelpers.h"
#include "Paths/Paths.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RenderResource.h"

#define STBI_MALLOC(Sz) Lumina::Memory::Malloc(Sz)
#define STBI_REALLOC(p, newsz) Lumina::Memory::Realloc(p, newsz)
#define STBI_FREE(p) Lumina::Memory::Free(p)

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"

namespace Lumina::Import::Textures
{
    TOptional<FTextureImportResult> ImportTexture(const FString& RawFilePath, bool bFlipVertical)
    {
        FTextureImportResult Result = {};
        
        stbi_set_flip_vertically_on_load(bFlipVertical);
        
        int x, y, channels;
        
        // First, check if it's an HDR image (floating point)
        if (stbi_is_hdr(RawFilePath.c_str()))
        {
            float* data = stbi_loadf(RawFilePath.c_str(), &x, &y, &channels, 0);
            if (data == nullptr)
            {
                LOG_WARN("Failed to load HDR image: {0}", RawFilePath);
                return eastl::nullopt;
            }
            
            switch (channels)
            {
                case 1: Result.Format = EFormat::R32_FLOAT; break;
                case 2: Result.Format = EFormat::RG32_FLOAT; break;
                case 3: Result.Format = EFormat::RGB32_FLOAT; break;
                case 4: Result.Format = EFormat::RGBA32_FLOAT; break;
                default:
                    stbi_image_free(data);
                    LOG_WARN("Unsupported channel count for HDR: {0}", channels);
                    return Result;
            }
            
            size_t dataSize = static_cast<size_t>(x) * y * channels * sizeof(float);
            Result.Pixels.assign(reinterpret_cast<uint8*>(data), reinterpret_cast<uint8*>(data) + dataSize);
            stbi_image_free(data);
            
            Result.Dimensions = {x, y};
            return Result;
        }
        
        // Check if it's a 16-bit image
        if (stbi_is_16_bit(RawFilePath.c_str()))
        {
            uint16* data = stbi_load_16(RawFilePath.c_str(), &x, &y, &channels, 0);
            if (data == nullptr)
            {
                LOG_WARN("Failed to load 16-bit image: {0}", RawFilePath);
                return eastl::nullopt;
            }
            
            switch (channels)
            {
                case 1: Result.Format = EFormat::R16_UNORM; break;
                case 2: Result.Format = EFormat::RG16_UNORM; break;
                case 4: Result.Format = EFormat::RGBA16_UNORM; break;
                case 3: 
                    // RGB16 not commonly supported, convert to RGBA16
                    {
                        Result.Format = EFormat::RGBA16_UNORM;
                        TVector<uint16> rgba16Data(x * y * 4);
                        for (int i = 0; i < x * y; ++i)
                        {
                            rgba16Data[i * 4 + 0] = data[i * 3 + 0];
                            rgba16Data[i * 4 + 1] = data[i * 3 + 1];
                            rgba16Data[i * 4 + 2] = data[i * 3 + 2];
                            rgba16Data[i * 4 + 3] = 0xFFFF; // Max value for 16-bit
                        }
                        size_t dataSize = rgba16Data.size() * sizeof(uint16);
                        Result.Pixels.assign(reinterpret_cast<uint8*>(rgba16Data.data()), reinterpret_cast<uint8*>(rgba16Data.data()) + dataSize);
                    }
                    break;
                default:
                    stbi_image_free(data);
                    LOG_WARN("Unsupported channel count for 16-bit: {0}", channels);
                    return eastl::nullopt;
            }
            
            if (channels != 3) // If we didn't do the RGB->RGBA conversion above
            {
                size_t dataSize = static_cast<size_t>(x) * y * channels * sizeof(uint16);
                Result.Pixels.assign(reinterpret_cast<uint8*>(data), reinterpret_cast<uint8*>(data) + dataSize);
            }
            
            stbi_image_free(data);
            Result.Dimensions = {x, y};
            return Result;
        }
        
        // Standard 8-bit image
        uint8* data = stbi_load(RawFilePath.c_str(), &x, &y, &channels, 0);
        if (data == nullptr)
        {
            LOG_WARN("Failed to load 8-bit image: {0}", RawFilePath);
            return eastl::nullopt;
        }
        
        // Check if image is likely sRGB (common for color textures)
        // Might want to make this configurable or check file extension
        bool bIsSRGB = false;//IsSRGBTexture(RawFilePath); // Helper function
        
        
        switch (channels)
        {
            case 1: Result.Format = EFormat::R8_UNORM; break;
            case 2: Result.Format = EFormat::RG8_UNORM; break;
            case 4: Result.Format = bIsSRGB ? EFormat::SRGBA8_UNORM : EFormat::RGBA8_UNORM; break;
            case 3:
                // RGB8 not commonly supported, convert to RGBA8
                {
                    Result.Format = bIsSRGB ? EFormat::SRGBA8_UNORM : EFormat::RGBA8_UNORM;
                    TVector<uint8> rgba8Data(x * y * 4);
                    for (int i = 0; i < x * y; ++i)
                    {
                        rgba8Data[i * 4 + 0] = data[i * 3 + 0];
                        rgba8Data[i * 4 + 1] = data[i * 3 + 1];
                        rgba8Data[i * 4 + 2] = data[i * 3 + 2];
                        rgba8Data[i * 4 + 3] = 0xFF;
                    }
                    Result.Pixels = eastl::move(rgba8Data);
                }
                break;
            default:
                stbi_image_free(data);
                LOG_WARN("Unsupported channel count: {0}", channels);
                return eastl::nullopt;
        }
        
        if (channels != 3) // If we didn't do the RGB->RGBA conversion above
        {
            Result.Pixels.assign(data, data + static_cast<size_t>(x) * y * channels);
        }
        
        stbi_image_free(data);
        Result.Dimensions = {x, y};
        return Result;
    }

    FRHIImageRef CreateTextureFromImport(IRenderContext* RenderContext, const FString& RawFilePath, bool bFlipVerticalOnLoad)
    {
        LUMINA_PROFILE_SCOPE();

        TOptional<FTextureImportResult> MaybeResult = ImportTexture(RawFilePath, bFlipVerticalOnLoad);
        if (!MaybeResult.has_value())
        {
            return nullptr;
        }

        const FTextureImportResult& Result = MaybeResult.value();
        
        
        TVector<uint8> Pixels = Result.Pixels;
        
        FRHIImageDesc ImageDescription;
        ImageDescription.Format = Result.Format;
        ImageDescription.Extent = Result.Dimensions;
        ImageDescription.Flags.SetFlag(EImageCreateFlags::ShaderResource);
        ImageDescription.NumMips = 1;
        ImageDescription.DebugName = Paths::FileName(RawFilePath, true);
        ImageDescription.InitialState = EResourceStates::ShaderResource;
        ImageDescription.bKeepInitialState = true;
        
        FRHIImageRef ReturnImage = RenderContext->CreateImage(ImageDescription);

        const uint32 Width = ImageDescription.Extent.x;
        const uint32 RowPitch = Width * RHI::Format::BytesPerBlock(Result.Format);

        FRHICommandListRef TransferCommandList = RenderContext->CreateCommandList(FCommandListInfo::Transfer());
        TransferCommandList->Open();
        TransferCommandList->WriteImage(ReturnImage, 0, 0, Pixels.data(), RowPitch, 0);
        TransferCommandList->Close();
        RenderContext->ExecuteCommandList(TransferCommandList, ECommandQueue::Transfer);

        return ReturnImage;
    }
}
