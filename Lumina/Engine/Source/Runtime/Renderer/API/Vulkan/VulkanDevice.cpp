#include "VulkanDevice.h"
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include "VulkanMacros.h"
#include "VulkanRenderContext.h"
#include "VulkanResources.h"
#include "Core/Profiler/Profile.h"


namespace Lumina
{
    FVulkanMemoryAllocator::FVulkanMemoryAllocator(FVulkanRenderContext* InCxt, VkInstance Instance, VkPhysicalDevice PhysicalDevice, VkDevice Device)
    {
        VmaVulkanFunctions Functions = {};
        Functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        Functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    
        VmaAllocatorCreateInfo Info = {};
        Info.vulkanApiVersion = VK_API_VERSION_1_3;
        Info.instance = Instance;
        Info.physicalDevice = PhysicalDevice;
        Info.device = Device;
        Info.pVulkanFunctions = &Functions;
        Info.pAllocationCallbacks = VK_ALLOC_CALLBACK;
        
        Info.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT | VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;
        
        // Enable buffer device address if needed
        // Info.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    
        VK_CHECK(vmaCreateAllocator(&Info, &Allocator));
        RenderContext = InCxt;
        
        CreateCommonPools();
    }

    FVulkanMemoryAllocator::~FVulkanMemoryAllocator()
    {
        ClearAllAllocations();
    }

    void FVulkanMemoryAllocator::CreateCommonPools()
    {
        CreateBufferPool(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, 16 * 1024 * 1024); // 16MB blocks
        
        CreateBufferPool(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, 64 * 1024 * 1024); // 64MB blocks
        
        CreateBufferPool(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, 32 * 1024 * 1024); // 32MB blocks
    }
    
    void FVulkanMemoryAllocator::CreateBufferPool(VkBufferUsageFlags Usage, VmaAllocationCreateFlags Flags, VkDeviceSize BlockSize)
    {
        VkBufferCreateInfo BufferInfo = {};
        BufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        BufferInfo.size = 1024;
        BufferInfo.usage = Usage;
        
        VmaAllocationCreateInfo AllocInfo = {};
        AllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        AllocInfo.flags = Flags;
        
        uint32_t MemTypeIndex;
        VK_CHECK(vmaFindMemoryTypeIndexForBufferInfo(Allocator, &BufferInfo, &AllocInfo, &MemTypeIndex));
        
        VmaPoolCreateInfo PoolInfo = {};
        PoolInfo.memoryTypeIndex = MemTypeIndex;
        PoolInfo.blockSize = BlockSize;
        PoolInfo.minBlockCount = 1;
        PoolInfo.maxBlockCount = 8;
        PoolInfo.priority = 0.5f; // Medium priority
        
        PoolInfo.flags = VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT;
        
        VmaPool Pool;
        VK_CHECK(vmaCreatePool(Allocator, &PoolInfo, &Pool));
        
        uint64_t Key = static_cast<uint64_t>(Usage);
        BufferPools[Key] = {Pool, static_cast<uint32_t>(BlockSize)};
    }
    
    VmaAllocation FVulkanMemoryAllocator::AllocateBuffer(const VkBufferCreateInfo* CreateInfo, VmaAllocationCreateFlags Flags, VkBuffer* vkBuffer, const char* AllocationName)
    {
        LUMINA_PROFILE_SCOPE();
    
        VmaAllocationCreateInfo Info = {};
        Info.usage = VMA_MEMORY_USAGE_AUTO;
        Info.flags = Flags;

        FScopeLock Lock(BufferAllocationMutex);

        uint64 PoolKey = CreateInfo->usage;
        auto PoolIt = BufferPools.find(PoolKey);
        if (PoolIt != BufferPools.end() && CreateInfo->size < PoolIt->second.BlockSize / 2)
        {
            Info.pool = PoolIt->second.Pool;
        }
        
        if (CreateInfo->size > 256 * 1024 * 1024) // 256MB
        {
            Info.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            Info.priority = 1.0f;
        }
        
        if (Flags & VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)
        {
            Info.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }
    
        VmaAllocation Allocation = nullptr;
        VmaAllocationInfo AllocationInfo;

        
        VK_CHECK(vmaCreateBuffer(Allocator, CreateInfo, &Info, vkBuffer, &Allocation, &AllocationInfo));
        AssertMsg(Allocation, "Vulkan failed to allocate buffer memory!");
    
    #if LE_DEBUG
        if (AllocationName)
        {
            vmaSetAllocationName(Allocator, Allocation, AllocationName);
        }
    #endif
        
        AllocatedBuffers[*vkBuffer] = eastl::make_pair(Allocation, AllocationInfo);
        
        return Allocation;
    }
    
    VmaAllocation FVulkanMemoryAllocator::AllocateImage(VkImageCreateInfo* CreateInfo, VmaAllocationCreateFlags Flags, VkImage* vkImage, const char* AllocationName)
    {
        constexpr uint64 DEDICATED_MEMORY_THRESHOLD = 2048 * 2048;

        LUMINA_PROFILE_SCOPE();
    
        if (CreateInfo->extent.depth == 0)
        {
            LOG_WARN("Trying to allocate image with 0 depth. No allocation done");
            return VK_NULL_HANDLE;
        }
    
        VmaAllocationCreateInfo Info = {};
        Info.usage = VMA_MEMORY_USAGE_AUTO;
        Info.flags = Flags;
        
        VkDeviceSize ImageSize = CreateInfo->extent.width * CreateInfo->extent.height * CreateInfo->extent.depth * CreateInfo->arrayLayers;
        
        if (ImageSize > DEDICATED_MEMORY_THRESHOLD || CreateInfo->usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT || CreateInfo->usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            Info.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            Info.priority = 0.75f;
        }
    
        VmaAllocation Allocation;
        VmaAllocationInfo AllocationInfo;

        FScopeLock Lock(ImageAllocationMutex);

        VK_CHECK(vmaCreateImage(Allocator, CreateInfo, &Info, vkImage, &Allocation, &AllocationInfo));
        AssertMsg(Allocation, "Vulkan failed to allocate image memory!");
    
    #if LE_DEBUG
        if (AllocationName && strlen(AllocationName) > 0)
        {
            vmaSetAllocationName(Allocator, Allocation, AllocationName);
        }
    #endif
        
        AllocatedImages[*vkImage] = eastl::make_pair(Allocation, AllocationInfo);
        
        return Allocation;
    }
    
    VmaAllocation FVulkanMemoryAllocator::GetAllocation(VkBuffer Buffer)
    {
        auto it = AllocatedBuffers.find(Buffer);
        return (it != AllocatedBuffers.end()) ? it->second.first : VK_NULL_HANDLE;
    }
    
    VmaAllocation FVulkanMemoryAllocator::GetAllocation(VkImage Image)
    {
        auto it = AllocatedImages.find(Image);
        return (it != AllocatedImages.end()) ? it->second.first : VK_NULL_HANDLE;
    }
    
    void FVulkanMemoryAllocator::DestroyBuffer(VkBuffer Buffer)
    {
        LUMINA_PROFILE_SCOPE();
        Assert(Buffer)
        
        FScopeLock Lock(BufferAllocationMutex);
    
        auto it = AllocatedBuffers.find(Buffer);
        if (it == AllocatedBuffers.end())
        {
            LOG_CRITICAL("Buffer was not found in VulkanMemoryAllocator!");
            return;
        }
        
        vmaDestroyBuffer(Allocator, Buffer, it->second.first);
        AllocatedBuffers.erase(it);
    }
    
    void FVulkanMemoryAllocator::DestroyImage(VkImage Image)
    {
        LUMINA_PROFILE_SCOPE();
        Assert(Image)
    
        FScopeLock Lock(ImageAllocationMutex);
        
        auto it = AllocatedImages.find(Image);
        if (it == AllocatedImages.end())
        {
            LOG_CRITICAL("Image was not found in VulkanMemoryAllocator!");
            return;
        }
        
        vmaDestroyImage(Allocator, Image, it->second.first);
        AllocatedImages.erase(it);
    }
    
    void* FVulkanMemoryAllocator::GetMappedMemory(FVulkanBuffer* Buffer)
    {
        LUMINA_PROFILE_SCOPE();
        
        if (Buffer->LastUseCommandListID != 0)
        {
            FQueue* Queue = RenderContext->GetQueue(Buffer->LastUseQueue);
            Queue->WaitCommandList(Buffer->LastUseCommandListID, UINT64_MAX);
        }

        return AllocatedBuffers[Buffer->GetBuffer()].second.pMappedData;
    }
    
    void FVulkanMemoryAllocator::ClearAllAllocations()
    {
        LUMINA_PROFILE_SCOPE();
    
        for (auto& kvp : AllocatedBuffers)
        {
            if (kvp.first != VK_NULL_HANDLE)
            {
                vmaDestroyBuffer(Allocator, kvp.first, kvp.second.first);
            }
        }
        AllocatedBuffers.clear();
    
        for (auto& kvp : AllocatedImages)
        {
            if (kvp.first != VK_NULL_HANDLE)
            {
                vmaDestroyImage(Allocator, kvp.first, kvp.second.first);
            }
        }
        AllocatedImages.clear();
        
        for (auto& pool : BufferPools)
        {
            vmaDestroyPool(Allocator, pool.second.Pool);
        }
        BufferPools.clear();
        
        for (auto& pool : ImagePools)
        {
            vmaDestroyPool(Allocator, pool.second.Pool);
        }
        ImagePools.clear();
        
        vmaDestroyAllocator(Allocator);
        Allocator = VK_NULL_HANDLE;
    }
    
    void FVulkanMemoryAllocator::DefragmentBuffers()
    {
        LUMINA_PROFILE_SCOPE();
        
        for (auto& pool : BufferPools)
        {
            VmaDefragmentationInfo DefragInfo = {};
            DefragInfo.pool = pool.second.Pool;
            DefragInfo.flags = VMA_DEFRAGMENTATION_FLAG_ALGORITHM_FAST_BIT;
            
            VmaDefragmentationContext DefragCtx;
            VK_CHECK(vmaBeginDefragmentation(Allocator, &DefragInfo, &DefragCtx));
            
            VmaDefragmentationPassMoveInfo PassInfo = {};
            VK_CHECK(vmaBeginDefragmentationPass(Allocator, DefragCtx, &PassInfo));
            vmaEndDefragmentationPass(Allocator, DefragCtx, &PassInfo);
            vmaEndDefragmentation(Allocator, DefragCtx, nullptr);
        }
    }
    
    void FVulkanMemoryAllocator::GetMemoryBudget(VmaBudget* OutBudgets)
    {
        vmaGetHeapBudgets(Allocator, OutBudgets);
    }
    
    void FVulkanMemoryAllocator::LogMemoryStats()
    {
        VmaTotalStatistics Stats;
        vmaCalculateStatistics(Allocator, &Stats);
        
        LOG_INFO("=== Vulkan Memory Statistics ===");
        LOG_INFO("Total Allocated: %.2f MB", Stats.total.statistics.allocationBytes / (1024.0f * 1024.0f));
        LOG_INFO("Total Block Count: %u", Stats.total.statistics.blockCount);
        LOG_INFO("Allocation Count: %u", Stats.total.statistics.allocationCount);
    }
}
