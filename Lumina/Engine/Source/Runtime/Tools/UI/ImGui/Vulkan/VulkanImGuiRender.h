
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#include "Tools/UI/ImGui/ImGuiRenderer.h"

namespace Lumina
{
    class FVulkanRenderContext;
    class FUpdateContext;

    class FVulkanImGuiRender: public IImGuiRenderer
    {
    public:
        
        void Initialize() override;
        void Deinitialize() override;
        
        void OnStartFrame(const FUpdateContext& UpdateContext) override;
        void OnEndFrame(const FUpdateContext& UpdateContext, FRenderGraph& RenderGraph) override;

        void DrawRenderDebugInformationWindow(bool* bOpen, const FUpdateContext& Context) override;

        /** An ImTextureID in this context is castable to a VkDescriptorSet. */
        ImTextureID GetOrCreateImTexture(FRHIImageRef Image) override;
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
        
        TFixedVector<float, 300> VRAMHistory;
        FMutex TextureMutex;
        VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;
        FVulkanRenderContext* VulkanRenderContext = nullptr;

        THashMap<VkImage, VkDescriptorSet> ImageCache;
        
        TFixedVector<FRHIImageRef, 10> ReferencedImages;
    };
}
