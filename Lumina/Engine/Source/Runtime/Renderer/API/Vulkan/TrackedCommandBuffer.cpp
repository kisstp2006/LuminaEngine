#include "TrackedCommandBuffer.h"

#include "VulkanRenderContext.h"


namespace Lumina
{
    FTrackedCommandBuffer::FTrackedCommandBuffer(FVulkanDevice* InDevice, VkCommandBuffer InBuffer, VkCommandPool InPool, FQueue* InQueue)
        : IDeviceChild(InDevice)
        , CommandBuffer(InBuffer)
        , CommandPool(InPool)
        , Queue(InQueue)
    {
        if (InQueue->Type != ECommandQueue::Transfer)
        {
            std::scoped_lock Lock(InQueue->Mutex);
            LockMark(InQueue->Mutex);
            TracyContext = TracyVkContext(InDevice->GetPhysicalDevice(), InDevice->GetDevice(), Queue->Queue, CommandBuffer)
        }
    }

    FTrackedCommandBuffer::~FTrackedCommandBuffer()
    {
        if (TracyContext)
        {
            std::scoped_lock Lock(Queue->Mutex);
            LockMark(Queue->Mutex);
            TracyVkDestroy(TracyContext)
            TracyContext = nullptr;
        }
        
        vkDestroyCommandPool(Device->GetDevice(), CommandPool, VK_ALLOC_CALLBACK);
        CommandPool = VK_NULL_HANDLE;
    }

    void FTrackedCommandBuffer::AddReferencedResource(const TRefCountPtr<IRHIResource>& InResource)
    {
        ReferencedResources.push_back(InResource);
    }

    void FTrackedCommandBuffer::AddStagingResource(const TRefCountPtr<FRHIBuffer>& InResource)
    {
        ReferencedStagingResources.push_back(InResource);
    }

    void FTrackedCommandBuffer::ClearReferencedResources()
    {
        ReferencedResources.clear();
        ReferencedStagingResources.clear();
    }
}
