#pragma once

#include "Platform/Platform.h"
#include "VulkanResources.h"
#include "Containers/Array.h"
#include <vulkan/vulkan_core.h>
#include "Core/Math/Math.h"
#include "Renderer/RHIFwd.h"

namespace Lumina
{
    class FVulkanRenderContext;
    class FWindow;
}

namespace Lumina
{

    class FVulkanSwapchain
    {
    public:
        
        ~FVulkanSwapchain();

        void CreateSwapchain(VkInstance Instance, FVulkanRenderContext* InContext, FWindow* Window, FIntVector2D Extent, bool bFromResize = false);

        void RecreateSwapchain(const FIntVector2D& Extent);
        void SetPresentMode(VkPresentModeKHR NewMode);

        FORCEINLINE const VkSurfaceFormatKHR& GetSurfaceFormat() const { return SurfaceFormat; }
        FORCEINLINE uint32 GetNumFramesInFlight() const { return FramesInFlight.size(); }
        FORCEINLINE uint32 GetCurrentImageIndex() const { return CurrentImageIndex; }
        FORCEINLINE uint32 GetImageCount() const { return (uint32)SwapchainImages.size(); }
        FORCEINLINE VkPresentModeKHR GetPresentMode() const { return CurrentPresentMode; }
        FORCEINLINE VkFormat GetSwapchainFormat() const { return Format; }
        FORCEINLINE const FIntVector2D& GetSwapchainExtent() const { return SwapchainExtent; }
        
        TRefCountPtr<FVulkanImage> GetCurrentImage() const;

        bool AcquireNextImage();
        bool Present();
        
    private:

        uint64                                  AcquireSemaphoreIndex = 0;
        uint32                                  CurrentImageIndex = 0;
        
        bool                                    bNeedsResize = false;
        VkSurfaceKHR                            Surface = VK_NULL_HANDLE;
        VkFormat                                Format = VK_FORMAT_MAX_ENUM;
        FIntVector2D                            SwapchainExtent;
                                                
        VkSwapchainKHR                          Swapchain = VK_NULL_HANDLE;
        VkSurfaceFormatKHR                      SurfaceFormat = {};
        VkPresentModeKHR                        CurrentPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        
        TVector<TRefCountPtr<FVulkanImage>>     SwapchainImages;
        TVector<VkSemaphore>                    PresentSemaphores;
        TVector<VkSemaphore>                    AcquireSemaphores;
        FVulkanRenderContext*                   Context = nullptr;

        TFixedVector<FRHIEventQueryRef, 4>      FramesInFlight;
        TFixedVector<FRHIEventQueryRef, 4>      QueryPool;
    };
    
}
