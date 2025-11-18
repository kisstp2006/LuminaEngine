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
#include "Renderer/RenderManager.h"
#include "Renderer/RenderTypes.h"
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
        
        FRHIImageDesc ImageDescription;
        ImageDescription.Format                 = EFormat::RGBA8_UNORM;
        ImageDescription.Extent                 = Import::Textures::ImportTexture(NewTexture->Pixels, RawPath, false);
        ImageDescription.Flags                  .SetFlag(EImageCreateFlags::ShaderResource);
        ImageDescription.NumMips                = RenderUtils::CalculateMipCount(ImageDescription.Extent.x, ImageDescription.Extent.y);
        ImageDescription.InitialState           = EResourceStates::ShaderResource;
        ImageDescription.bKeepInitialState      = true;
        NewTexture->ImageDescription            = ImageDescription;

        if (ImageDescription.Extent.x == 0 || ImageDescription.Extent.y == 0)
        {
            LOG_ERROR("Attempted to import an image with an invalid size: X: {} Y: {}", ImageDescription.Extent.x, ImageDescription.Extent.y);
        }
        
    }
}
