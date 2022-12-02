#include "VulcanRenderer.h"

#include <algorithm>
#include <set>


int VulcanRenderer::Init(GLFWwindow* window)
{
	window_ = window;
	try
	{
		CreateVkInstance();
		CreateSurface(window);
		GetPhysicalDevice();
		CreateLogicalDevice();
		CreateSwapchain();
		CreateRenderPass();
		CreateDescriptorSetLayout();
		CreateCraphicsPipeline();
		CreateFramebuffers();
		CreateCommandPool();

		CreateMeshes();
		mvp.projection = glm::perspective(glm::radians(45.0f), (float)swapchainExtent_.width / swapchainExtent_.height, 0.1f, 1000.0f);
		mvp.projection[1][1] *= -1.0f;
		mvp.view = glm::lookAt(glm::vec3(3.0f, 1.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		mvp.model = glm::mat4(1.0f);

		CreateCommandBuffers();
		CreateUniformBuffers();
		CreateDescriptorPool();
		CreateDescriptorSets();
		RecordCommands();
		CreateSynchronization();
	}
	catch (const std::runtime_error &e)
	{
		printf("Error: %s\n", e.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

void VulcanRenderer::Deinit()
{
	vkDeviceWaitIdle(mainDevice.logicalDevice);

	for (auto& mesh : meshes_)
	{
		mesh->DestroyBuffers();
	}

	vkDestroyDescriptorPool(mainDevice.logicalDevice, descriptorPool_, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, descriptorSetLayout_, nullptr);
	for (size_t i = 0; i < uniformBuffer_.size(); i++)
	{
		vkDestroyBuffer(mainDevice.logicalDevice, uniformBuffer_[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, uniformBufferMemory_[i], nullptr);
	}

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
	vkDestroyCommandPool(mainDevice.logicalDevice, graphicsCommandPool_, nullptr);

	vkDestroyPipeline(mainDevice.logicalDevice, graphicsPipeline_, nullptr);
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


void VulcanRenderer::Draw()
{
	vkWaitForFences(mainDevice.logicalDevice, 1, &drawFences_[currentFrame_], VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(mainDevice.logicalDevice, 1, &drawFences_[currentFrame_]);

	uint32_t imageIndex;
	vkAcquireNextImageKHR(mainDevice.logicalDevice, swapchain_, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphores_[currentFrame_], VK_NULL_HANDLE, &imageIndex);

	UpdateUniformBuffer(imageIndex);

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


void VulcanRenderer::CreateVkInstance()
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

void VulcanRenderer::GetPhysicalDevice()
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
}

void VulcanRenderer::CreateLogicalDevice()
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

	VkPhysicalDeviceFeatures deviceFeatures = {};

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

void VulcanRenderer::CreateSurface(GLFWwindow* window)
{
	VkResult result = glfwCreateWindowSurface(vkInsatance_, window, nullptr, &surface_);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a surface.");
	}
}

void VulcanRenderer::CreateSwapchain()
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

void VulcanRenderer::CreateRenderPass()
{
	// Color attachment
	VkAttachmentDescription colorAttachment
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

	VkAttachmentReference colorAttachmentReference
	{
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};

	VkSubpassDescription subpass
	{
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachmentReference
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
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		},
		// Conversion form VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		VkSubpassDependency
		{
			.srcSubpass = 0,
			.dstSubpass = VK_SUBPASS_EXTERNAL,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		}
	};

	VkRenderPassCreateInfo renderPassCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &colorAttachment,
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = static_cast<uint32_t>(subpassDependencies.size()),
		.pDependencies = subpassDependencies.data()
	};

	VkResult result = vkCreateRenderPass(mainDevice.logicalDevice, &renderPassCreateInfo, nullptr, &renderPass_);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a render pass.");
	}
}

void VulcanRenderer::CreateDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding mvpLayoutBinding
	{
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.pImmutableSamplers = nullptr
	};

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 1,
		.pBindings = &mvpLayoutBinding
	};

	VkResult result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout_);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a descriptor set.");
	}
}

void VulcanRenderer::CreateCraphicsPipeline()
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

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &descriptorSetLayout_,
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr
	};

	VkResult result = vkCreatePipelineLayout(mainDevice.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout_);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create runtime layout.");
	}

	VkGraphicsPipelineCreateInfo graphicsPiplineCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
	};

	VkGraphicsPipelineCreateInfo pipelineCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = 2,
		.pStages = shaderStages,
		.pVertexInputState = &vertexInputCreateInfo,
		.pInputAssemblyState = &inputAssemblyCreateInfo,
		.pViewportState = &viewportStateCreateInfo,
		.pRasterizationState = &rasterizerCreateInfo,
		.pMultisampleState = &multisamplingCreateInfo,
		.pDepthStencilState = nullptr,
		.pColorBlendState = &colorBlendingCreateInfo,
		.pDynamicState = nullptr,
		.layout = pipelineLayout_,
		.renderPass = renderPass_,
		.subpass = 0,
		.basePipelineHandle = VK_NULL_HANDLE,
		.basePipelineIndex = -1
	};

	result = vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline_);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a graphics pipeline.");
	}

	vkDestroyShaderModule(mainDevice.logicalDevice, vertexShaderModule, nullptr);
	vkDestroyShaderModule(mainDevice.logicalDevice, fragmentShaderModule, nullptr);
}

void VulcanRenderer::CreateFramebuffers()
{
	swapchainFramebuffers_.resize(swapchainImages_.size());

	for (size_t i = 0; i < swapchainFramebuffers_.size(); i++)
	{
		std::vector<VkImageView> attachments
		{
			swapchainImages_[i].imageView
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

void VulcanRenderer::CreateCommandPool()
{
	QueueFamilyIndices queueFamilyIndices = GetQueueFamilies(mainDevice.physicalDevice);

	VkCommandPoolCreateInfo commandPoolCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.queueFamilyIndex = static_cast<uint32_t>(queueFamilyIndices.graphicsFamily)
	};

	VkResult result = vkCreateCommandPool(mainDevice.logicalDevice, &commandPoolCreateInfo, nullptr, &graphicsCommandPool_);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a command pool.");
	}
}

void VulcanRenderer::CreateCommandBuffers()
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

void VulcanRenderer::CreateSynchronization()
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

void VulcanRenderer::CreateUniformBuffers()
{
	VkDeviceSize buffersSize = sizeof(MVP);

	uniformBuffer_.resize(swapchainImages_.size());
	uniformBufferMemory_.resize(swapchainImages_.size());
	for (size_t i = 0; i < uniformBuffer_.size(); i++)
	{
		createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, buffersSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffer_[i], &uniformBufferMemory_[i]);
	}
}

void VulcanRenderer::CreateDescriptorPool()
{
	VkDescriptorPoolSize descriptorPoolSize
	{
		.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = static_cast<uint32_t>(uniformBuffer_.size())
	};

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = static_cast<uint32_t>(uniformBuffer_.size()),
		.poolSizeCount = 1,
		.pPoolSizes = &descriptorPoolSize
	};

	VkResult result = vkCreateDescriptorPool(mainDevice.logicalDevice, &descriptorPoolCreateInfo, nullptr, &descriptorPool_);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a descriptor pool.");
	}
}

void VulcanRenderer::CreateDescriptorSets()
{
	std::vector<VkDescriptorSetLayout> descriptorSetLayouts(uniformBuffer_.size(), descriptorSetLayout_);

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = descriptorPool_,
		.descriptorSetCount = static_cast<uint32_t>(uniformBuffer_.size()),
		.pSetLayouts = descriptorSetLayouts.data()
	};

	descriptorSets_.resize(uniformBuffer_.size());

	VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &descriptorSetAllocateInfo, descriptorSets_.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate descriptor sets.");
	}

	for (size_t i = 0; i < descriptorSets_.size(); i++)
	{
		VkDescriptorBufferInfo mvpBufferInfo
		{
			.buffer = uniformBuffer_[i],
			.offset = 0,
			.range = sizeof(MVP)
		};

		VkWriteDescriptorSet mvpSetWrite
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = descriptorSets_[i],
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &mvpBufferInfo
		};

		vkUpdateDescriptorSets(mainDevice.logicalDevice, 1, &mvpSetWrite, 0, nullptr);
	}
}


bool VulcanRenderer::CheckInstanceExtensionSupport(std::vector<const char*> extensions)
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

bool VulcanRenderer::checkDeviceExtensionSupport(VkPhysicalDevice device)
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

bool VulcanRenderer::CheckInstanceDeviceSupport(VkPhysicalDevice device)
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

bool VulcanRenderer::CheckValidationLayerSupport(std::vector<const char*> layers)
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

bool VulcanRenderer::CheckPhysicalDeviceSuitable(VkPhysicalDevice device)
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

	return true;
}


void VulcanRenderer::RecordCommands()
{
	VkCommandBufferBeginInfo commandBufferBeginInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
	};

	VkClearValue clearValues[]
	{
		VkClearValue
		{
			.color = VkClearColorValue { 0.6f, 0.65f, 0.4f, 0.0f }
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
		.clearValueCount = 1,
		.pClearValues = clearValues
	};

	for (size_t i = 0; i < commandBuffers_.size(); i++)
	{
		renderPassBeginInfo.framebuffer = swapchainFramebuffers_[i];

		VkResult result = vkBeginCommandBuffer(commandBuffers_[i], &commandBufferBeginInfo);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to start recording a command buffer.");
		}

		vkCmdBeginRenderPass(commandBuffers_[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		{
			vkCmdBindPipeline(commandBuffers_[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline_);

			for (const std::unique_ptr<Mesh>& mesh : meshes_)
			{
				VkBuffer vertexBuffers[] = { mesh->GetVertexBuffer() };
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(commandBuffers_[i], 0, 1, vertexBuffers, offsets);

				vkCmdBindIndexBuffer(commandBuffers_[i], mesh->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

				vkCmdBindDescriptorSets(commandBuffers_[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &descriptorSets_[i], 0, nullptr);

				vkCmdDrawIndexed(commandBuffers_[i], mesh->GetIndexCount(), 1, 0, 0, 0);
			}
		}
		vkCmdEndRenderPass(commandBuffers_[i]);

		result = vkEndCommandBuffer(commandBuffers_[i]);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to stop recording a command buffer.");
		}
	}
}

void VulcanRenderer::UpdateUniformBuffer(uint32_t imageIndex)
{
	void* data;
	vkMapMemory(mainDevice.logicalDevice, uniformBufferMemory_[imageIndex], 0, sizeof(MVP), 0, &data);
	memcpy(data, &mvp, sizeof(MVP));
	vkUnmapMemory(mainDevice.logicalDevice, uniformBufferMemory_[imageIndex]);
}

void VulcanRenderer::CreateMeshes()
{
	std::vector<Vertex> meshVertices1
	{
		Vertex { {-0.4f, -0.8f, 0.0f}, {1.0f, 0.0f, 0.0f} }, // top right
		Vertex { {-0.4f, -0.4f, 0.0f}, {0.0f, 1.0f, 0.0f} }, // bottom right
		Vertex { {-0.8f, -0.4f, 0.0f}, {0.0f, 0.0f, 1.0f} }, // bottom left
		Vertex { {-0.8f, -0.8f, 0.0f}, {1.0f, 0.0f, 1.0f} }  // top left
	};

	std::vector<Vertex> meshVertices2
	{
		Vertex { {0.8f, -0.8f, 0.0f}, {1.0f, 0.0f, 0.0f} }, // top right
		Vertex { {0.8f, -0.4f, 0.0f}, {0.0f, 1.0f, 0.0f} }, // bottom right
		Vertex { {0.4f, -0.4f, 0.0f}, {0.0f, 0.0f, 1.0f} }, // bottom left
		Vertex { {0.4f, -0.8f, 0.0f}, {1.0f, 0.0f, 1.0f} }  // top left
	};

	std::vector<Vertex> meshVertices3
	{
		Vertex { {-0.4f, 0.4f, 0.0f}, {1.0f, 0.0f, 0.0f} }, // top right
		Vertex { {-0.4f, 0.8f, 0.0f}, {0.0f, 1.0f, 0.0f} }, // bottom right
		Vertex { {-0.8f, 0.8f, 0.0f}, {0.0f, 0.0f, 1.0f} }, // bottom left
		Vertex { {-0.8f, 0.4f, 0.0f}, {1.0f, 0.0f, 1.0f} }  // top left
	};

	std::vector<Vertex> meshVertices4
	{
		Vertex { {0.8f, 0.4f, 0.0f}, {1.0f, 0.0f, 0.0f} }, // top right
		Vertex { {0.8f, 0.8f, 0.0f}, {0.0f, 1.0f, 0.0f} }, // bottom right
		Vertex { {0.4f, 0.8f, 0.0f}, {0.0f, 0.0f, 1.0f} }, // bottom left
		Vertex { {0.4f, 0.4f, 0.0f}, {1.0f, 0.0f, 1.0f} }  // top left
	};

	std::vector<uint32_t> meshIndices
	{
		0, 1, 2,
		2, 3, 0
	};

	// (Usually graphics queue = transfer queue)

	meshes_.push_back(
		std::make_unique<Mesh>(mainDevice.physicalDevice, mainDevice.logicalDevice, graphicsQueue_, 
			graphicsCommandPool_, meshVertices1, meshIndices)
	);
	meshes_.push_back(
		std::make_unique<Mesh>(mainDevice.physicalDevice, mainDevice.logicalDevice, graphicsQueue_,
			graphicsCommandPool_, meshVertices2, meshIndices)
	);
	meshes_.push_back(
		std::make_unique<Mesh>(mainDevice.physicalDevice, mainDevice.logicalDevice, graphicsQueue_,
			graphicsCommandPool_, meshVertices3, meshIndices)
	);
	meshes_.push_back(
		std::make_unique<Mesh>(mainDevice.physicalDevice, mainDevice.logicalDevice, graphicsQueue_,
			graphicsCommandPool_, meshVertices4, meshIndices)
	);
}


QueueFamilyIndices VulcanRenderer::GetQueueFamilies(VkPhysicalDevice device)
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

SwapchainDetails VulcanRenderer::GetSwapchainDetails(VkPhysicalDevice device)
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


VkSurfaceFormatKHR VulcanRenderer::ChooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
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

VkPresentModeKHR VulcanRenderer::ChooseBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes)
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

VkExtent2D VulcanRenderer::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities)
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


VkImageView VulcanRenderer::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
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

VkShaderModule VulcanRenderer::CreateShaderModule(const std::vector<char>& code)
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
