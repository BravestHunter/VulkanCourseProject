#include "VulkanRenderer.h"

#include <algorithm>
#include <set>


int VulkanRenderer::Init(GLFWwindow* window)
{
	window_ = window;
	try
	{
		CreateVkInstance();
		CreateSurface(window);
		GetPhysicalDevice();
		//AllocateDynamicBufferTransferSpace();
		CreateLogicalDevice();
		CreateSwapchain();
		CreateRenderPass();
		CreateDescriptorSetLayout();
		CreatePushConstantRange();
		CreateCraphicsPipeline();
		CreateColorBufferImages();
		CreateDepthBufferImages();
		CreateFramebuffers();
		CreateCommandPool();
		CreateCommandBuffers();
		CreateTextureSampler();
		CreateUniformBuffers();
		CreateDescriptorPools();
		CreateDescriptorSets();
		CreateInputDescriptorSets();
		CreateSynchronization();

		CreateAssets();

		uboViewProjection_.projection = glm::perspective(glm::radians(45.0f), (float)swapchainExtent_.width / swapchainExtent_.height, 0.1f, 1000.0f);
		uboViewProjection_.projection[1][1] *= -1.0f;
		uboViewProjection_.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	}
	catch (const std::runtime_error &e)
	{
		printf("Error: %s\n", e.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

void VulkanRenderer::Deinit()
{
	vkDeviceWaitIdle(mainDevice.logicalDevice);

	//_aligned_free(modelTransferSpace_);

	for (auto& model : models_)
	{
		model.Destroy();
	}

	for (size_t i = 0; i < textureImages_.size(); i++)
	{
		vkDestroyImageView(mainDevice.logicalDevice, textureImageViews_[i], nullptr);
		vkDestroyImage(mainDevice.logicalDevice, textureImages_[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, textureImagesMemory_[i], nullptr);
	}

	vkDestroyDescriptorPool(mainDevice.logicalDevice, inputDescriptorPool_, nullptr);
	vkDestroyDescriptorPool(mainDevice.logicalDevice, samplerDescriptorPool_, nullptr);
	vkDestroyDescriptorPool(mainDevice.logicalDevice, descriptorPool_, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, inputDescriptorSetLayout_, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, samplerDescriptorSetLayout_, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, descriptorSetLayout_, nullptr);
	for (size_t i = 0; i < swapchainImages_.size(); i++)
	{
		vkDestroyBuffer(mainDevice.logicalDevice, vpUniformBuffer_[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, vpUniformBufferMemory_[i], nullptr);

		//vkDestroyBuffer(mainDevice.logicalDevice, modelDynamicUniformBuffer_[i], nullptr);
		//vkFreeMemory(mainDevice.logicalDevice, modelDynamicUniformBufferMemory_[i], nullptr);
	}

	vkDestroySampler(mainDevice.logicalDevice, textureSampler_, nullptr);

	for (size_t i = 0; i < MAX_FRAME_DRAWS; i++)
	{
		vkDestroySemaphore(mainDevice.logicalDevice, renderFinishedSemaphores_[i], nullptr);
		vkDestroySemaphore(mainDevice.logicalDevice, imageAvailableSemaphores_[i], nullptr);
		vkDestroyFence(mainDevice.logicalDevice, drawFences_[i], nullptr);
	}

	for (VkFramebuffer framebuffer : swapchainFramebuffers_)
	{
		vkDestroyFramebuffer(mainDevice.logicalDevice, framebuffer, nullptr);
	}

	for (size_t i = 0; i < depthBufferImages_.size(); i++)
	{
		vkDestroyImageView(mainDevice.logicalDevice, depthBufferImageViews_[i], nullptr);
		vkDestroyImage(mainDevice.logicalDevice, depthBufferImages_[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, depthBufferImagesMemory_[i], nullptr);
	}

	for (size_t i = 0; i < colorBufferImages_.size(); i++)
	{
		vkDestroyImageView(mainDevice.logicalDevice, colorBufferImageViews_[i], nullptr);
		vkDestroyImage(mainDevice.logicalDevice, colorBufferImages_[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, colorBufferImagesMemory_[i], nullptr);
	}

	vkDestroyCommandPool(mainDevice.logicalDevice, graphicsCommandPool_, nullptr);

	vkDestroyPipeline(mainDevice.logicalDevice, secondPipeline_, nullptr);
	vkDestroyPipeline(mainDevice.logicalDevice, graphicsPipeline_, nullptr);
	vkDestroyPipelineLayout(mainDevice.logicalDevice, secondPipelineLayout_, nullptr);
	vkDestroyPipelineLayout(mainDevice.logicalDevice, pipelineLayout_, nullptr);
	vkDestroyRenderPass(mainDevice.logicalDevice, renderPass_, nullptr);

	for (SwapchainImage image : swapchainImages_)
	{
		vkDestroyImageView(mainDevice.logicalDevice, image.imageView, nullptr);
	}

	vkDestroySwapchainKHR(mainDevice.logicalDevice, swapchain_, nullptr);
	vkDestroySurfaceKHR(vkInsatance_, surface_, nullptr);
	vkDestroyDevice(mainDevice.logicalDevice, nullptr);
	vkDestroyInstance(vkInsatance_, nullptr);
}


void VulkanRenderer::Draw()
{
	vkWaitForFences(mainDevice.logicalDevice, 1, &drawFences_[currentFrame_], VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(mainDevice.logicalDevice, 1, &drawFences_[currentFrame_]);

	uint32_t imageIndex;
	vkAcquireNextImageKHR(mainDevice.logicalDevice, swapchain_, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphores_[currentFrame_], VK_NULL_HANDLE, &imageIndex);

	RecordCommands(imageIndex);
	UpdateUniformBuffers(imageIndex);

	VkPipelineStageFlags waitStageFlags[]
	{
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};

	VkSubmitInfo submitInfo
	{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &imageAvailableSemaphores_[currentFrame_],
		.pWaitDstStageMask = waitStageFlags,
		.commandBufferCount = 1,
		.pCommandBuffers = &commandBuffers_[imageIndex],
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &renderFinishedSemaphores_[currentFrame_]
	};

	VkResult result = vkQueueSubmit(graphicsQueue_, 1, &submitInfo, drawFences_[currentFrame_]);
	if (result != VK_SUCCESS) 
	{
		throw std::runtime_error("Failed to submit command buffer to queue.");
	}

	VkPresentInfoKHR presentInfo
	{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &renderFinishedSemaphores_[currentFrame_],
		.swapchainCount = 1,
		.pSwapchains = &swapchain_,
		.pImageIndices = &imageIndex
	};

	result = vkQueuePresentKHR(graphicsQueue_, &presentInfo);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to present image.");
	}

	currentFrame_ = (currentFrame_ + 1) % MAX_FRAME_DRAWS; 
}


void VulkanRenderer::UpdateModel(const int& index, const glm::mat4& model)
{
	if (index < 0 || index >= models_.size())
	{
		return;
	}

	models_[index].SetModel({ model });
}


void VulkanRenderer::CreateVkInstance()
{
	VkApplicationInfo appInfo
	{
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "Vulcan application",
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		.pEngineName = "No engine",
		.engineVersion = VK_MAKE_VERSION(0, 0, 0),
		.apiVersion = VK_API_VERSION_1_3
	};

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	std::vector<const char*> instanceExtensions;
	for (size_t i = 0; i < glfwExtensionCount; i++)
	{
		instanceExtensions.push_back(glfwExtensions[i]);
	}

	if (CheckInstanceExtensionSupport(instanceExtensions) == false)
	{
		throw std::runtime_error("Vk instance does not support required extensions.");
	}

#ifdef _DEBUG
	const std::vector<const char*> validationLayers = 
	{
		"VK_LAYER_KHRONOS_validation"
	};
	if (CheckValidationLayerSupport(validationLayers) == false) 
	{
		throw std::runtime_error("Vk instance does not support required validation layers.");
	}
#endif // !_DEBUG

	VkInstanceCreateInfo createInfo
	{
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &appInfo,
#ifndef _DEBUG
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = nullptr,
#else
		.enabledLayerCount = static_cast<uint32_t>(validationLayers.size()),
		.ppEnabledLayerNames = validationLayers.data(),
#endif
		.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size()),
		.ppEnabledExtensionNames = instanceExtensions.data()
	};

	VkResult result = vkCreateInstance(&createInfo, nullptr, &vkInsatance_);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create vk instance.");
	}
}

void VulkanRenderer::GetPhysicalDevice()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(vkInsatance_, &deviceCount, nullptr);

	if (deviceCount == 0)
	{
		throw std::runtime_error("Can't find GPUs that support Vulkan instance.");
	}

	std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
	vkEnumeratePhysicalDevices(vkInsatance_, &deviceCount, physicalDevices.data());

	for (const VkPhysicalDevice device : physicalDevices)
	{
		if (CheckPhysicalDeviceSuitable(device))
		{
			mainDevice.physicalDevice = device;
			break;
		}
	}

	//VkPhysicalDeviceProperties properties;
	//vkGetPhysicalDeviceProperties(mainDevice.physicalDevice, &properties);
	//minUniformBufferOffset_ = properties.limits.minUniformBufferOffsetAlignment;
}

void VulkanRenderer::CreateLogicalDevice()
{
	QueueFamilyIndices indices = GetQueueFamilies(mainDevice.physicalDevice);
	float priority = 1.0f;

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<int> queueFamilyIndices = { indices.graphicsFamily, indices.presentationFamily };
	for (const int queueFamilyIndex : queueFamilyIndices)
	{
		VkDeviceQueueCreateInfo queueCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = static_cast<uint32_t>(queueFamilyIndex),
			.queueCount = 1,
			.pQueuePriorities = &priority
		};

		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures
	{
		.samplerAnisotropy = VK_TRUE
	};

	VkDeviceCreateInfo deviceCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
		.pQueueCreateInfos = queueCreateInfos.data(),
		.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
		.ppEnabledExtensionNames = deviceExtensions.data(),
		.pEnabledFeatures = &deviceFeatures
	};

	VkResult result = vkCreateDevice(mainDevice.physicalDevice, &deviceCreateInfo, nullptr, &mainDevice.logicalDevice);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a logical device.");
	}

	vkGetDeviceQueue(mainDevice.logicalDevice, static_cast<uint32_t>(indices.graphicsFamily), 0, &graphicsQueue_);
	vkGetDeviceQueue(mainDevice.logicalDevice, static_cast<uint32_t>(indices.presentationFamily), 0, &presentationQueue_);
}

void VulkanRenderer::CreateSurface(GLFWwindow* window)
{
	VkResult result = glfwCreateWindowSurface(vkInsatance_, window, nullptr, &surface_);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a surface.");
	}
}

void VulkanRenderer::CreateSwapchain()
{
	SwapchainDetails swapchainDetails = GetSwapchainDetails(mainDevice.physicalDevice);

	VkSurfaceFormatKHR surfaceFormat = ChooseBestSurfaceFormat(swapchainDetails.formats);
	VkPresentModeKHR presentMode = ChooseBestPresentationMode(swapchainDetails.presentationModes);
	VkExtent2D extent = ChooseSwapExtent(swapchainDetails.surfaceCapabilities);

	uint32_t imageCount = swapchainDetails.surfaceCapabilities.minImageCount + 1; // At least one extra image for allowing triple buffering for sure
	if (swapchainDetails.surfaceCapabilities.maxImageCount > 0 &&  swapchainDetails.surfaceCapabilities.maxImageCount < imageCount)
	{
		imageCount = swapchainDetails.surfaceCapabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR sawpChainCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = surface_,
		.minImageCount = imageCount,
		.imageFormat = surfaceFormat.format,
		.imageColorSpace = surfaceFormat.colorSpace,
		.imageExtent = extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,

		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,

		.preTransform = swapchainDetails.surfaceCapabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.clipped = VK_TRUE,
		.oldSwapchain = VK_NULL_HANDLE
	};

	QueueFamilyIndices indices = GetQueueFamilies(mainDevice.physicalDevice);
	if (indices.graphicsFamily != indices.presentationFamily)
	{
		uint32_t queueFamiliIndices[] =
		{
			static_cast<uint32_t>(indices.graphicsFamily),
			static_cast<uint32_t>(indices.presentationFamily)
		};

		sawpChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		sawpChainCreateInfo.queueFamilyIndexCount = 2;
		sawpChainCreateInfo.pQueueFamilyIndices = queueFamiliIndices;
	}

	VkResult result = vkCreateSwapchainKHR(mainDevice.logicalDevice, &sawpChainCreateInfo, nullptr, &swapchain_);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a swapchain.");
	}

	swapchainImageFormat_ = surfaceFormat.format;
	swapchainExtent_ = extent;

	uint32_t swapchainImageCount;
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain_, &swapchainImageCount, nullptr);
	std::vector<VkImage> images(swapchainImageCount);
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain_, &swapchainImageCount, images.data());

	for (VkImage image : images)
	{
		SwapchainImage swapchainImage
		{
			.image = image,
			.imageView = CreateImageView(image, swapchainImageFormat_, VK_IMAGE_ASPECT_COLOR_BIT)
		};

		swapchainImages_.push_back(swapchainImage);
	}
}

void VulkanRenderer::CreateRenderPass()
{
	colorBufferImageFormat_ = ChooseSupportedFormat(
		{ VK_FORMAT_R8G8B8A8_UNORM },
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	);

	depthBufferImageFormat_ = ChooseSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);

	// Color attachment
	VkAttachmentDescription colorAttachment
	{
		.format = colorBufferImageFormat_,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};
	// Depth attachment
	VkAttachmentDescription depthAttachment
	{
		.format = depthBufferImageFormat_,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	};

	VkAttachmentReference colorAttachmentReference
	{
		.attachment = 1,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};
	VkAttachmentReference depthAttachmentReference
	{
		.attachment = 2,
		.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	};

	// Swapchain color attachment
	VkAttachmentDescription swapchainColorAttachment
	{
		.format = swapchainImageFormat_,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	};

	VkAttachmentReference swapchainColorAttachmentReference
	{
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};

	std::vector<VkAttachmentReference> inputReferences 
	{ 
		VkAttachmentReference
		{
			.attachment = 1,
			.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		},
		VkAttachmentReference
		{
			.attachment = 2,
			.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		}
	};

	// Color attachment
	//VkAttachmentDescription colorAttachment
	//{
	//	.format = swapchainImageFormat_,
	//	.samples = VK_SAMPLE_COUNT_1_BIT,
	//	.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
	//	.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
	//	.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	//	.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
	//	.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	//	.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	//};
	//// Depth attachment
	//VkAttachmentDescription depthAttachment
	//{
	//	.format = depthBufferImageFormat_,
	//	.samples = VK_SAMPLE_COUNT_1_BIT,
	//	.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
	//	.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
	//	.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	//	.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
	//	.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	//	.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	//};
	//std::vector<VkAttachmentDescription> renderPassAttachments { colorAttachment , depthAttachment };
	//
	//VkAttachmentReference colorAttachmentReference
	//{
	//	.attachment = 0,
	//	.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	//};
	//VkAttachmentReference depthAttachmentReference
	//{
	//	.attachment = 1,
	//	.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	//};

	std::vector<VkSubpassDescription> subpassDescriptions
	{
		VkSubpassDescription
		{
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAttachmentReference,
			.pDepthStencilAttachment = &depthAttachmentReference
		},
		VkSubpassDescription
		{
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount = static_cast<uint32_t>(inputReferences.size()),
			.pInputAttachments = inputReferences.data(),
			.colorAttachmentCount = 1,
			.pColorAttachments = &swapchainColorAttachmentReference
		}
	};

	std::vector<VkSubpassDependency> subpassDependencies =
	{
		// Conversion form VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		VkSubpassDependency
		{
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			//.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
			.dependencyFlags = 0
		},
		// Conversion form VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		VkSubpassDependency
		{
			.srcSubpass = 0,
			.dstSubpass = 1,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			//.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
			.dependencyFlags = 0
		},
		VkSubpassDependency
		{
			.srcSubpass = 1,
			.dstSubpass = VK_SUBPASS_EXTERNAL,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
			//.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
			.dependencyFlags = 0
		}
	};

	std::vector<VkAttachmentDescription> renderPassAttachments{ swapchainColorAttachment, colorAttachment , depthAttachment };

	VkRenderPassCreateInfo renderPassCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = static_cast<uint32_t>(renderPassAttachments.size()),
		.pAttachments = renderPassAttachments.data(),
		.subpassCount = static_cast<uint32_t>(subpassDescriptions.size()),
		.pSubpasses = subpassDescriptions.data(),
		.dependencyCount = static_cast<uint32_t>(subpassDependencies.size()),
		.pDependencies = subpassDependencies.data()
	};

	VkResult result = vkCreateRenderPass(mainDevice.logicalDevice, &renderPassCreateInfo, nullptr, &renderPass_);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a render pass.");
	}
}

void VulkanRenderer::CreateDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding vpLayoutBinding
	{
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.pImmutableSamplers = nullptr
	};
	//VkDescriptorSetLayoutBinding modelLayoutBinding
	//{
	//	.binding = 1,
	//	.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
	//	.descriptorCount = 1,
	//	.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
	//	.pImmutableSamplers = nullptr
	//};
	std::vector<VkDescriptorSetLayoutBinding> layoutBindings { vpLayoutBinding/*, modelLayoutBinding*/ };

	VkDescriptorSetLayoutCreateInfo vpDescriptorSetLayoutCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = static_cast<uint32_t>(layoutBindings.size()),
		.pBindings = layoutBindings.data()
	};

	VkResult result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &vpDescriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout_);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a descriptor set.");
	}


	VkDescriptorSetLayoutBinding samplerLayoutBinding
	{
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr
	};

	VkDescriptorSetLayoutCreateInfo samplerDescriptorSetLayoutCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 1,
		.pBindings = &samplerLayoutBinding
	};

	result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &samplerDescriptorSetLayoutCreateInfo, nullptr, &samplerDescriptorSetLayout_);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a descriptor set.");
	}


	std::vector<VkDescriptorSetLayoutBinding> inputDescriptorSetLayoutBindings
	{
		// Color input binding
		VkDescriptorSetLayoutBinding
		{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = nullptr
		},
		// Depth input binding
		VkDescriptorSetLayoutBinding
		{
			.binding = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = nullptr
		}
	};

	VkDescriptorSetLayoutCreateInfo inputDescriptorSetLayoutCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = static_cast<uint32_t>(inputDescriptorSetLayoutBindings.size()),
		.pBindings = inputDescriptorSetLayoutBindings.data()
	};

	result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &inputDescriptorSetLayoutCreateInfo, nullptr, &inputDescriptorSetLayout_);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a descriptor set.");
	}
}

void VulkanRenderer::CreatePushConstantRange()
{
	pushConstantRange_ = VkPushConstantRange
	{
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.offset = 0,
		.size = sizeof(Model)
	};
}

void VulkanRenderer::CreateCraphicsPipeline()
{
	std::vector<char> vertexShaderCode = readBinaryFile("../shaders/vert.spv");
	std::vector<char> fragmentShaderCode = readBinaryFile("../shaders/frag.spv");

	VkShaderModule vertexShaderModule = CreateShaderModule(vertexShaderCode);
	VkShaderModule fragmentShaderModule = CreateShaderModule(fragmentShaderCode);

	VkPipelineShaderStageCreateInfo vertexShaderCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.module = vertexShaderModule,
		.pName = "main"
	};
	VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = fragmentShaderModule,
		.pName = "main"
	};

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertexShaderCreateInfo, fragmentShaderCreateInfo };


	uint32_t binding = 0;

	VkVertexInputBindingDescription bindingDescription
	{
		.binding = binding,
		.stride = sizeof(Vertex),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
	};

	std::vector<VkVertexInputAttributeDescription> attributeDescriptions =
	{
		VkVertexInputAttributeDescription
		{
			.location = 0,
			.binding = binding,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(Vertex, position)
		},
		VkVertexInputAttributeDescription
		{
			.location = 1,
			.binding = binding,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(Vertex, color)
		},
		VkVertexInputAttributeDescription
		{
			.location = 2,
			.binding = binding,
			.format = VK_FORMAT_R32G32_SFLOAT,
			.offset = offsetof(Vertex, texCoords)
		}
	};

	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &bindingDescription,
		.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
		.pVertexAttributeDescriptions = attributeDescriptions.data()
	};

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = VK_FALSE
	};

	VkViewport viewport
	{
		.x = 0,
		.y = 0,
		.width = static_cast<float>(swapchainExtent_.width),
		.height = static_cast<float>(swapchainExtent_.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};

	VkRect2D scissor
	{
		.offset = VkOffset2D {.x = 0, .y = 0},
		.extent = swapchainExtent_
	};

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports = &viewport,
		.scissorCount = 1,
		.pScissors = &scissor
	};


	//std::vector<VkDynamicState> dynamicStateEnables
	//{
	//	VK_DYNAMIC_STATE_VIEWPORT,
	//	VK_DYNAMIC_STATE_SCISSOR
	//};
	//
	//VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo
	//{
	//	.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
	//	.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size()),
	//	.pDynamicStates = dynamicStateEnables.data()
	//};


	VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.depthBiasEnable = VK_FALSE,
		.lineWidth = 1.0f
	};


	VkPipelineMultisampleStateCreateInfo multisamplingCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = VK_FALSE
	};


	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS,
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable = VK_FALSE
	};


	// BlendingOp ( NewColor * SrcColorBlendFactor , OldColor * DstColorBlendFactor )
	VkPipelineColorBlendAttachmentState colorBlendAttachmentState
	{
		.blendEnable = VK_TRUE,

		.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.colorBlendOp = VK_BLEND_OP_ADD,

		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_BLEND_OP_ADD,

		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};

	VkPipelineColorBlendStateCreateInfo colorBlendingCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.attachmentCount = 1,
		.pAttachments = &colorBlendAttachmentState
	};

	std::vector<VkDescriptorSetLayout> descriptorSetLayouts {descriptorSetLayout_, samplerDescriptorSetLayout_ };

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size()),
		.pSetLayouts = descriptorSetLayouts.data(),
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &pushConstantRange_
	};

	VkResult result = vkCreatePipelineLayout(mainDevice.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout_);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a pipeline layout.");
	}

	VkGraphicsPipelineCreateInfo graphicsPiplineCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = 2,
		.pStages = shaderStages,
		.pVertexInputState = &vertexInputCreateInfo,
		.pInputAssemblyState = &inputAssemblyCreateInfo,
		.pViewportState = &viewportStateCreateInfo,
		.pRasterizationState = &rasterizerCreateInfo,
		.pMultisampleState = &multisamplingCreateInfo,
		.pDepthStencilState = &depthStencilStateCreateInfo,
		.pColorBlendState = &colorBlendingCreateInfo,
		.pDynamicState = nullptr,
		.layout = pipelineLayout_,
		.renderPass = renderPass_,
		.subpass = 0,
		.basePipelineHandle = VK_NULL_HANDLE,
		.basePipelineIndex = -1
	};

	result = vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &graphicsPiplineCreateInfo, nullptr, &graphicsPipeline_);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a graphics pipeline.");
	}

	vkDestroyShaderModule(mainDevice.logicalDevice, vertexShaderModule, nullptr);
	vkDestroyShaderModule(mainDevice.logicalDevice, fragmentShaderModule, nullptr);


	vertexShaderCode = readBinaryFile("../shaders/second_vert.spv");
	fragmentShaderCode = readBinaryFile("../shaders/second_frag.spv");

	vertexShaderModule = CreateShaderModule(vertexShaderCode);
	fragmentShaderModule = CreateShaderModule(fragmentShaderCode);

	vertexShaderCreateInfo.module = vertexShaderModule;
	fragmentShaderCreateInfo.module = fragmentShaderModule;

	VkPipelineShaderStageCreateInfo secondShaderStages[] = { vertexShaderCreateInfo, fragmentShaderCreateInfo };

	vertexInputCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputCreateInfo.pVertexBindingDescriptions = nullptr;
	vertexInputCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputCreateInfo.pVertexAttributeDescriptions = nullptr;

	depthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;

	VkPipelineLayoutCreateInfo secondPipelineLayoutCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &inputDescriptorSetLayout_,
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr
	};

	result = vkCreatePipelineLayout(mainDevice.logicalDevice, &secondPipelineLayoutCreateInfo, nullptr, &secondPipelineLayout_);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a pipeline layout.");
	}

	graphicsPiplineCreateInfo.pStages = secondShaderStages;
	graphicsPiplineCreateInfo.layout = secondPipelineLayout_;
	graphicsPiplineCreateInfo.subpass = 1;

	result = vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &graphicsPiplineCreateInfo, nullptr, &secondPipeline_);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a graphics pipeline.");
	}

	vkDestroyShaderModule(mainDevice.logicalDevice, vertexShaderModule, nullptr);
	vkDestroyShaderModule(mainDevice.logicalDevice, fragmentShaderModule, nullptr);
}

void VulkanRenderer::CreateColorBufferImages()
{
	if (colorBufferImageFormat_ == VK_FORMAT_UNDEFINED)
	{
		colorBufferImageFormat_ = ChooseSupportedFormat(
			{ VK_FORMAT_R8G8B8A8_UNORM },
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		);
	}

	colorBufferImages_.resize(swapchainImages_.size());
	colorBufferImagesMemory_.resize(swapchainImages_.size());
	colorBufferImageViews_.resize(swapchainImages_.size());

	for (size_t i = 0; i < colorBufferImages_.size(); i++)
	{
		colorBufferImages_[i] = CreateImage(
			swapchainExtent_.width,
			swapchainExtent_.height,
			colorBufferImageFormat_,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&colorBufferImagesMemory_[i]
		);

		colorBufferImageViews_[i] = CreateImageView(colorBufferImages_[i], colorBufferImageFormat_, VK_IMAGE_ASPECT_COLOR_BIT);
	}
}

void VulkanRenderer::CreateDepthBufferImages()
{
	if (depthBufferImageFormat_ == VK_FORMAT_UNDEFINED)
	{
		depthBufferImageFormat_ = ChooseSupportedFormat(
			{ VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);
	}

	depthBufferImages_.resize(swapchainImages_.size());
	depthBufferImagesMemory_.resize(swapchainImages_.size());
	depthBufferImageViews_.resize(swapchainImages_.size());

	for (size_t i = 0; i < depthBufferImages_.size(); i++)
	{
		depthBufferImages_[i] = CreateImage(
			swapchainExtent_.width,
			swapchainExtent_.height,
			depthBufferImageFormat_,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&depthBufferImagesMemory_[i]
		);

		depthBufferImageViews_[i] = CreateImageView(depthBufferImages_[i], depthBufferImageFormat_, VK_IMAGE_ASPECT_DEPTH_BIT);
	}
}

void VulkanRenderer::CreateFramebuffers()
{
	swapchainFramebuffers_.resize(swapchainImages_.size());

	for (size_t i = 0; i < swapchainFramebuffers_.size(); i++)
	{
		std::vector<VkImageView> attachments
		{
			swapchainImages_[i].imageView,
			colorBufferImageViews_[i],
			depthBufferImageViews_[i]
		};

		VkFramebufferCreateInfo framebufferCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = renderPass_,
			.attachmentCount = static_cast<uint32_t>(attachments.size()),
			.pAttachments = attachments.data(),
			.width = swapchainExtent_.width,
			.height = swapchainExtent_.height,
			.layers = 1
		};

		VkResult result = vkCreateFramebuffer(mainDevice.logicalDevice, &framebufferCreateInfo, nullptr, &swapchainFramebuffers_[i]);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create a famebuffer.");
		}
	}
}

void VulkanRenderer::CreateCommandPool()
{
	QueueFamilyIndices queueFamilyIndices = GetQueueFamilies(mainDevice.physicalDevice);

	VkCommandPoolCreateInfo commandPoolCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = static_cast<uint32_t>(queueFamilyIndices.graphicsFamily)
	};

	VkResult result = vkCreateCommandPool(mainDevice.logicalDevice, &commandPoolCreateInfo, nullptr, &graphicsCommandPool_);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a command pool.");
	}
}

void VulkanRenderer::CreateCommandBuffers()
{
	commandBuffers_.resize(swapchainFramebuffers_.size());

	VkCommandBufferAllocateInfo commandBufferAllocateInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = graphicsCommandPool_,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = static_cast<uint32_t>(commandBuffers_.size())
	};

	VkResult result = vkAllocateCommandBuffers(mainDevice.logicalDevice, &commandBufferAllocateInfo, commandBuffers_.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate command buffers.");
	}
}

void VulkanRenderer::CreateSynchronization()
{
	imageAvailableSemaphores_.resize(MAX_FRAME_DRAWS);
	renderFinishedSemaphores_.resize(MAX_FRAME_DRAWS);
	drawFences_.resize(MAX_FRAME_DRAWS);

	VkSemaphoreCreateInfo semaphoreCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
	};

	VkFenceCreateInfo fenceCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT
	};

	for (size_t i = 0; i < MAX_FRAME_DRAWS; i++)
	{
		if (vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &imageAvailableSemaphores_[i]) != VK_SUCCESS ||
			vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &renderFinishedSemaphores_[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create a semaphore.");
		}

		if (vkCreateFence(mainDevice.logicalDevice, &fenceCreateInfo, nullptr, &drawFences_[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create a fence.");
		}
	}
}

void VulkanRenderer::CreateUniformBuffers()
{
	VkDeviceSize vpBuffersSize = sizeof(UboViewProjection);
	//VkDeviceSize modelBuffersSize = modelUniformAlignment_ * MAX_OBJECTS;

	vpUniformBuffer_.resize(swapchainImages_.size());
	vpUniformBufferMemory_.resize(swapchainImages_.size());
	//modelDynamicUniformBuffer_.resize(swapchainImages_.size());
	//modelDynamicUniformBufferMemory_.resize(swapchainImages_.size());

	for (size_t i = 0; i < swapchainImages_.size(); i++)
	{
		createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, vpBuffersSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &vpUniformBuffer_[i], &vpUniformBufferMemory_[i]);

		//createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, modelBuffersSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		//	VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &modelDynamicUniformBuffer_[i], &modelDynamicUniformBufferMemory_[i]);
	}
}

void VulkanRenderer::CreateDescriptorPools()
{
	VkDescriptorPoolSize vpDescriptorPoolSize
	{
		.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = static_cast<uint32_t>(vpUniformBuffer_.size())
	};
	//VkDescriptorPoolSize modelDescriptorPoolSize
	//{
	//	.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
	//	.descriptorCount = static_cast<uint32_t>(modelDynamicUniformBuffer_.size())
	//};
	std::vector<VkDescriptorPoolSize> poolSizes { vpDescriptorPoolSize/*, modelDescriptorPoolSize*/ };

	VkDescriptorPoolCreateInfo vpDescriptorPoolCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = static_cast<uint32_t>(swapchainImages_.size()),
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data()
	};

	VkResult result = vkCreateDescriptorPool(mainDevice.logicalDevice, &vpDescriptorPoolCreateInfo, nullptr, &descriptorPool_);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a descriptor pool.");
	}


	VkDescriptorPoolSize samplerDescriptorPoolSize
	{
		.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = MAX_OBJECTS
	};

	VkDescriptorPoolCreateInfo samplerDescriptorPoolCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = MAX_OBJECTS,
		.poolSizeCount = 1,
		.pPoolSizes = &samplerDescriptorPoolSize
	};

	result = vkCreateDescriptorPool(mainDevice.logicalDevice, &samplerDescriptorPoolCreateInfo, nullptr, &samplerDescriptorPool_);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a descriptor pool.");
	}

	std::vector<VkDescriptorPoolSize> inputDescriptorPoolSizes
	{
		// Color attachment
		VkDescriptorPoolSize
		{
			.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
			.descriptorCount = static_cast<uint32_t>(colorBufferImageViews_.size())
		},
		// Depth attachment
		VkDescriptorPoolSize
		{
			.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
			.descriptorCount = static_cast<uint32_t>(depthBufferImageViews_.size())
		}
	};

	VkDescriptorPoolCreateInfo inputDescriptorPoolCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = static_cast<uint32_t>(colorBufferImageViews_.size()),
		.poolSizeCount = static_cast<uint32_t>(inputDescriptorPoolSizes.size()),
		.pPoolSizes = inputDescriptorPoolSizes.data()
	};

	result = vkCreateDescriptorPool(mainDevice.logicalDevice, &inputDescriptorPoolCreateInfo, nullptr, &inputDescriptorPool_);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a descriptor pool.");
	}
}

void VulkanRenderer::CreateDescriptorSets()
{
	std::vector<VkDescriptorSetLayout> descriptorSetLayouts(vpUniformBuffer_.size(), descriptorSetLayout_);

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = descriptorPool_,
		.descriptorSetCount = static_cast<uint32_t>(swapchainImages_.size()),
		.pSetLayouts = descriptorSetLayouts.data()
	};

	descriptorSets_.resize(swapchainImages_.size());

	VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &descriptorSetAllocateInfo, descriptorSets_.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate descriptor sets.");
	}

	for (size_t i = 0; i < descriptorSets_.size(); i++)
	{
		VkDescriptorBufferInfo vpBufferInfo
		{
			.buffer = vpUniformBuffer_[i],
			.offset = 0,
			.range = sizeof(UboViewProjection)
		};
		//VkDescriptorBufferInfo modelBufferInfo
		//{
		//	.buffer = modelDynamicUniformBuffer_[i],
		//	.offset = 0,
		//	.range = modelUniformAlignment_
		//};

		VkWriteDescriptorSet vpSetWrite
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = descriptorSets_[i],
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &vpBufferInfo
		};
		//VkWriteDescriptorSet modelSetWrite
		//{
		//	.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		//	.dstSet = descriptorSets_[i],
		//	.dstBinding = 1,
		//	.dstArrayElement = 0,
		//	.descriptorCount = 1,
		//	.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
		//	.pBufferInfo = &modelBufferInfo
		//};
		std::vector<VkWriteDescriptorSet> writeDescriptorSets { vpSetWrite/*, modelSetWrite*/ };

		vkUpdateDescriptorSets(mainDevice.logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}
}

void VulkanRenderer::CreateInputDescriptorSets()
{
	inputDescriptorSets_.resize(swapchainImages_.size());

	std::vector<VkDescriptorSetLayout> descriptorSetLayouts(swapchainImages_.size(), inputDescriptorSetLayout_);

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = inputDescriptorPool_,
		.descriptorSetCount = static_cast<uint32_t>(swapchainImages_.size()),
		.pSetLayouts = descriptorSetLayouts.data()
	};

	VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &descriptorSetAllocateInfo, inputDescriptorSets_.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate input attachments descriptor sets.");
	}

	for (size_t i = 0; i < inputDescriptorSets_.size(); i++)
	{
		VkDescriptorImageInfo colorAttachmentDescriptorInfo
		{
			.sampler = VK_NULL_HANDLE,
			.imageView = colorBufferImageViews_[i],
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		VkWriteDescriptorSet colorWriteDescriptorSet
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = inputDescriptorSets_[i],
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
			.pImageInfo = &colorAttachmentDescriptorInfo
		};

		VkDescriptorImageInfo depthAttachmentDescriptorInfo
		{
			.sampler = VK_NULL_HANDLE,
			.imageView = depthBufferImageViews_[i],
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		VkWriteDescriptorSet depthWriteDescriptorSet
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = inputDescriptorSets_[i],
			.dstBinding = 1,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
			.pImageInfo = &depthAttachmentDescriptorInfo
		};

		std::vector<VkWriteDescriptorSet> writeDescriptorSets { colorWriteDescriptorSet, depthWriteDescriptorSet };

		vkUpdateDescriptorSets(mainDevice.logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}
}

void VulkanRenderer::CreateTextureSampler()
{
	VkSamplerCreateInfo samplerCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.mipLodBias = 0.0f,
		.anisotropyEnable = VK_TRUE,
		.maxAnisotropy = 16,
		.minLod = 0.0f,
		.maxLod = 0.0f,
		.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
		.unnormalizedCoordinates = VK_FALSE,
	};

	VkResult result = vkCreateSampler(mainDevice.logicalDevice, &samplerCreateInfo, nullptr, &textureSampler_);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a texture sampler.");
	}
}


bool VulkanRenderer::CheckInstanceExtensionSupport(std::vector<const char*> extensions)
{
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

	for (const char* extension : extensions)
	{
		auto foundExtension = std::find_if(
			availableExtensions.begin(), 
			availableExtensions.end(),
			[extension](const VkExtensionProperties props) { return strcmp(extension, props.extensionName); }
		);
		if (foundExtension == availableExtensions.end())
		{
			return false;
		}
	}

	return true;
}

bool VulkanRenderer::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	if (extensionCount == 0)
	{
		return false;
	}

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	for (const char* extension : deviceExtensions)
	{
		auto foundExtension = std::find_if(
			availableExtensions.begin(),
			availableExtensions.end(),
			[extension](const VkExtensionProperties props) { return strcmp(extension, props.extensionName); }
		);
		if (foundExtension == availableExtensions.end())
		{
			return false;
		}
	}

	return true;
}

bool VulkanRenderer::CheckInstanceDeviceSupport(VkPhysicalDevice device)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	if (extensionCount == 0)
	{
		return false;
	}

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	return false;
}

bool VulkanRenderer::CheckValidationLayerSupport(std::vector<const char*> layers)
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layer : layers) {
		auto foundExtension = std::find_if(
			availableLayers.begin(),
			availableLayers.end(),
			[layer](const VkLayerProperties props) { return strcmp(layer, props.layerName); }
		);
		if (foundExtension == availableLayers.end())
		{
			return false;
		}
	}

	return true;
}

bool VulkanRenderer::CheckPhysicalDeviceSuitable(VkPhysicalDevice device)
{
	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties(device, &properties);

	// Check

	VkPhysicalDeviceFeatures features;
	vkGetPhysicalDeviceFeatures(device, &features);

	// Check

	QueueFamilyIndices indices = GetQueueFamilies(device);
	if (GetQueueFamilies(device).IsValid() == false)
	{
		return false;
	}

	if (checkDeviceExtensionSupport(device) == false)
	{
		return false;
	}

	if (GetSwapchainDetails(device).IsValid() == false)
	{
		return false;
	}

	if (features.samplerAnisotropy == VK_FALSE)
	{
		return false;
	}

	return true;
}


void VulkanRenderer::RecordCommands(uint32_t imageIndex)
{
	VkCommandBufferBeginInfo commandBufferBeginInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
	};

	std::vector<VkClearValue> clearValues
	{
		//VkClearValue
		//{
		//	.color = VkClearColorValue { 0.6f, 0.65f, 0.4f, 0.0f }
		//},
		VkClearValue
		{
			.color = VkClearColorValue { 0.0f, 0.0f, 0.0f, 1.0f }
		},
		VkClearValue
		{
			.color = VkClearColorValue { 0.6f, 0.65f, 0.4f, 0.0f }
		},
		VkClearValue
		{
			.depthStencil = VkClearDepthStencilValue
			{
				.depth = 1.0f
			}
		}
	};

	VkRenderPassBeginInfo renderPassBeginInfo
	{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = renderPass_,
		.renderArea = VkRect2D
		{
			.offset = VkOffset2D { 0, 0 },
			.extent = swapchainExtent_
		},
		.clearValueCount = static_cast<uint32_t>(clearValues.size()),
		.pClearValues = clearValues.data()
	};

	renderPassBeginInfo.framebuffer = swapchainFramebuffers_[imageIndex];

	const VkCommandBuffer& commandBuffer = commandBuffers_[imageIndex];
	VkResult result = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to start recording a command buffer.");
	}

	vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	{
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline_);

		for (size_t j = 0; j < models_.size(); j++)
		{
			const MeshModel& model = models_[j];

			vkCmdPushConstants(commandBuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Model), &model.GetModel());

			for (size_t k = 0; k < model.GetMeshCount(); k++)
			{
				const Mesh& mesh = model.GetMesh(k);

				VkBuffer vertexBuffers[] = { mesh.GetVertexBuffer() };
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

				vkCmdBindIndexBuffer(commandBuffer, mesh.GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

				std::vector<VkDescriptorSet> descriptorSetGroup{ descriptorSets_[imageIndex], samplerDescriptorSets_[mesh.GetTextureId()] };
				//uint32_t dynamicOffset = static_cast<uint32_t>(modelUniformAlignment_) * j;
				//vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &descriptorSets_[imageIndex], 1, &dynamicOffset);
				vkCmdBindDescriptorSets(
					commandBuffer,
					VK_PIPELINE_BIND_POINT_GRAPHICS,
					pipelineLayout_,
					0,
					static_cast<uint32_t>(descriptorSetGroup.size()),
					descriptorSetGroup.data(),
					0,
					nullptr
				);

				vkCmdDrawIndexed(commandBuffer, mesh.GetIndexCount(), 1, 0, 0, 0);
			}
		}

		vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, secondPipeline_);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, secondPipelineLayout_, 0, 1, &inputDescriptorSets_[imageIndex], 0, nullptr);
		vkCmdDraw(commandBuffer, 3, 1, 0, 0);
	}
	vkCmdEndRenderPass(commandBuffer);

	result = vkEndCommandBuffer(commandBuffer);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to stop recording a command buffer.");
	}
}

void VulkanRenderer::UpdateUniformBuffers(uint32_t imageIndex)
{
	void* data;

	vkMapMemory(mainDevice.logicalDevice, vpUniformBufferMemory_[imageIndex], 0, sizeof(UboViewProjection), 0, &data);
	memcpy(data, &uboViewProjection_, sizeof(UboViewProjection));
	vkUnmapMemory(mainDevice.logicalDevice, vpUniformBufferMemory_[imageIndex]);

	/*
	for (size_t i = 0; i < meshes_.size(); i++)
	{
		Model* currentModel = (Model*)((uint64_t)modelTransferSpace_ + (i * modelUniformAlignment_));
		*currentModel = meshes_[i]->GetModel();
	}
	vkMapMemory(mainDevice.logicalDevice, modelDynamicUniformBufferMemory_[imageIndex], 0, modelUniformAlignment_ * meshes_.size(), 0, &data);
	memcpy(data, modelTransferSpace_, modelUniformAlignment_ * meshes_.size());
	vkUnmapMemory(mainDevice.logicalDevice, modelDynamicUniformBufferMemory_[imageIndex]);
	*/
}

void VulkanRenderer::CreateAssets()
{
	models_.push_back(MeshModel::LoadModel("scooter", "scene.gltf", this));
}


QueueFamilyIndices VulkanRenderer::GetQueueFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyProperties.data());

	for (int i = 0; i < queueFamilyProperties.size(); i++)
	{
		const VkQueueFamilyProperties queueFamily = queueFamilyProperties[i];

		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			indices.graphicsFamily = i;
		}

		VkBool32 presentationSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, static_cast<uint32_t>(i), surface_, &presentationSupport);
		if (queueFamily.queueCount > 0 && presentationSupport)
		{
			indices.presentationFamily = i;
		}

		if (indices.IsValid())
		{
			break;
		}
	}

	return indices;
}

SwapchainDetails VulkanRenderer::GetSwapchainDetails(VkPhysicalDevice device)
{
	SwapchainDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &details.surfaceCapabilities);

	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);
	if (formatCount > 0)
	{
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, details.formats.data());
	}

	uint32_t presentationModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentationModeCount, nullptr);
	if (presentationModeCount > 0)
	{
		details.presentationModes.resize(presentationModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentationModeCount, details.presentationModes.data());
	}

	return details;
}


VkSurfaceFormatKHR VulkanRenderer::ChooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
	if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
	{
		// All formats are supported
		return VkSurfaceFormatKHR
		{
			.format = VK_FORMAT_R8G8B8A8_UNORM,
			.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
		};
	}

	for (const VkSurfaceFormatKHR& format : formats)
	{
		if ((format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_B8G8R8A8_UNORM) && 
			format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return format;
		}
	}

	return formats[0];
}

VkPresentModeKHR VulkanRenderer::ChooseBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes)
{
	for (const VkPresentModeKHR& presentationMode : presentationModes)
	{
		if (presentationMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return presentationMode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR; // Is always available
}

VkExtent2D VulkanRenderer::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities)
{
	if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return surfaceCapabilities.currentExtent;
	}

	int width, height;
	glfwGetFramebufferSize(window_, &width, &height);

	VkExtent2D newExtent =
	{
		.width = static_cast<uint32_t>(width),
		.height = static_cast<uint32_t>(height)
	};

	newExtent.width = std::max(surfaceCapabilities.minImageExtent.width, std::min(surfaceCapabilities.maxImageExtent.width, newExtent.width));
	newExtent.height = std::max(surfaceCapabilities.minImageExtent.height, std::min(surfaceCapabilities.maxImageExtent.height, newExtent.height));

	return newExtent;
}

VkFormat VulkanRenderer::ChooseSupportedFormat(const std::vector<VkFormat>& formats, const VkImageTiling tiling, const VkFormatFeatureFlags featureFlags)
{
	for (const VkFormat& format : formats)
	{
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(mainDevice.physicalDevice, format, &formatProperties);

		if (tiling == VK_IMAGE_TILING_LINEAR && (formatProperties.linearTilingFeatures & featureFlags) == featureFlags)
		{
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (formatProperties.optimalTilingFeatures & featureFlags) == featureFlags)
		{
			return format;
		}
	}

	throw std::runtime_error("Failed to find matching format.");
}


VkImage VulkanRenderer::CreateImage(const uint32_t width, const uint32_t height, const VkFormat format, const VkImageTiling tiling, const VkImageUsageFlags useFlags, const VkMemoryPropertyFlags propFlags, VkDeviceMemory* imageMemory)
{
	VkImageCreateInfo imageCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = format,
		.extent = VkExtent3D 
		{
			.width = width,
			.height = height,
			.depth = 1
		},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = tiling,
		.usage = useFlags,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};

	VkImage image;
	VkResult result = vkCreateImage(mainDevice.logicalDevice, &imageCreateInfo, nullptr, &image);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create an image.");
	}

	VkMemoryRequirements imageMemoryRequirements;
	vkGetImageMemoryRequirements(mainDevice.logicalDevice, image, &imageMemoryRequirements);

	VkMemoryAllocateInfo imageMemoryAllocateInfo
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = imageMemoryRequirements.size,
		.memoryTypeIndex = findMemoryTypeIndex(mainDevice.physicalDevice, imageMemoryRequirements.memoryTypeBits, propFlags)
	};

	result = vkAllocateMemory(mainDevice.logicalDevice, &imageMemoryAllocateInfo, nullptr, imageMemory);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate memory for an image.");
	}

	vkBindImageMemory(mainDevice.logicalDevice, image, *imageMemory, 0);

	return image;
}

VkImageView VulkanRenderer::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
	VkImageViewCreateInfo imageViewCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,
		.components = VkComponentMapping
		{
			.r = VK_COMPONENT_SWIZZLE_IDENTITY,	
			.g = VK_COMPONENT_SWIZZLE_IDENTITY,	
			.b = VK_COMPONENT_SWIZZLE_IDENTITY,	
			.a = VK_COMPONENT_SWIZZLE_IDENTITY	
		},
		.subresourceRange = VkImageSubresourceRange
		{
			.aspectMask = aspectFlags,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		}
	};
	
	VkImageView imageView;
	VkResult result = vkCreateImageView(mainDevice.logicalDevice, &imageViewCreateInfo, nullptr, &imageView);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create an image view.");
	}

	return imageView;
}

VkShaderModule VulkanRenderer::CreateShaderModule(const std::vector<char>& code)
{
	VkShaderModuleCreateInfo shaderModuleCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = code.size(),
		.pCode = reinterpret_cast<const uint32_t*>(code.data())
	};

	VkShaderModule shaderModule;
	VkResult result = vkCreateShaderModule(mainDevice.logicalDevice, &shaderModuleCreateInfo, nullptr, &shaderModule);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a shader module.");
	}

	return shaderModule;
}


void VulkanRenderer::AllocateDynamicBufferTransferSpace()
{
	//modelUniformAlignment_ = (sizeof(Model) + (minUniformBufferOffset_ - 1)) & ~(minUniformBufferOffset_ - 1);
	//
	//modelTransferSpace_ = (Model*)_aligned_malloc(modelUniformAlignment_ * MAX_OBJECTS, modelUniformAlignment_);
}

int VulkanRenderer::CreateTextureImage(std::string fileName, bool textureFolderUsed)
{
	int width, height;
	VkDeviceSize imageSize;

	stbi_uc* imageData;
	if (textureFolderUsed)
	{
		imageData = loadTexture(fileName, &width, &height, &imageSize);
	}
	else
	{
		imageData = loadImage(fileName, &width, &height, &imageSize);
	}

	VkBuffer imageStagingBuffer;
	VkDeviceMemory imageStagingBufferMemory;
	createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &imageStagingBuffer, &imageStagingBufferMemory);

	void* data;
	vkMapMemory(mainDevice.logicalDevice, imageStagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, imageData, static_cast<size_t>(imageSize));
	vkUnmapMemory(mainDevice.logicalDevice, imageStagingBufferMemory);

	stbi_image_free(imageData);

	VkImage textureImage;
	VkDeviceMemory textureImageMemory;
	textureImage = CreateImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &textureImageMemory);

	transitionImageLayout(mainDevice.logicalDevice, graphicsQueue_, graphicsCommandPool_, textureImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	copyImageBuffer(mainDevice.logicalDevice, graphicsQueue_, graphicsCommandPool_, imageStagingBuffer, textureImage, width, height);

	transitionImageLayout(mainDevice.logicalDevice, graphicsQueue_, graphicsCommandPool_, textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	textureImages_.push_back(textureImage);
	textureImagesMemory_.push_back(textureImageMemory);

	vkDestroyBuffer(mainDevice.logicalDevice, imageStagingBuffer, nullptr);
	vkFreeMemory(mainDevice.logicalDevice, imageStagingBufferMemory, nullptr);

	return textureImages_.size() - 1;
}

int VulkanRenderer::CreateTextureDescriptor(VkImageView textureImage)
{
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = samplerDescriptorPool_,
		.descriptorSetCount = 1,
		.pSetLayouts = &samplerDescriptorSetLayout_
	};

	VkDescriptorSet descriptorSet;
	VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &descriptorSetAllocateInfo, &descriptorSet);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate texture descriptor set.");
	}

	VkDescriptorImageInfo descriptorImageInfo
	{
		.sampler = textureSampler_,
		.imageView = textureImage,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};

	VkWriteDescriptorSet descriptorWrite
	{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = descriptorSet,
		.dstArrayElement = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.pImageInfo = &descriptorImageInfo
	};

	vkUpdateDescriptorSets(mainDevice.logicalDevice, 1, &descriptorWrite, 0, nullptr);

	samplerDescriptorSets_.push_back(descriptorSet);

	return samplerDescriptorSets_.size() - 1;
}

int VulkanRenderer::CreateTexture(std::string fileName, bool textureFolderUsed)
{
	int textureImageIndex = CreateTextureImage(fileName, textureFolderUsed);

	VkImageView imageView = CreateImageView(textureImages_[textureImageIndex], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
	textureImageViews_.push_back(imageView);

	int descriptorIndex = CreateTextureDescriptor(imageView);

	return descriptorIndex;
}
