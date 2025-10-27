#pragma once

#include <atomic>
#include <Containers/Array.h>
#include <vulkan/vulkan.h>
#include <tracy/TracyVulkan.hpp>
#include "TrackedCommandBuffer.h"
#include "VulkanDescriptorCache.h"
#include "VulkanMacros.h"
#include "VulkanPipelineCache.h"
#include "VulkanResources.h"
#include "Core/Threading/Thread.h"
#include "Memory/SmartPtr.h"
#include "Renderer/RenderContext.h"
#include "src/VkBootstrap.h"
#include "Types/BitFlags.h"

namespace Lumina
{
    class FSpirVShaderCompiler;
    class FVulkanCommandList;
    class FVulkanSwapchain;
    class FVulkanDevice;
    enum class ECommandQueue : uint8;
}

#define USE_VK_ALLOC_CALLBACK 1

namespace Lumina
{
    extern VkAllocationCallbacks GVulkanAllocationCallbacks;

#if USE_VK_ALLOC_CALLBACK
    #define VK_ALLOC_CALLBACK &GVulkanAllocationCallbacks
#else
    #define VK_ALLOC_CALLBACK nullptr
#endif
    
    struct FVulkanRenderContextFunctions
    {
        VkDebugUtilsMessengerEXT DebugMessenger = nullptr;
        PFN_vkSetDebugUtilsObjectNameEXT DebugUtilsObjectNameEXT = nullptr;
        PFN_vkCmdDebugMarkerBeginEXT vkCmdDebugMarkerBeginEXT = nullptr;
        PFN_vkCmdDebugMarkerEndEXT vkCmdDebugMarkerEndEXT = nullptr;
    };

    enum class EVulkanExtensions : uint8
    {
        None,
        PushDescriptors,
        ConservativeRasterization,
        ViewportIndexLayer,
    };

    VkImageAspectFlags GuessImageAspectFlags(VkFormat Format);
    
    class FQueue : public IDeviceChild
    {
    public:
        
        FQueue(FVulkanRenderContext* InRenderContext, VkQueue InQueue, uint32 InQueueFamilyIndex, ECommandQueue InType);
        ~FQueue() override;
        
        /** Free threaded */
        TRefCountPtr<FTrackedCommandBuffer> GetOrCreateCommandBuffer();

        void RetireCommandBuffers();
        
        /** Submission must happen from a single thread at a time */
        uint64 Submit(ICommandList* const* CommandLists, uint32 NumCommandLists);

        void WaitIdle();
        uint64 UpdateLastFinishID();
        bool PollCommandList(uint64 CommandListID);
        bool WaitCommandList(uint64 CommandListID, uint64 Timeout);
        
        void AddSignalSemaphore(VkSemaphore Semaphore, uint64 Value);
        void AddWaitSemaphore(VkSemaphore Semaphore, uint64 Value);

        uint64                              LastRecordingID = 0;
        uint64                              LastSubmittedID = 0;
        uint64                              LastFinishedID = 0;
        
        TFixedVector<VkSemaphore, 4>        WaitSemaphores;
        TFixedVector<uint64, 4>             WaitSemaphoreValues;
        TFixedVector<VkSemaphore, 4>        SignalSemaphores;
        TFixedVector<uint64, 4>             SignalSemaphoreValues;
        
        ECommandQueue               Type;
        TracyLockable(FMutex, GetMutex);
        TracyLockable(FMutex, SubmitMutex);
        VkCommandPool               CommandPool;
        VkQueue                     Queue;
        uint32                      QueueFamilyIndex;
        VkSemaphore                 TimelineSemaphore;

        TFixedVector<TRefCountPtr<FTrackedCommandBuffer>, 4> CommandBuffersInFlight;
        TFixedVector<TRefCountPtr<FTrackedCommandBuffer>, 4> CommandBufferPool;
    };

    class FCommandListManager
    {
    private:

        THashMap<Threading::ThreadID, FRHICommandListRef> CommandLists;
        FMutex Mutex;

    public:
        FRHICommandListRef GetOrCreateCommandList(const FCommandListInfo& CommandListInfo = FCommandListInfo::Graphics());

        void CloseAll()
        {
            FScopeLock Lock(Mutex);
            for (auto& [ID, CmdList] : CommandLists)
            {
                CmdList->Close();
            }
        }

        TVector<ICommandList*> GetAllCommandLists()
        {
            TVector<ICommandList*> Result;
            FScopeLock Lock(Mutex);
            Result.reserve(CommandLists.size());
            for (auto& [ID, CmdList] : CommandLists)
            {
                Result.push_back(CmdList);
            }
            return Result;
        }

        void Cleanup()
        {
            FScopeLock Lock(Mutex);
            CommandLists.clear();
        }

        ~FCommandListManager()
        {
            Cleanup();
        }
    };
    
    class FVulkanRenderContext : public IRenderContext
    {
    public:

        using FQueueArray = TArray<TUniquePtr<FQueue>, (uint32)ECommandQueue::Num>;
        
        FVulkanRenderContext();
        
        bool Initialize() override;
        void Deinitialize() override;
        
        void SetVSyncEnabled(bool bEnable) override;
        bool IsVSyncEnabled() const override;

        void WaitIdle() override;
        void CreateDevice(vkb::Instance Instance);
        
        bool FrameStart(const FUpdateContext& UpdateContext, uint8 InCurrentFrameIndex) override;
        bool FrameEnd(const FUpdateContext& UpdateContext) override;

        uint64 GetAllocatedMemory() const override;
        uint64 GetAvailableMemory() const override;


        NODISCARD FQueue* GetQueue(ECommandQueue Type) const { return Queues[(uint32)Type].get(); }

        NODISCARD FRHICommandListRef CreateCommandList(const FCommandListInfo& Info) override;
        uint64 ExecuteCommandLists(ICommandList* const* CommandLists, uint32 NumCommandLists, ECommandQueue QueueType) override;
        NODISCARD FRHICommandListRef GetOrCreateCommandList(ECommandQueue Queue) override;
        NODISCARD FRHICommandListRef GetImmediateCommandList() override;
        
        NODISCARD VkInstance GetVulkanInstance() const { return VulkanInstance; }
        NODISCARD FVulkanDevice* GetDevice() const { return VulkanDevice; }
        NODISCARD FVulkanSwapchain* GetSwapchain() const { return Swapchain; }
        
        //-------------------------------------------------------------------------------------

        NODISCARD FRHIEventQueryRef CreateEventQuery() override;
        void SetEventQuery(IEventQuery* Query, ECommandQueue Queue) override;
        void ResetEventQuery(IEventQuery* Query) override;
        void PollEventQuery(IEventQuery* Query) override;
        void WaitEventQuery(IEventQuery* Query) override;

        //-------------------------------------------------------------------------------------

        NODISCARD void* MapBuffer(FRHIBuffer* Buffer) override;
        NODISCARD void UnMapBuffer(FRHIBuffer* Buffer) override;
        NODISCARD FRHIBufferRef CreateBuffer(const FRHIBufferDesc& Description) override;
        NODISCARD FRHIBufferRef CreateBuffer(ICommandList* CommandList, const void* InitialData, const FRHIBufferDesc& Description) override;
        NODISCARD uint64 GetAlignedSizeForBuffer(uint64 Size, TBitFlags<EBufferUsageFlags> Usage) override;

        
        //-------------------------------------------------------------------------------------

        NODISCARD FRHIViewportRef CreateViewport(const FIntVector2D& Size) override;
        
        NODISCARD FRHIStagingImageRef CreateStagingImage(const FRHIImageDesc& Desc, ERHIAccess Access) override;
        void* MapStagingTexture(FRHIStagingImage* Image, const FTextureSlice& slice, ERHIAccess Access, size_t* OutRowPitch) override;
        void UnMapStagingTexture(FRHIStagingImage* Image) override;
        
        NODISCARD FRHIImageRef CreateImage(const FRHIImageDesc& ImageSpec) override;
        NODISCARD FRHISamplerRef CreateSampler(const FSamplerDesc& SamplerDesc) override;
        
        
        //-------------------------------------------------------------------------------------

        NODISCARD FRHIVertexShaderRef CreateVertexShader(const FShaderHeader& Shader) override;
        NODISCARD FRHIPixelShaderRef CreatePixelShader(const FShaderHeader& Shader) override;
        NODISCARD FRHIComputeShaderRef CreateComputeShader(const FShaderHeader& Shader) override;
        NODISCARD FRHIGeometryShaderRef CreateGeometryShader(const FShaderHeader& Shader) override;

        NODISCARD IShaderCompiler* GetShaderCompiler() const override;
        NODISCARD FRHIShaderLibraryRef GetShaderLibrary() const override;
        void CompileEngineShaders() override;
        void OnShaderCompiled(FRHIShader* Shader, bool bAddToLibrary, bool bReloadPipelines) override;

        
        //-------------------------------------------------------------------------------------

        NODISCARD FRHIDescriptorTableRef CreateDescriptorTable(FRHIBindingLayout* InLayout) override;
        void ResizeDescriptorTable(FRHIDescriptorTable* Table, uint32 NewSize, bool bKeepContents) override;
        bool WriteDescriptorTable(FRHIDescriptorTable* Table, const FBindingSetItem& Binding) override;
        NODISCARD FRHIInputLayoutRef CreateInputLayout(const FVertexAttributeDesc* AttributeDesc, uint32 Count) override;
        NODISCARD FRHIBindingLayoutRef CreateBindingLayout(const FBindingLayoutDesc& Desc) override;
        NODISCARD FRHIBindingLayoutRef CreateBindlessLayout(const FBindlessLayoutDesc& Desc) override;
        NODISCARD FRHIBindingSetRef CreateBindingSet(const FBindingSetDesc& Desc, FRHIBindingLayout* InLayout) override;
        void CreateBindingSetAndLayout(const TBitFlags<ERHIShaderType>& Visibility, uint16 Binding, const FBindingSetDesc& Desc, FRHIBindingLayoutRef& OutLayout, FRHIBindingSetRef& OutBindingSet) override;
        NODISCARD FRHIComputePipelineRef CreateComputePipeline(const FComputePipelineDesc& Desc) override;
        NODISCARD FRHIGraphicsPipelineRef CreateGraphicsPipeline(const FGraphicsPipelineDesc& Desc, const FRenderPassDesc& RenderPassDesc) override;
        

        //-------------------------------------------------------------------------------------

        void SetObjectName(IRHIResource* Resource, const char* Name, EAPIResourceType Type) override;
        
        void FlushPendingDeletes() override;
        
        void SetVulkanObjectName(FString Name, VkObjectType ObjectType, uint64 Handle);
        FVulkanRenderContextFunctions& GetDebugUtils();
    
    private:

        uint8                                               CurrentFrameIndex;
        TBitFlags<EVulkanExtensions>                        EnabledExtensions;
        
        THashMap<uint64, FRHIInputLayoutRef>                InputLayoutMap;
        THashMap<uint64, FRHISamplerRef>                    SamplerMap;
        
        FVulkanDescriptorCache                              DescriptorCache;
        FVulkanPipelineCache                                PipelineCache;
        FQueueArray                                         Queues;
        
        FRHICommandListRef                                  ImmediateCommandList;

        VkInstance                                          VulkanInstance;
        
        FVulkanSwapchain*                                   Swapchain = nullptr;
        FVulkanDevice*                                      VulkanDevice = nullptr;
        FVulkanRenderContextFunctions                       DebugUtils;

        FSpirVShaderCompiler*                               ShaderCompiler;
        FRHIShaderLibraryRef                                ShaderLibrary;
    };
}
