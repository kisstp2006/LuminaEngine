#pragma once

#include "VulkanDevice.h"
#include "VulkanMacros.h"
#include "VulkanRenderContext.h"
#include "Containers/Tuple.h"
#include "Memory/SmartPtr.h"
#include "Renderer/RenderResource.h"
#include "Renderer/Shader.h"

namespace Lumina
{

    // ----------------------------------------------------------------------------------
    // GLSL / GLM Type       | Vulkan VkFormat Equivalent         | Size (Bytes) | Notes
    // ----------------------------------------------------------------------------------
    // float                 | VK_FORMAT_R32_SFLOAT               | 4            | 1 × 32-bit float
    // vec2 / glm::vec2      | VK_FORMAT_R32G32_SFLOAT            | 8            | 2 × 32-bit floats
    // vec3 / glm::vec3      | VK_FORMAT_R32G32B32_SFLOAT         | 12           | 3 × 32-bit floats (no padding in VkFormat)
    // vec4 / glm::vec4      | VK_FORMAT_R32G32B32A32_SFLOAT      | 16           | 4 × 32-bit floats
    // int                   | VK_FORMAT_R32_SINT                 | 4            | 1 × 32-bit signed integer
    // ivec2 / glm::ivec2    | VK_FORMAT_R32G32_SINT              | 8            | 2 × 32-bit signed integers
    // ivec3 / glm::ivec3    | VK_FORMAT_R32G32B32_SINT           | 12           | 3 × 32-bit signed integers
    // ivec4 / glm::ivec4    | VK_FORMAT_R32G32B32A32_SINT        | 16           | 4 × 32-bit signed integers
    // uint                  | VK_FORMAT_R32_UINT                 | 4            | 1 × 32-bit unsigned integer
    // uvec2 / glm::uvec2    | VK_FORMAT_R32G32_UINT              | 8            | 2 × 32-bit unsigned integers
    // uvec3 / glm::uvec3    | VK_FORMAT_R32G32B32_UINT           | 12           | 3 × 32-bit unsigned integers
    // uvec4 / glm::uvec4    | VK_FORMAT_R32G32B32A32_UINT        | 16           | 4 × 32-bit unsigned integers
    // ----------------------------------------------------------------------------------
    // Note:
    // - There is no VK_FORMAT for 3-element types with alignment padding like GLSL's std140 vec3.
    //   In vertex formats, VK_FORMAT_R32G32B32_* is valid and used, but memory alignment should
    //   still be handled carefully when packing structs in C++.
    // - Avoid using vec3 in UBOs unless you pad to vec4 alignment manually.
    // ----------------------------------------------------------------------------------
    

    VkFormat ConvertFormat(EFormat Format);
    
    class FVulkanSwapchain;

    struct FBufferChunk
    {
        FRHIBufferRef Buffer;
        uint64 Version = 0;
        uint64 BufferSize = 0;
        std::atomic_uint64_t WritePointer{0};
        void* MappedMemory = nullptr;

        static constexpr uint64 GSizeAlignment = 4096; // GPU page size
    };

    class FUploadManager : public IDeviceChild
    {
    public:
        
        FUploadManager(FVulkanRenderContext* Ctx, uint64 InDefaultChunkSize, uint64 InMemoryLimit, bool bInIsScratchBuffer);

        TSharedPtr<FBufferChunk> CreateChunk(uint64 Size);

        bool SuballocateBuffer(uint64 Size, FRHIBuffer** Buffer, uint64* Offset, void** CpuVA, uint64 CurrentVersion, uint32 Alignment = 256);
        void SubmitChunks(uint64 CurrentVersion, uint64 submittedVersion);

    private:
        
        FVulkanRenderContext* Context;
        uint64 DefaultChunkSize = 0;
        uint64 MemoryLimit = 0;
        uint64 AllocatedMemory = 0;
        bool bIsScratchBuffer = false;

        TFixedList<TSharedPtr<FBufferChunk>, 8>    ChunkPool;
        TSharedPtr<FBufferChunk>                   CurrentChunk = nullptr;
    };
    
    class FVulkanEventQuery : public IEventQuery
    {
    public:
        RENDER_RESOURCE(RRT_None)

        FVulkanEventQuery();
        ~FVulkanEventQuery();

        ECommandQueue   Queue = ECommandQueue::Graphics;
        uint64          CommandListID = 0;

        VkQueryPool QueryPool = nullptr;
    };
    
    class FVulkanViewport : public FRHIViewport
    {
    public:
        
        friend class FVulkanRenderContext;
        

        FVulkanViewport(const FIntVector2D& InSize, IRenderContext* InContext)
            : FRHIViewport(InSize, InContext)
        {}

    private:

        FVulkanSwapchain* Swapchain = nullptr;
        
    };

    // A copyable version of std::atomic.
    class FBufferVersionItem : public std::atomic<uint64> 
    {
    public:
        FBufferVersionItem()
        { }

        FBufferVersionItem(const FBufferVersionItem& other)
        {
            store(other);
        }

        FBufferVersionItem& operator=(const uint64 a)
        {
            store(a);
            return *this;
        }
    };

    class FVulkanBuffer : public FRHIBuffer, public IDeviceChild
    {
    public:
        friend class FVulkanCommandList;
        friend class FQueue;
        friend class FVulkanMemoryAllocator;
        
        FVulkanBuffer(FVulkanDevice* InDevice, const FRHIBufferDesc& InDescription);
        ~FVulkanBuffer() override;

        void* GetAPIResourceImpl(EAPIResourceType Type) override
        {
            return Buffer;
        }

        VkBuffer GetBuffer() const { return Buffer; }
        VmaAllocation GetAllocation() const { return Allocation; }
        void* GetMappedMemory() const;

    private:

        ECommandQueue                       LastUseQueue = ECommandQueue::Graphics;
        uint64                              LastUseCommandListID = 0;

        TFixedVector<FBufferVersionItem, 4> VersionTracking;
        uint32                              VersionSearchStart = 0;
        VmaAllocation                       Allocation = nullptr;
        VkBuffer                            Buffer = VK_NULL_HANDLE;

    };

    
    //----------------------------------------------------------------------------------------------


    class FVulkanSampler : public FRHISampler,  public IDeviceChild
    {
    public:

        FVulkanSampler(FVulkanDevice* InDevice, const FSamplerDesc& InDesc);
        ~FVulkanSampler() override;

        const FSamplerDesc& GetDesc() const override { return Desc; }

        void* GetAPIResourceImpl(EAPIResourceType Type) override { return Sampler; }

        
    private:

        FSamplerDesc    Desc;
        VkSampler       Sampler;
    };

    struct FTextureSubresourceView
    {
        class FVulkanImage& Image;
        FTextureSubresourceSet Subresource;

        VkImageView View = nullptr;
        VkImageSubresourceRange SubresourceRange;

        FTextureSubresourceView(FVulkanImage& InImage)
            : Image(InImage)
            , SubresourceRange({})
        {
        }

        FTextureSubresourceView(const FTextureSubresourceView&) = delete;

        bool operator==(const FTextureSubresourceView& Other) const
        {
            return &Image == &Other.Image && Subresource == Other.Subresource && View == Other.View
                && SubresourceRange.aspectMask == Other.SubresourceRange.aspectMask
                && SubresourceRange.baseArrayLayer == Other.SubresourceRange.baseArrayLayer
                && SubresourceRange.baseMipLevel == Other.SubresourceRange.baseMipLevel
                && SubresourceRange.layerCount == Other.SubresourceRange.layerCount
                && SubresourceRange.levelCount == Other.SubresourceRange.levelCount;
        }
    };
    
    class FVulkanImage : public FRHIImage, public IDeviceChild
    {
    public:

        enum class ESubresourceViewType
        {
            AllAspects,
            DepthOnly,
            StencilOnly,
        };

        using FSubresourceViewKey = TTuple<FTextureSubresourceSet, ESubresourceViewType, EImageDimension, EFormat, VkImageUsageFlags>;

        struct Hash
        {
            std::size_t operator()(const FSubresourceViewKey& s) const noexcept
            {
                const auto& [subresources, viewType, dimension, format, usage] = s;

                size_t hash = 0;

                Lumina::Hash::HashCombine(hash, subresources.BaseMipLevel);
                Lumina::Hash::HashCombine(hash, subresources.NumMipLevels);
                Lumina::Hash::HashCombine(hash, subresources.BaseArraySlice);
                Lumina::Hash::HashCombine(hash, subresources.NumArraySlices);
                Lumina::Hash::HashCombine(hash, viewType);
                Lumina::Hash::HashCombine(hash, dimension);
                Lumina::Hash::HashCombine(hash, format);
                Lumina::Hash::HashCombine(hash, static_cast<uint32>(usage));

                return hash;
            }
        };

        FVulkanImage(FVulkanDevice* InDevice, const FRHIImageDesc& InDescription);
        FVulkanImage(FVulkanDevice* InDevice, const FRHIImageDesc& InDescription, VkImage RawImage, bool bManagedExternal);
        ~FVulkanImage() override;

        void* GetAPIResourceImpl(EAPIResourceType Type) override;

        VkImage GetImage() const { return Image; }
        void* GetRHIView(EFormat Format, FTextureSubresourceSet Subresources, EImageDimension Dimension, bool bReadyOnlyDSV) override;
        
        VkImageAspectFlags GetFullAspectMask() const { return FullAspectMask; }
        VkImageAspectFlags GetPartialAspectMask() const { return PartialAspectMask; }
        FTextureSubresourceView& GetSubresourceView(const FTextureSubresourceSet& Subresource, EImageDimension Dimension, EFormat Format, VkImageUsageFlags Usage, ESubresourceViewType ViewType = ESubresourceViewType::AllAspects);

        uint32 GetNumSubresources() const;
        uint32 GetSubresourceIndex(uint32 MipLevel, uint32 ArrayLayer) const;
        
    private:
        
        THashMap<FSubresourceViewKey, FTextureSubresourceView, Hash> SubresourceViews;

        
        FMutex                      SubresourceMutex;                      
        bool                        bImageManagedExternal = false;  // Mostly for swapchain.
        VkImageAspectFlags          FullAspectMask =        VK_IMAGE_ASPECT_NONE;
        VkImageAspectFlags          PartialAspectMask =     VK_IMAGE_ASPECT_NONE;
        VkImage                     Image =                 VK_NULL_HANDLE;
    };

    class FVulkanStagingImage : public FRHIStagingImage, public IDeviceChild
    {
    public:

        FVulkanStagingImage(FVulkanDevice* InDevice);

        struct FRegion
        {
            off_t Offset;
            size_t Size;
        };
        
        size_t GetBufferSize()
        {
            size_t Size = SliceRegions.back().Offset + SliceRegions.back().Size;
            return Size;
        }

        size_t ComputeSliceSize(uint32 MipLevel);
        const FRegion& GetSliceRegion(uint32 MipLevel, uint32 ArraySlice, uint32 Z);
        void PopulateSliceRegions();
        

        FRHIImageDesc Desc;
        TRefCountPtr<FVulkanBuffer> Buffer;
        TFixedVector<FRegion, 4> SliceRegions;
        
        const FRHIImageDesc& GetDesc() const override { return Desc; }
        
    };

    constexpr uint32 ShaderBinarySize = sizeof(uint32);
    
    class IVulkanShader : public IDeviceChild
    {
    public:

        IVulkanShader(FVulkanDevice* InDevice, const FShaderHeader& Shader, ERHIResourceType Type);
        
        ~IVulkanShader() override;

        void GetByteCodeImpl(void** ByteCode, uint64* Size)
        {
            *ByteCode = ShaderHeader.Binaries.data();
            *Size = ShaderHeader.Binaries.size() * ShaderBinarySize;
        }

        bool CompareSpirV(const IVulkanShader* Other) const
        {
            return Other->ShaderHeader.Binaries == ShaderHeader.Binaries;
        }
    
    protected:
        
        
        FShaderHeader ShaderHeader;
        VkShaderModule  ShaderModule = VK_NULL_HANDLE;
    };

    
    
    class FVulkanVertexShader : public FRHIVertexShader, public IVulkanShader
    {
    public:
        RENDER_RESOURCE(RRT_VertexShader)

        FVulkanVertexShader(FVulkanDevice* InDevice, const FShaderHeader& Shader)
            :IVulkanShader(InDevice, Shader, RRT_VertexShader)
        {}

        void* GetAPIResourceImpl(EAPIResourceType ResourceType) override
        {
            return ShaderModule;
        }
        
        void GetByteCode(void** ByteCode, uint64* Size) override
        {
            GetByteCodeImpl(ByteCode, Size);
        }

        const FShaderHeader& GetShaderHeader() const override
        {
            return ShaderHeader;
        }
    };

    class FVulkanPixelShader : public FRHIPixelShader, public IVulkanShader
    {
    public:

        RENDER_RESOURCE(RRT_PixelShader)

        FVulkanPixelShader(FVulkanDevice* InDevice, const FShaderHeader& Shader)
            :IVulkanShader(InDevice, Shader, RRT_PixelShader)
        {}

        void* GetAPIResourceImpl(EAPIResourceType ResourceType) override
        {
            return ShaderModule;
        }
        
        void GetByteCode(void** ByteCode, uint64* Size) override
        {
            GetByteCodeImpl(ByteCode, Size);
        }

        const FShaderHeader& GetShaderHeader() const override
        {
            return ShaderHeader;
        }
    };

    class FVulkanComputeShader : public FRHIComputeShader, public IVulkanShader
    {
    public:
        RENDER_RESOURCE(RRT_ComputeShader)

        FVulkanComputeShader(FVulkanDevice* InDevice, const FShaderHeader& Shader)
            :IVulkanShader(InDevice, Shader, RRT_ComputeShader)
        {}

        void* GetAPIResourceImpl(EAPIResourceType ResourceType) override
        {
            return ShaderModule;
        }
        
        void GetByteCode(void** ByteCode, uint64* Size) override
        {
            GetByteCodeImpl(ByteCode, Size);
        }

        const FShaderHeader& GetShaderHeader() const override
        {
            return ShaderHeader;
        }
    };

    class FVulkanInputLayout : public IRHIInputLayout
    {
    public:
    
        RENDER_RESOURCE(RTT_InputLayout)

        FVulkanInputLayout(const FVertexAttributeDesc* InAttributeDesc, uint32 AttributeCount);
        void* GetAPIResourceImpl(EAPIResourceType Type) override;
        
        
        TFixedVector<FVertexAttributeDesc, 4> InputDesc;
        TFixedVector<VkVertexInputBindingDescription, 4> BindingDesc;
        TFixedVector<VkVertexInputAttributeDescription, 4> AttributeDesc;
        
        uint32 GetNumAttributes() const override;
        const FVertexAttributeDesc* GetAttributeDesc(uint32 index) const override;
    };


    class FVulkanBindingLayout : public FRHIBindingLayout, public IDeviceChild
    {
    public:

        RENDER_RESOURCE(RRT_BindingLayout)

        FVulkanBindingLayout(FVulkanDevice* InDevice, const FBindingLayoutDesc& InDesc);
        FVulkanBindingLayout(FVulkanDevice* InDevice, const FBindlessLayoutDesc& InDesc);

        ~FVulkanBindingLayout() override;

        bool Bake();
        const FBindingLayoutDesc* GetDesc() const override { return bBindless ? nullptr : &Desc; }
        const FBindlessLayoutDesc* GetBindlessDesc() const override { return bBindless ? &BindlessDesc : nullptr; }
        void* GetAPIResourceImpl(EAPIResourceType Type) override;
        
        bool                                            bBindless = false;
        FBindingLayoutDesc                              Desc;
        FBindlessLayoutDesc                             BindlessDesc;
        
        VkDescriptorSetLayout                           DescriptorSetLayout;
        TFixedVector<VkDescriptorSetLayoutBinding, 2>   Bindings;
        TFixedVector<VkDescriptorPoolSize, 2>           PoolSizes;
    };

    // Contains a VkDescriptorSet
    class FVulkanBindingSet : public FRHIBindingSet, public IDeviceChild
    {
    public:

        RENDER_RESOURCE(RRT_BindingSet)
        
        FVulkanBindingSet(FVulkanRenderContext* RenderContext, const FBindingSetDesc& InDesc, FVulkanBindingLayout* InLayout);
        ~FVulkanBindingSet() override;

        const FBindingSetDesc* GetDesc() const override { return &Desc; }
        FRHIBindingLayout* GetLayout() const override { return Layout; }
        void* GetAPIResourceImpl(EAPIResourceType Type) override;
        
        
        TFixedVector<FRHIBufferRef, 2>              DynamicBuffers;
        TFixedVector<uint32, 4>                     BindingsRequiringTransitions;
        TFixedVector<FRHIResourceRef, 4>            Resources;

        
        TRefCountPtr<FVulkanBindingLayout>          Layout;
        FBindingSetDesc                             Desc;
        VkDescriptorPool                            DescriptorPool;
        VkDescriptorSet                             DescriptorSet;
    };

    class FVulkanDescriptorTable : public FRHIDescriptorTable, public IDeviceChild
    {
    public:

        RENDER_RESOURCE(RRT_DescriptorTable)

        FVulkanDescriptorTable(FVulkanRenderContext* InContext, FVulkanBindingLayout* InLayout);
        ~FVulkanDescriptorTable() override;
        
        const FBindingSetDesc* GetDesc() const override { return nullptr; }
        FRHIBindingLayout* GetLayout() const override { return Layout; }
        uint32_t GetCapacity() const override { return Capacity; }

        uint32_t GetFirstDescriptorIndexInHeap() const override { return 0; }
        void* GetAPIResourceImpl(EAPIResourceType Type) override;
        
        TRefCountPtr<FVulkanBindingLayout> Layout;
        uint32 Capacity = 0;

        VkDescriptorPool DescriptorPool = nullptr;
        VkDescriptorSet DescriptorSet = nullptr;
    };

    class FVulkanPipeline : public IDeviceChild
    {
    public:
        
        ~FVulkanPipeline() override;

        FVulkanPipeline(FVulkanDevice* InDevice)
            : IDeviceChild(InDevice)
            , PipelineLayout(nullptr)
            , Pipeline(nullptr)
            , PushConstantVisibility(VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM)
        {
        }
        
        void CreatePipelineLayout(FString DebugName, const TFixedVector<FRHIBindingLayoutRef, 1>& BindingLayouts, VkShaderStageFlags& OutStageFlags);
        
        VkPipelineLayout            PipelineLayout;
        VkPipeline                  Pipeline;
        VkShaderStageFlags          PushConstantVisibility;

    };
    
    class FVulkanGraphicsPipeline : public FRHIGraphicsPipeline,  public FVulkanPipeline
    {
    public:

        friend class FVulkanRenderContext;

        FVulkanGraphicsPipeline(FVulkanDevice* InDevice, const FGraphicsPipelineDesc& InDesc);

        const FGraphicsPipelineDesc& GetDesc() const override { return Desc; }
        void* GetAPIResourceImpl(EAPIResourceType Type) override;
    
    private:

        FGraphicsPipelineDesc       Desc;
        
    };

    class FVulkanComputePipeline : public FRHIComputePipeline,  public FVulkanPipeline
    {
    public:

        friend class FVulkanRenderContext;

        FVulkanComputePipeline(FVulkanDevice* InDevice, const FComputePipelineDesc& InDesc);

        const FComputePipelineDesc& GetDesc() const override { return Desc; }
        void* GetAPIResourceImpl(EAPIResourceType Type) override;
    
    private:

        FComputePipelineDesc Desc;
    };
    
}
