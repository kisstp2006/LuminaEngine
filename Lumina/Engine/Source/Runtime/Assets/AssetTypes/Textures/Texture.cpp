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
        Ar << ImageDescription;
        Ar << Pixels;
    }

    void CTexture::Serialize(IStructuredArchive::FSlot Slot)
    {
        CObject::Serialize(Slot);
    }

    void CTexture::PostLoad()
    {
        if (ImageDescription.Extent.x == 0 || ImageDescription.Extent.x == 0)
        {
            LOG_ERROR("Image {} has an invalid size!: X: {} Y: {}, Image may be corrupt!", ImageDescription.DebugName, ImageDescription.Extent.x, ImageDescription.Extent.y);
            return;
        }
        
        RHIImage = GRenderContext->CreateImage(ImageDescription);

        const uint32 Width = ImageDescription.Extent.x;
        const uint32 RowPitch = Width * RHI::Format::BytesPerBlock(ImageDescription.Format);

        FRHICommandListRef TransferCommandList = GRenderContext->CreateCommandList(FCommandListInfo::Transfer());
        TransferCommandList->Open();
        TransferCommandList->WriteImage(RHIImage, 0, 0, Pixels.data(), RowPitch, 1);
        TransferCommandList->Close();
        GRenderContext->ExecuteCommandList(TransferCommandList, ECommandQueue::Transfer);
    }
}
