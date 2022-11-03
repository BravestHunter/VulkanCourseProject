#pragma once

#include <stdexcept>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Utilities.h"


class VulcanRenderer
{
public:
	int Init();
	void Deinit();

private:
	VkInstance vkInsatance_;

	struct {
		VkPhysicalDevice physicalDevice;
		VkDevice logicalDevice;
	} mainDevice;

	VkQueue graphicsQueue_;

	void CreateVkInstance();
	void GetPhysicalDevice();
	void CreateLogicalDevice();
	bool CheckInstanceExtensionSupport(std::vector<const char*> extensions);
	bool CheckValidationLayerSupport(std::vector<const char*> layers);
	bool CheckPhysicalDeviceSuitable(VkPhysicalDevice device);
	QueueFamilyIndices GetQueueFamilies(VkPhysicalDevice device);
};
