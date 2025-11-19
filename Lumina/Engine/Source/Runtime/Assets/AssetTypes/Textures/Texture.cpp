#include "Texture.h"

#include "Core/Engine/Engine.h"
#include "Core/Object/Class.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RenderManager.h"
#include "Renderer/RHIGlobals.h"

namespace Lumina
{
    void CTexture::Serialize(FArchive& Ar)
    {
        Super::Serialize(Ar);
        Ar << *TextureResource.get();
    }

    void CTexture::Serialize(IStructuredArchive::FSlot Slot)
    {
        CObject::Serialize(Slot);
    }

    void CTexture::PreLoad()
    {
        if (TextureResource == nullptr)
        {
            TextureResource = MakeUniquePtr<FTextureResource>();
        }
    }

    void CTexture::PostLoad()
    {
        TextureResource->RHIImage = GRenderContext->CreateImage(TextureResource->ImageDescription);

        FRHICommandListRef TransferCommandList = GRenderContext->CreateCommandList(FCommandListInfo::Compute());
        TransferCommandList->Open();
        
        for (uint8 i = 0; i < TextureResource->Mips.size(); ++i)
        {
            FTextureResource::FMip& Mip = TextureResource->Mips[i];
            const uint32 RowPitch = Mip.RowPitch;
            TransferCommandList->WriteImage(TextureResource->RHIImage, 0, i, Mip.Pixels.data(), RowPitch, 1);
        }

        TransferCommandList->Close();
        GRenderContext->ExecuteCommandList(TransferCommandList, ECommandQueue::Compute);   
    }
}
