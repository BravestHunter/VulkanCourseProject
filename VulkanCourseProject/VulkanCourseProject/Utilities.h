#pragma once

#include <vector>
#include <string>
#include <fstream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include "stb/stb_image.h"


const int MAX_FRAME_DRAWS = 2;
const int MAX_OBJECTS = 4;

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

struct Vertex
{
	glm::vec3 position;
	glm::vec3 color;
	glm::vec2 texCoords;
};

std::vector<char> readBinaryFile(const std::string& filename);
stbi_uc* loadTexture(std::string fileName, int* width, int* height, VkDeviceSize* imageSize);

uint32_t findMemoryTypeIndex(VkPhysicalDevice physicalDevice, const uint32_t& allowedTypes, const VkMemoryPropertyFlags& propertyFlags);

void createBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage,
	VkMemoryPropertyFlags bufferProperties, VkBuffer* buffer, VkDeviceMemory* bufferMemory);

VkCommandBuffer beginCommandBuffer(VkDevice device, VkCommandPool commandPool);
void endAndSubmitCommandBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer);
void copyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool, 
	VkBuffer sourceBuffer, VkBuffer destinationBuffer, VkDeviceSize bufferSize);
void copyImageBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool, VkBuffer sourceBuffer, VkImage image, uint32_t width, uint32_t height);

void transitionImageLayout(VkDevice device, VkQueue queue, VkCommandPool commandPool, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
