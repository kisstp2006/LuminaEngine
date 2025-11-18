#pragma once

#include <atomic>
#include <Containers/Array.h>
#include <volk/volk.h>
#define VK_NO_PROTOTYPES
#include <tracy/TracyVulkan.hpp>
#include "TrackedCommandBuffer.h"
#include "VulkanMacros.h"
#include "VulkanPipelineCache.h"
#include "VulkanResources.h"
#include "concurrentqueue/concurrentqueue.h"
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

        /**
         * @threadsafety - Called from ICommandList::Open, so free-threaded.
         * @return A valid command buffer to use.
         */
        TRefCountPtr<FTrackedCommandBuffer> GetOrCreateCommandBuffer();

        void RetireCommandBuffers();
        
        /** Submission must happen from a single thread at a time */
        uint64 Submit(ICommandList* const* CommandLists, uint32 NumCommandLists);
        void SignalSemaphore(VkSemaphore SemaphoreToSignal) const;
        
        void WaitIdle();
        uint64 UpdateLastFinishID();
        bool PollCommandList(uint64 CommandListID);
        bool WaitCommandList(uint64 CommandListID, uint64 Timeout);
        
        void AddSignalSemaphore(VkSemaphore Semaphore, uint64 Value);
        void AddWaitSemaphore(VkSemaphore Semaphore, uint64 Value, VkPipelineStageFlags Stage);

        FVulkanRenderContext*               RenderContext;
        eastl::atomic<uint64>               LastRecordingID = 0;
        uint64                              LastSubmittedID = 0;
        uint64                              LastFinishedID = 0;
        
        TFixedVector<VkSemaphore, 4>            WaitSemaphores;
        TFixedVector<uint64, 4>                 WaitSemaphoreValues;
        TFixedVector<VkPipelineStageFlags, 4>   WaitStageFlags;
        TFixedVector<VkSemaphore, 4>            SignalSemaphores;
        TFixedVector<uint64, 4>                 SignalSemaphoreValues;

        ECommandQueue               Type;
        TracyLockable(FMutex,       Mutex);
        
        VkQueue                     Queue;
        uint32                      QueueFamilyIndex;
        VkSemaphore                 TimelineSemaphore;

        TFixedVector<TRefCountPtr<FTrackedCommandBuffer>, 4> CommandBuffersInFlight;
        TConcurrentQueue<TRefCountPtr<FTrackedCommandBuffer>> CommandBufferPool;
    };

    class FCommandListManager
    {
    private:

        TArray<TConcurrentQueue<FRHICommandListRef>, (uint32)ECommandQueue::Num> CommandLists;

    public:
        FRHICommandListRef GetOrCreateCommandList(FVulkanRenderContext* RenderContext, const FCommandListInfo& CommandListInfo);

        void Enqueue(ICommandList* RetiredCommandList);
        void BulkEnqueue(ICommandList* const* RetiredCommandLists, uint32 Num, ECommandQueue QueueType);
        void Cleanup();
    };


    
    class FVulkanRenderContext : public IRenderContext
    {
    public:

        using FQueueArray = TArray<TUniquePtr<FQueue>, (uint32)ECommandQueue::Num>;
        
        FVulkanRenderContext();
        
        bool Initialize(const FRenderContextDesc& Desc) override;
        void Deinitialize() override;
        
        void SetVSyncEnabled(bool bEnable) override;
        bool IsVSyncEnabled() const override;

        void WaitIdle() override;
        void CreateDevice(vkb::Instance Instance);
        
        bool FrameStart(const FUpdateContext& UpdateContext, uint8 InCurrentFrameIndex) override;
        bool FrameEnd(const FUpdateContext& UpdateContext, FRenderGraph& RenderGraph) override;

        uint64 GetAllocatedMemory() const override;
        uint64 GetAvailableMemory() const override;


        NODISCARD FQueue* GetQueue(ECommandQueue Type) const { return Queues[(uint32)Type].get(); }

        void ClearCommandListCache() override;
        NODISCARD FRHICommandListRef CreateCommandList(const FCommandListInfo& Info) override;
        uint64 ExecuteCommandLists(ICommandList* const* CommandLists, uint32 NumCommandLists, ECommandQueue QueueType) override;
        
        NODISCARD VkInstance GetVulkanInstance() const { return VulkanInstance; }
        NODISCARD FVulkanDevice* GetDevice() const { return VulkanDevice; }
        NODISCARD FVulkanSwapchain* GetSwapchain() const { return Swapchain; }
        
        //-------------------------------------------------------------------------------------

        NODISCARD FRHIEventQueryRef CreateEventQuery() override;
        void SetEventQuery(IEventQuery* Query, ECommandQueue Queue) override;
        void ResetEventQuery(IEventQuery* Query) override;
        bool PollEventQuery(IEventQuery* Query) override;
        void WaitEventQuery(IEventQuery* Query) override;

        void AddCommandQueueWait(ECommandQueue Waiting, ECommandQueue WaitOn) override;
        
        //-------------------------------------------------------------------------------------

        NODISCARD void* MapBuffer(FRHIBuffer* Buffer) override;
        NODISCARD void UnMapBuffer(FRHIBuffer* Buffer) override;
        NODISCARD FRHIBufferRef CreateBuffer(const FRHIBufferDesc& Description) override;
        NODISCARD FRHIBufferRef CreateBuffer(ICommandList* CommandList, const void* InitialData, const FRHIBufferDesc& Description) override;
        NODISCARD uint64 GetAlignedSizeForBuffer(uint64 Size, TBitFlags<EBufferUsageFlags> Usage) override;

        
        //-------------------------------------------------------------------------------------

        NODISCARD FRHIViewportRef CreateViewport(const glm::uvec2& Size) override;
        
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

        void ClearBindingCaches() override;
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

        FRenderContextDesc                                  Description;
        uint8                                               CurrentFrameIndex;
        TBitFlags<EVulkanExtensions>                        EnabledExtensions;

        FMutex                                              LayoutMutex;
        THashMap<uint64, FRHIInputLayoutRef>                InputLayoutMap;
        FMutex                                              SamplerMutex;
        THashMap<uint64, FRHISamplerRef>                    SamplerMap;

        FCommandListManager                                 CommandListManager;
        FVulkanPipelineCache                                PipelineCache;
        FQueueArray                                         Queues;
        
        VkInstance                                          VulkanInstance;
        
        FVulkanSwapchain*                                   Swapchain = nullptr;
        FVulkanDevice*                                      VulkanDevice = nullptr;
        FVulkanRenderContextFunctions                       DebugUtils;

        FSpirVShaderCompiler*                               ShaderCompiler;
        FRHIShaderLibraryRef                                ShaderLibrary;
    };
}
