#include "VulcanRenderer.h"

#include <algorithm>


int VulcanRenderer::Init()
{
	try
	{
		CreateVkInstance();
		GetPhysicalDevice();
		CreateLogicalDevice();
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
	vkDestroyDevice(mainDevice.logicalDevice, nullptr);
	vkDestroyInstance(vkInsatance_, nullptr);
}


void VulcanRenderer::CreateVkInstance()
{
	VkApplicationInfo appInfo =
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
	const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};
	if (CheckValidationLayerSupport(validationLayers) == false) {
		throw std::runtime_error("Vk instance does not support required validation layers.");
	}
#endif // !_DEBUG

	VkInstanceCreateInfo createInfo = 
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
	float priority = 2.0f;

	VkDeviceQueueCreateInfo queueCreateInfo =
	{
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = static_cast<uint32_t>(indices.graphicsFamily),
		.queueCount = 1,
		.pQueuePriorities = &priority
	};

	VkPhysicalDeviceFeatures deviceFeatures = {};

	VkDeviceCreateInfo deviceCreateInfo =
	{
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &queueCreateInfo,
		.enabledExtensionCount = 0,
		.ppEnabledExtensionNames = nullptr,
		.pEnabledFeatures = &deviceFeatures
	};

	VkResult result = vkCreateDevice(mainDevice.physicalDevice, &deviceCreateInfo, nullptr, &mainDevice.logicalDevice);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a logical device.");
	}

	vkGetDeviceQueue(mainDevice.logicalDevice, static_cast<uint32_t>(indices.graphicsFamily), 0, &graphicsQueue_);
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

	return indices.IsValid();
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

		if (indices.IsValid())
		{
			break;
		}
	}

	return indices;
}
