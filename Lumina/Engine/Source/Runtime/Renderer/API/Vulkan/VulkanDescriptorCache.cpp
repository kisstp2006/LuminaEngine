#include "VulkanDescriptorCache.h"

#include "VulkanRenderContext.h"
#include "VulkanResources.h"
#include "Core/Math/Hash/Hash.h"
#include "Core/Profiler/Profile.h"

namespace Lumina
{
    FRHIBindingLayoutRef FVulkanDescriptorCache::GetOrCreateLayout(FVulkanDevice* Device, const FBindingLayoutDesc& Desc)
    {
        LUMINA_PROFILE_SCOPE();
        SIZE_T Hash = Hash::GetHash(Desc);

        if (LayoutMap.find(Hash) != LayoutMap.end())
        {
            return LayoutMap.at(Hash);
        }

        auto Layout = MakeRefCount<FVulkanBindingLayout>(Device, Desc);
        Layout->Bake();
        LayoutMap.insert_or_assign(Hash, Layout);

        return Layout;
    }
    
    void FVulkanDescriptorCache::ReleaseResources()
    {
        LayoutMap.clear();
    }
}
