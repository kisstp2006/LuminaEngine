#pragma once

#include "Core/Object/Object.h"
#include "Memory/RefCounted.h"
#include "Renderer/RenderResource.h"
#include "Texture.generated.h"
#include "Memory/SmartPtr.h"
#include "Renderer/TextureData.h"

namespace Lumina
{

    LUM_CLASS()
    class LUMINA_API CTexture : public CObject
    {
        GENERATED_BODY()
        
    public:

        
        void Serialize(FArchive& Ar) override;
        void Serialize(IStructuredArchive::FSlot Slot) override;
        void PreLoad() override;
        void PostLoad() override;
        bool IsAsset() const override { return true; }


        FORCEINLINE FRHIImage* GetRHIRef() const { return TextureResource.get() ? TextureResource->RHIImage : nullptr; }
        FTextureResource* GetTextureResource() const { return TextureResource.get(); }
        uint8 GetNumMips() const { return TextureResource.get() ? TextureResource->Mips.size() : 0u; }
        
        
        TUniquePtr<FTextureResource> TextureResource;
    };
}
