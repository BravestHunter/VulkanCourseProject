#pragma once

#include <vector>


const std::vector<const char*> deviceExtensions = 
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};


struct QueueFamilyIndices
{
	int graphicsFamily = -1;
	int presentationFamily = -1;

	bool IsValid()
	{
		return 
			graphicsFamily >=0 &&
			presentationFamily >=0;
	}
};

struct SwapchainDetails
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentationModes;

	bool IsValid()
	{
		return
			!formats.empty() &&
			!presentationModes.empty();
	}
};

struct SwapchainImage
{
	VkImage image;
	VkImageView imageView;
};
