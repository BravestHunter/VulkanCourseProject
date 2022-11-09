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
	}
	catch (const std::runtime_error &e)
	{
		printf("Error: %s\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

void VulcanRenderer::Deinit()
{
	for (SwapchainImage image : swapchainImages_)
	{
		vkDestroyImageView(mainDevice.logicalDevice, image.imageView, nullptr);
	}

	vkDestroySwapchainKHR(mainDevice.logicalDevice, swapchain_, nullptr);
	vkDestroySurfaceKHR(vkInsatance_, surface_, nullptr);
	vkDestroyDevice(mainDevice.logicalDevice, nullptr);
	vkDestroyInstance(vkInsatance_, nullptr);
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
		"VK_LAYER_KHRONOS_validation",
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
	vkCreateImageView(mainDevice.logicalDevice, &imageViewCreateInfo, nullptr, &imageView);

	return imageView;
}
