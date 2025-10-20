#include "VulkanImGuiRender.h"

#include "ImGuizmo.h"
#include "Assets/Factories/TextureFactory/TextureFactory.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "Core/Engine/Engine.h"
#include "Core/Profiler/Profile.h"
#include "Core/Windows/Window.h"
#include "Renderer/RenderManager.h"
#include "Renderer/RHIStaticStates.h"
#include "Renderer/API/Vulkan/VulkanMacros.h"
#include "Renderer/API/Vulkan/VulkanRenderContext.h"
#include "Renderer/API/Vulkan/VulkanSwapchain.h"
#include "Renderer/RenderGraph/RenderGraph.h"
#include "Renderer/RenderGraph/RenderGraphDescriptor.h"

namespace Lumina
{

	const char* GetRHIResourceTypeName(ERHIResourceType type)
	{
		switch (type)
		{
		case RRT_SamplerState:        return "SamplerState";
		case RTT_InputLayout:         return "InputLayout";
		case RRT_BindingLayout:       return "BindingLayout";
		case RRT_BindingSet:          return "BindingSet";
		case RRT_DepthStencilState:   return "DepthStencilState";
		case RRT_BlendState:          return "BlendState";
		case RRT_VertexShader:        return "VertexShader";
		case RRT_PixelShader:         return "PixelShader";
		case RRT_ComputeShader:       return "ComputeShader";
		case RRT_ShaderLibrary:       return "ShaderLibrary";
		case RRT_GraphicsPipeline:    return "GraphicsPipeline";
		case RRT_ComputePipeline:     return "ComputePipeline";
		case RRT_UniformBufferLayout: return "UniformBufferLayout";
		case RRT_UniformBuffer:       return "UniformBuffer";
		case RRT_Buffer:              return "Buffer";
		case RRT_Image:               return "Image";
		case RRT_GPUFence:            return "GPUFence";
		case RRT_Viewport:            return "Viewport";
		case RRT_StagingBuffer:       return "StagingBuffer";
		case RRT_CommandList:         return "CommandList";
		case RRT_DescriptorTable:     return "DescriptorTable";
		default:                      return "Unknown";
		}
	}
	
	static FVulkanImage::ESubresourceViewType GetTextureViewType(EFormat BindingFormat, EFormat TextureFormat)
	{
		EFormat Format = (BindingFormat == EFormat::UNKNOWN) ? TextureFormat : BindingFormat;

		const FFormatInfo& FormatInfo = GetFormatInfo(Format);

		if (FormatInfo.bHasDepth)
		{
			return FVulkanImage::ESubresourceViewType::DepthOnly;
		}
        
		if (FormatInfo.bHasStencil)
		{
			return FVulkanImage::ESubresourceViewType::StencilOnly;
		}
        
		return FVulkanImage::ESubresourceViewType::AllAspects;
	}
	
	FString VkFormatToString(VkFormat format)
	{
		switch (format)
		{
		case VK_FORMAT_B8G8R8A8_SRGB: return "VK_FORMAT_B8G8R8A8_SRGB";
		case VK_FORMAT_R8G8B8A8_SRGB: return "VK_FORMAT_R8G8B8A8_SRGB";
		case VK_FORMAT_B8G8R8A8_UNORM: return "VK_FORMAT_B8G8R8A8_UNORM";
		case VK_FORMAT_R8G8B8A8_UNORM: return "VK_FORMAT_R8G8B8A8_UNORM";
		case VK_FORMAT_R32G32B32A32_SFLOAT: return "VK_FORMAT_R32G32B32A32_SFLOAT";
		case VK_FORMAT_R32G32B32_SFLOAT: return "VK_FORMAT_R32G32B32_SFLOAT";
		case VK_FORMAT_R32G32_SFLOAT: return "VK_FORMAT_R32G32_SFLOAT";
		case VK_FORMAT_R32_SFLOAT: return "VK_FORMAT_R32_SFLOAT";
		default: return "Unknown Format";
		}
	}
	
	FString VkColorSpaceToString(VkColorSpaceKHR colorSpace)
	{
		switch (colorSpace)
		{
		case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR: return "VK_COLOR_SPACE_SRGB_NONLINEAR_KHR";
		case VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT: return "VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT";
		case VK_COLOR_SPACE_BT2020_LINEAR_EXT: return "VK_COLOR_SPACE_BT2020_LINEAR_EXT";
		default: return "Unknown ColorSpace";
		}
	}
	
    void FVulkanImGuiRender::Initialize()
    {
		IImGuiRenderer::Initialize();
    	LUMINA_PROFILE_SCOPE();

        VkDescriptorPoolSize PoolSizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

        VkDescriptorPoolCreateInfo PoolInfo =  {};
        PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        PoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        PoolInfo.maxSets = 1000;
        PoolInfo.poolSizeCount = (uint32)std::size(PoolSizes);
        PoolInfo.pPoolSizes = PoolSizes;
		
		VulkanRenderContext = (FVulkanRenderContext*)GRenderContext;
		
        VK_CHECK(vkCreateDescriptorPool(VulkanRenderContext->GetDevice()->GetDevice(), &PoolInfo, VK_ALLOC_CALLBACK, &DescriptorPool));
    	
        VulkanRenderContext->SetVulkanObjectName("ImGui Descriptor Pool", VK_OBJECT_TYPE_DESCRIPTOR_POOL, reinterpret_cast<uint64>(DescriptorPool));
    	
        Assert(ImGui_ImplGlfw_InitForVulkan(Windowing::GetPrimaryWindowHandle()->GetWindow(), true))

		VkFormat Format = VulkanRenderContext->GetSwapchain()->GetSwapchainFormat();
		
        VkPipelineRenderingCreateInfo RenderPipeline = {};
        RenderPipeline.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        RenderPipeline.pColorAttachmentFormats = &Format;
        RenderPipeline.colorAttachmentCount = 1;

    	
        ImGui_ImplVulkan_InitInfo InitInfo = {};
    	InitInfo.ApiVersion = VK_API_VERSION_1_3;
		InitInfo.Allocator = VK_ALLOC_CALLBACK;
        InitInfo.PipelineRenderingCreateInfo = RenderPipeline;
        InitInfo.Instance = VulkanRenderContext->GetVulkanInstance();
        InitInfo.PhysicalDevice = VulkanRenderContext->GetDevice()->GetPhysicalDevice();
        InitInfo.Device = VulkanRenderContext->GetDevice()->GetDevice();
        InitInfo.Queue = VulkanRenderContext->GetQueue(ECommandQueue::Graphics)->Queue;
        InitInfo.DescriptorPool = DescriptorPool;
        InitInfo.MinImageCount = 2;
        InitInfo.ImageCount = 3;
        InitInfo.UseDynamicRendering = true;
        InitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		
        Assert(ImGui_ImplVulkan_Init(&InitInfo))
    }

    void FVulkanImGuiRender::Deinitialize()
    {
		VulkanRenderContext->WaitIdle();
		
    	for (auto& KVP : ImageCache)
    	{
    		ImGui_ImplVulkan_RemoveTexture(KVP.second);
    	}

		ImageCache.clear();
		
    	ImGui_ImplVulkan_Shutdown();
    	
    	vkDestroyDescriptorPool(VulkanRenderContext->GetDevice()->GetDevice(), DescriptorPool, VK_ALLOC_CALLBACK);
		DescriptorPool = nullptr;
    	
    	ImGui_ImplGlfw_Shutdown();
    	ImGui::DestroyContext();
    }

    void FVulkanImGuiRender::OnStartFrame(const FUpdateContext& UpdateContext)
    {
    	LUMINA_PROFILE_SCOPE();
		ReferencedImages.clear();
		
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    	ImGuizmo::BeginFrame();
    }
	
    void FVulkanImGuiRender::OnEndFrame(const FUpdateContext& UpdateContext, FRenderGraph& RenderGraph)
    {
    	LUMINA_PROFILE_SCOPE();

		FRGPassDescriptor* Descriptor = RenderGraph.AllocDescriptor();
		Descriptor->AddRawWrite(GEngine->GetEngineViewport()->GetRenderTarget());
		for (FRHIImage* Image : ReferencedImages)
		{
			Descriptor->AddRawRead(Image);
		}
		
		RenderGraph.AddPass<RG_Raster>(FRGEvent("ImGui Render"), Descriptor, [&] (ICommandList& CmdList)
		{
			LUMINA_PROFILE_SECTION_COLORED("ImGui Render", tracy::Color::Aquamarine3);
			if (ImDrawData* DrawData = ImGui::GetDrawData())
			{
				FRenderPassDesc::FAttachment Attachment; Attachment
					.SetImage(GEngine->GetEngineViewport()->GetRenderTarget())
					.SetLoadOp(ERenderLoadOp::Load);
			
		
				FRenderPassDesc RenderPass; RenderPass
				.AddColorAttachment(Attachment)
				.SetRenderArea(GEngine->GetEngineViewport()->GetRenderTarget()->GetExtent());
			
				for (FRHIImage* Image : ReferencedImages)
				{
					CmdList.SetImageState(Image, AllSubresources, EResourceStates::ShaderResource);
				}
			
				CmdList.CommitBarriers();
		
				CmdList.BeginRenderPass(RenderPass);
				{
					ImGui_ImplVulkan_RenderDrawData(DrawData, CmdList.GetAPIResource<VkCommandBuffer>());
				}
				CmdList.EndRenderPass();
			}
		});
    }

	void FVulkanImGuiRender::DrawRenderDebugInformationWindow(bool* bOpen, const FUpdateContext& Context)
	{
		ImGui::SetNextWindowSize(ImVec2(600, 800));
		if (!ImGui::Begin("Vulkan Render Information", bOpen))
		{
			ImGui::End();
			return;
		}
		
		VkPhysicalDevice physicalDevice = VulkanRenderContext->GetDevice()->GetPhysicalDevice();

		VkPhysicalDeviceFeatures Features;
		vkGetPhysicalDeviceFeatures(physicalDevice, &Features);
		
		VkPhysicalDeviceProperties props{};
		vkGetPhysicalDeviceProperties(physicalDevice, &props);
	
		VkPhysicalDeviceMemoryProperties memProps{};
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

    		VmaAllocator Allocator = VulkanRenderContext->GetDevice()->GetAllocator()->GetVMA();
    	
		ImGui::SeparatorText("Device Properties");

		if (ImGui::CollapsingHeader("Device Info"))
		{
			if (ImGui::BeginTable("VulkanDeviceInfo", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
			{
				auto Label = [](const char* name, auto value)
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(name);
					ImGui::TableSetColumnIndex(1); ImGui::Text("%s", value);
				};
	
				Label("Device Name", props.deviceName);
				Label("Device Type", 
					props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "Discrete GPU" :
					props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? "Integrated GPU" :
					props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU ? "CPU" : "Other");
	
				Label("Vendor ID", std::format("0x{:04X}", props.vendorID).c_str());
				Label("Device ID", std::format("0x{:04X}", props.deviceID).c_str());
	
				Label("API Version", std::format("{}.{}.{}", 
					VK_VERSION_MAJOR(props.apiVersion), 
					VK_VERSION_MINOR(props.apiVersion), 
					VK_VERSION_PATCH(props.apiVersion)).c_str());
	
				Label("Driver Version", std::format("0x{:X}", props.driverVersion).c_str());
	
				Label("Max Image Dimension 2D", std::format("{} px", props.limits.maxImageDimension2D).c_str());
				Label("Max Image Array Layers", std::format("{}", props.limits.maxImageArrayLayers).c_str());
				Label("Max Uniform Buffer Range", std::format("{} bytes", props.limits.maxUniformBufferRange).c_str());
				Label("Max Storage Buffer Range", std::format("{} bytes", props.limits.maxStorageBufferRange).c_str());
				Label("Max Push Constants Size", std::format("{} bytes", props.limits.maxPushConstantsSize).c_str());
				Label("Max Bound Descriptor Sets", std::format("{}", props.limits.maxBoundDescriptorSets).c_str());
				Label("Max Sampler Allocation Count", std::format("{}", props.limits.maxSamplerAllocationCount).c_str());
				Label("Max Memory Allocation Count", std::format("{}", props.limits.maxMemoryAllocationCount).c_str());
				Label("Buffer-Image Granularity", std::format("{} bytes", props.limits.bufferImageGranularity).c_str());
				Label("Non-Coherent Atom Size", std::format("{} bytes", props.limits.nonCoherentAtomSize).c_str());
				Label("Min Memory Map Alignment", std::format("{} bytes", props.limits.minMemoryMapAlignment).c_str());

				Label("Min Texel Buffer Offset Align", std::format("{} bytes", props.limits.minTexelBufferOffsetAlignment).c_str());
				Label("Min Uniform Buffer Offset Align", std::format("{} bytes", props.limits.minUniformBufferOffsetAlignment).c_str());
				Label("Min Storage Buffer Offset Align", std::format("{} bytes", props.limits.minStorageBufferOffsetAlignment).c_str());
				Label("Optimal Buffer Copy Offset Align", std::format("{} bytes", props.limits.optimalBufferCopyOffsetAlignment).c_str());
				Label("Optimal Buffer Row Pitch Align", std::format("{} bytes", props.limits.optimalBufferCopyRowPitchAlignment).c_str());

				Label("Max Per-Stage Resources", std::format("{}", props.limits.maxPerStageResources).c_str());
				Label("Max Per-Stage Samplers", std::format("{}", props.limits.maxPerStageDescriptorSamplers).c_str());
				Label("Max Per-Stage Uniform Buffers", std::format("{}", props.limits.maxPerStageDescriptorUniformBuffers).c_str());
				Label("Max Per-Stage Storage Buffers", std::format("{}", props.limits.maxPerStageDescriptorStorageBuffers).c_str());
				Label("Max Per-Stage Sampled Images", std::format("{}", props.limits.maxPerStageDescriptorSampledImages).c_str());
				Label("Max Descriptor Set Sampled Images", std::format("{}", props.limits.maxDescriptorSetSampledImages).c_str());

				Label("Max Compute Work Group Count", std::format("{} x {} x {}", props.limits.maxComputeWorkGroupCount[0], props.limits.maxComputeWorkGroupCount[1], props.limits.maxComputeWorkGroupCount[2]).c_str());
				Label("Max Compute Work Group Size",  std::format("{} x {} x {}",  props.limits.maxComputeWorkGroupSize[0], props.limits.maxComputeWorkGroupSize[1], props.limits.maxComputeWorkGroupSize[2]).c_str());
				Label("Max Compute Shared Memory", std::format("{} bytes", props.limits.maxComputeSharedMemorySize).c_str());
				Label("Max Compute Invocations", std::format("{}", props.limits.maxComputeWorkGroupInvocations).c_str());
			

				Label("Max Viewports", std::format("{}", props.limits.maxViewports).c_str());
				Label("Max Viewport Dimensions", std::format("{} x {}", props.limits.maxViewportDimensions[0], props.limits.maxViewportDimensions[1]).c_str());
				Label("Viewport Bounds Range", std::format("[{}, {}]", props.limits.viewportBoundsRange[0], props.limits.viewportBoundsRange[1]).c_str());
				Label("Subpixel Precision Bits", std::format("{}", props.limits.subPixelPrecisionBits).c_str());
				Label("Line Width Range", std::format("[{}, {}]", props.limits.lineWidthRange[0], props.limits.lineWidthRange[1]).c_str());
				Label("Point Size Range", std::format("[{}, {}]", props.limits.pointSizeRange[0], props.limits.pointSizeRange[1]).c_str());

				Label("Max Framebuffer Width", std::format("{}", props.limits.maxFramebufferWidth).c_str());
				Label("Max Framebuffer Height", std::format("{}", props.limits.maxFramebufferHeight).c_str());
				Label("Max Framebuffer Layers", std::format("{}", props.limits.maxFramebufferLayers).c_str());
				Label("Max Color Attachments", std::format("{}", props.limits.maxColorAttachments).c_str());
				Label("Framebuffer Color Sample Counts", std::format("0x{:X}", props.limits.framebufferColorSampleCounts).c_str());

				Label("Timestamp Period", std::format("{:.3f} ns", props.limits.timestampPeriod).c_str());
				Label("Timestamp Compute & Graphics", props.limits.timestampComputeAndGraphics ? "Yes" : "No");
				Label("Discrete Queue Priorities", std::format("{}", props.limits.discreteQueuePriorities).c_str());

				
				ImGui::EndTable();
			}
		}

		ImGui::Spacing();


		if (ImGui::CollapsingHeader("Device Features"))
		{
			if (ImGui::BeginTable("VulkanFeatures", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
			{
				auto Feature = [](const char* name, VkBool32 value)
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(name);
					ImGui::TableSetColumnIndex(1);
					ImGui::TextColored(value ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f)
					                         : ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
						value ? "Yes" : "No");
				};
		
				const VkPhysicalDeviceFeatures& f = Features;
		
				Feature("robustBufferAccess", f.robustBufferAccess);
				Feature("fullDrawIndexUint32", f.fullDrawIndexUint32);
				Feature("imageCubeArray", f.imageCubeArray);
				Feature("independentBlend", f.independentBlend);
				Feature("geometryShader", f.geometryShader);
				Feature("tessellationShader", f.tessellationShader);
				Feature("sampleRateShading", f.sampleRateShading);
				Feature("dualSrcBlend", f.dualSrcBlend);
				Feature("logicOp", f.logicOp);
				Feature("multiDrawIndirect", f.multiDrawIndirect);
				Feature("drawIndirectFirstInstance", f.drawIndirectFirstInstance);
				Feature("depthClamp", f.depthClamp);
				Feature("depthBiasClamp", f.depthBiasClamp);
				Feature("fillModeNonSolid", f.fillModeNonSolid);
				Feature("depthBounds", f.depthBounds);
				Feature("wideLines", f.wideLines);
				Feature("largePoints", f.largePoints);
				Feature("alphaToOne", f.alphaToOne);
				Feature("multiViewport", f.multiViewport);
				Feature("samplerAnisotropy", f.samplerAnisotropy);
				Feature("textureCompressionETC2", f.textureCompressionETC2);
				Feature("textureCompressionASTC_LDR", f.textureCompressionASTC_LDR);
				Feature("textureCompressionBC", f.textureCompressionBC);
				Feature("occlusionQueryPrecise", f.occlusionQueryPrecise);
				Feature("pipelineStatisticsQuery", f.pipelineStatisticsQuery);
				Feature("vertexPipelineStoresAndAtomics", f.vertexPipelineStoresAndAtomics);
				Feature("fragmentStoresAndAtomics", f.fragmentStoresAndAtomics);
				Feature("shaderTessellationAndGeometryPointSize", f.shaderTessellationAndGeometryPointSize);
				Feature("shaderImageGatherExtended", f.shaderImageGatherExtended);
				Feature("shaderStorageImageExtendedFormats", f.shaderStorageImageExtendedFormats);
				Feature("shaderStorageImageMultisample", f.shaderStorageImageMultisample);
				Feature("shaderStorageImageReadWithoutFormat", f.shaderStorageImageReadWithoutFormat);
				Feature("shaderStorageImageWriteWithoutFormat", f.shaderStorageImageWriteWithoutFormat);
				Feature("shaderUniformBufferArrayDynamicIndexing", f.shaderUniformBufferArrayDynamicIndexing);
				Feature("shaderSampledImageArrayDynamicIndexing", f.shaderSampledImageArrayDynamicIndexing);
				Feature("shaderStorageBufferArrayDynamicIndexing", f.shaderStorageBufferArrayDynamicIndexing);
				Feature("shaderStorageImageArrayDynamicIndexing", f.shaderStorageImageArrayDynamicIndexing);
				Feature("shaderClipDistance", f.shaderClipDistance);
				Feature("shaderCullDistance", f.shaderCullDistance);
				Feature("shaderFloat64", f.shaderFloat64);
				Feature("shaderInt64", f.shaderInt64);
				Feature("shaderInt16", f.shaderInt16);
				Feature("shaderResourceResidency", f.shaderResourceResidency);
				Feature("shaderResourceMinLod", f.shaderResourceMinLod);
				Feature("sparseBinding", f.sparseBinding);
				Feature("sparseResidencyBuffer", f.sparseResidencyBuffer);
				Feature("sparseResidencyImage2D", f.sparseResidencyImage2D);
				Feature("sparseResidencyImage3D", f.sparseResidencyImage3D);
				Feature("sparseResidency2Samples", f.sparseResidency2Samples);
				Feature("sparseResidency4Samples", f.sparseResidency4Samples);
				Feature("sparseResidency8Samples", f.sparseResidency8Samples);
				Feature("sparseResidency16Samples", f.sparseResidency16Samples);
				Feature("sparseResidencyAliased", f.sparseResidencyAliased);
				Feature("variableMultisampleRate", f.variableMultisampleRate);
				Feature("inheritedQueries", f.inheritedQueries);
		
				ImGui::EndTable();
			}
		}

		ImGui::Spacing();


		if (ImGui::CollapsingHeader("Allocations"))
		{
			// Draw a second table for live RHI allocations
			if (ImGui::BeginTable("RHIResourceTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
			{
			
				ImGui::TableSetupColumn("Type");
				ImGui::TableSetupColumn("Count");
				ImGui::TableHeadersRow();

				THashMap<ERHIResourceType, TVector<IRHIResource*>> ResourceMap;
				ResourceMap.reserve(RRT_Num);

				for (IRHIResource* Resource : IRHIResource::GetAllRHIResources())
				{
					if (Resource != nullptr)
					{
						ResourceMap[Resource->GetResourceType()].push_back(Resource);
					}
				}

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted("Total");
				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%zu", GTotalRenderResourcesAllocated);

				// Display each type and its count
				for (int type = (int)RRT_None + 1; type < (int)RRT_Num; ++type)
				{
					auto it = ResourceMap.find((ERHIResourceType)type);
					if (it == ResourceMap.end() || it->second.empty())
					{
						continue;
					}

					const auto& list = it->second;
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("%s", GetRHIResourceTypeName((ERHIResourceType)type));
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%zu", list.size());
				}

				ImGui::EndTable();
			}
		}

    	
    	ImGui::Spacing();

		if (ImGui::CollapsingHeader("Swapchain Info"))
		{
			if (FVulkanSwapchain* Swapchain = VulkanRenderContext->GetSwapchain())
			{
				const VkSurfaceFormatKHR& surfaceFormat = Swapchain->GetSurfaceFormat();
				const FIntVector2D& extent = Swapchain->GetSwapchainExtent();
				VkPresentModeKHR presentMode = Swapchain->GetPresentMode();
				uint32 FramesInFlight = Swapchain->GetNumFramesInFlight();
				uint32 imageCount = Swapchain->GetImageCount();
				uint32 currentImageIndex = Swapchain->GetCurrentImageIndex();

				if (ImGui::BeginTable("SwapchainTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
				{
					auto Row = [](const char* label, const std::string& value)
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(label);
						ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(value.c_str());
					};

					Row("Resolution", std::format("{}x{}", extent.X, extent.Y));
					Row("Frames In Flight", std::format("{}", FramesInFlight));
					Row("Image Count", std::format("{}", imageCount));
					Row("Current Image Index", std::format("{}", currentImageIndex));
					Row("Present Mode", std::format("{}", presentMode == VK_PRESENT_MODE_FIFO_KHR ? "FIFO (VSync)"
													   : presentMode == VK_PRESENT_MODE_MAILBOX_KHR ? "Mailbox (Triple Buffer)"
													   : presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR ? "Immediate (No VSync)"
													   : "Unknown"));
					Row("Format", std::format("{}", VkFormatToString(surfaceFormat.format).c_str()));
					Row("Color Space", std::format("{}", VkColorSpaceToString(surfaceFormat.colorSpace).c_str()));

					ImGui::EndTable();
				}
			}
			else
			{
				ImGui::Text("No swapchain available.");
			}
		}
	
		ImGui::Spacing();
		ImGui::SeparatorText("Memory Heaps and Types");
	
		if (ImGui::BeginTable("VulkanMemoryInfo", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
		{
			ImGui::TableSetupColumn("Heap");
			ImGui::TableSetupColumn("Size (MB)");
			ImGui::TableSetupColumn("Flags");
			ImGui::TableHeadersRow();
	
			for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i)
			{
				const VkMemoryHeap& heap = memProps.memoryHeaps[i];
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::Text("Heap %u", i);
				ImGui::TableSetColumnIndex(1); ImGui::Text("%.2f MB", heap.size / (1024.0f * 1024.0f));
				ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(
					heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ? "Device Local" : "Host Visible");
			}
	
			ImGui::EndTable();
		}
	

		ImGui::Spacing();

		if (ImGui::CollapsingHeader("Vulkan Memory Allocator"))
		{
			// VMA Stats
			VmaTotalStatistics stats{};
			vmaCalculateStatistics(Allocator, &stats);
		
			ImGui::Spacing();

			if (ImGui::BeginTable("VmaHeapDetails", 10, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
			{
				ImGui::TableSetupColumn("Heap");
				ImGui::TableSetupColumn("Blocks");
				ImGui::TableSetupColumn("Allocs");
				ImGui::TableSetupColumn("Used (MB)");
				ImGui::TableSetupColumn("Unused (MB)");
				ImGui::TableSetupColumn("Min Alloc (KB)");
				ImGui::TableSetupColumn("Max Alloc (MB)");
				ImGui::TableSetupColumn("Free Ranges");
				ImGui::TableSetupColumn("Min Empty (KB)");
				ImGui::TableSetupColumn("Max Empty (MB)");
				ImGui::TableHeadersRow();

				for (uint32_t i = 0; i < VK_MAX_MEMORY_HEAPS; ++i)
				{
					const VmaDetailedStatistics& heap = stats.memoryHeap[i];
					if (heap.statistics.blockCount == 0 && heap.statistics.allocationCount == 0)
					{
						continue;
					}

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0); ImGui::Text("Heap %u", i);
					ImGui::TableSetColumnIndex(1); ImGui::Text("%u", heap.statistics.blockCount);
					ImGui::TableSetColumnIndex(2); ImGui::Text("%u", heap.statistics.allocationCount);
					ImGui::TableSetColumnIndex(3); ImGui::Text("%.2f", heap.statistics.allocationBytes / (1024.0f * 1024.0f)); // used
					ImGui::TableSetColumnIndex(4); ImGui::Text("%.2f", (heap.statistics.blockBytes - heap.statistics.allocationBytes) / (1024.0f * 1024.0f));
					ImGui::TableSetColumnIndex(5); ImGui::Text(heap.allocationSizeMin == VK_WHOLE_SIZE ? "-" : "%.2f", heap.allocationSizeMin / 1024.0f);
					ImGui::TableSetColumnIndex(6); ImGui::Text("%.2f", heap.allocationSizeMax / (1024.0f * 1024.0f));
					ImGui::TableSetColumnIndex(7); ImGui::Text("%u", heap.unusedRangeCount);
					ImGui::TableSetColumnIndex(8); ImGui::Text(heap.unusedRangeSizeMin == VK_WHOLE_SIZE ? "-" : "%.2f", heap.unusedRangeSizeMin / 1024.0f);
					ImGui::TableSetColumnIndex(9); ImGui::Text("%.2f", heap.unusedRangeSizeMax / (1024.0f * 1024.0f));

				}
				ImGui::EndTable();
			}
		}

		ImGui::Spacing();
	
		if (ImGui::Button("Close"))
		{
			*bOpen = false;
		}

		ImGui::End();
	}


    ImTextureID FVulkanImGuiRender::GetOrCreateImTexture(FRHIImageRef Image)
    {
    	LUMINA_PROFILE_SCOPE();
		FScopeLock Lock(TextureMutex);
		
    	if(Image == nullptr)
    	{
    		return 0;
    	}
		
    	
		ReferencedImages.push_back(Image);
	    VkImage VulkanImage = Image->GetAPIResource<VkImage>();
		
		const FTextureSubresourceSet Subresource = AllSubresources;
		FVulkanImage::ESubresourceViewType ViewType = GetTextureViewType(EFormat::UNKNOWN, Image->GetDescription().Format);
		VkImageView View = Image.As<FVulkanImage>()->GetSubresourceView(Subresource, Image->GetDescription().Dimension, Image->GetDescription().Format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, ViewType).View;
		
    	auto It = ImageCache.find(VulkanImage);
		
    	if (It != ImageCache.end())
    	{
    		return (intptr_t)It->second;
    	}

    	FRHISamplerRef Sampler = TStaticRHISampler<>::GetRHI();
		
    	VkSampler VulkanSampler = Sampler->GetAPIResource<VkSampler>();
    	
    	VkDescriptorSet DescriptorSet = ImGui_ImplVulkan_AddTexture(VulkanSampler, View, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    	ImageCache.insert_or_assign(VulkanImage, DescriptorSet);

    	return (intptr_t)DescriptorSet;
    	
    }

    void FVulkanImGuiRender::DestroyImTexture(ImTextureRef Image)
    {
		FScopeLock Lock(TextureMutex);
		VkDescriptorSet TargetDescriptor = (VkDescriptorSet)Image.GetTexID();

		ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)Image.GetTexID());
		for (auto it = ImageCache.begin(); it != ImageCache.end(); )
		{
			if (it->second == TargetDescriptor)
			{
				it = ImageCache.erase(it);
				break;
			}
			
			++it;
		}
    }
}
