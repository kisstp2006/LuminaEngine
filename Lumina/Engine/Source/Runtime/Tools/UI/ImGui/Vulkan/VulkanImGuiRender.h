#pragma once

#include <volk/volk.h>
#include <vk_mem_alloc.h>
#include "Tools/UI/ImGui/ImGuiRenderer.h"

namespace Lumina
{
    class FVulkanRenderContext;
    class FUpdateContext;

    class FVulkanImGuiRender: public IImGuiRenderer
    {
    public:
        
        enum class ETextureState : uint8
        {
            Empty,
            Loading,
            Ready,
        };
        
        struct FEntry
        {
            FName Name;
            uint64 Index;
            std::atomic_uint64_t LastUseFrame{0};
            std::atomic<ETextureState> State = ETextureState::Empty;
            FRHIImageRef RHIImage;
            ImTextureRef ImTexture;
        };
        
        void Initialize() override;
        void Deinitialize() override;
        
        void OnStartFrame(const FUpdateContext& UpdateContext) override;
        void OnEndFrame(const FUpdateContext& UpdateContext, FRenderGraph& RenderGraph) override;

        void DrawRenderDebugInformationWindow(bool* bOpen, const FUpdateContext& Context) override;

        /** An ImTextureID in this context is castable to a VkDescriptorSet. */
        LUMINA_API ImTextureID GetOrCreateImTexture(const FString& Path) override;
        LUMINA_API ImTextureID GetOrCreateImTexture(FRHIImage* Image, const FTextureSubresourceSet& Subresources = AllSubresources) override;
        void DestroyImTexture(ImTextureRef Image) override;
        
    private:

        void DrawOverviewTab(const VkPhysicalDeviceProperties& props, const VkPhysicalDeviceMemoryProperties& memProps, VmaAllocator Allocator);
        void DrawMemoryTab(const VkPhysicalDeviceMemoryProperties& memProps, VmaAllocator Allocator);
        void DrawResourcesTab();
        void DrawDeviceInfoTab(const VkPhysicalDeviceProperties& props, const VkPhysicalDeviceFeatures& Features);
        void DrawDeviceProperties(const VkPhysicalDeviceProperties& props);
        void DrawDeviceFeatures(const VkPhysicalDeviceFeatures& Features);
        void DrawDeviceLimits(const VkPhysicalDeviceProperties& props);
        void DrawGeneralLimits(const VkPhysicalDeviceLimits& limits);
        void DrawBufferImageLimits(const VkPhysicalDeviceLimits& limits);
        void DrawComputeLimits(const VkPhysicalDeviceLimits& limits);
        void DrawDescriptorLimits(const VkPhysicalDeviceLimits& limits);
        void DrawRenderingLimits(const VkPhysicalDeviceLimits& limits);
        
    
    private:

        mutable FMutex                  Mutex;
        FMutex                          TextureMutex;

        THashMap<uint64, FEntry*>       Images;
        
        TPair<FName, FEntry*>           SquareWhiteTexture;
        
        VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;
        FVulkanRenderContext* VulkanRenderContext = nullptr;

        TFixedVector<float, 300> VRAMHistory;
        TFixedVector<FRHIImageRef, 10> ReferencedImages;
    };
}
