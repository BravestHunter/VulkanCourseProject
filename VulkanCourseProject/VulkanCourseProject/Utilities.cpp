#include "Utilities.h"


std::vector<char> readBinaryFile(const std::string& filename)
{
	std::ifstream file(filename, std::ios::binary | std::ios::ate);

	if (file.is_open() == false)
	{
		throw std::runtime_error("Failed to open a file.");
	}

	size_t fileSize = static_cast<size_t>(file.tellg());
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	return buffer;
}

stbi_uc* loadTexture(std::string fileName, int* width, int* height, VkDeviceSize* imageSize)
{
	int channels;
	std::string filePath = "../textures/" + fileName;
	stbi_uc* imageData = stbi_load(filePath.c_str(), width, height, &channels, STBI_rgb_alpha);

	if (!imageData)
	{
		throw std::runtime_error("Failed to load a texture file.");
	}

	*imageSize = (*width) * (*height) * 4;

	return imageData;
}


uint32_t findMemoryTypeIndex(VkPhysicalDevice physicalDevice, const uint32_t& allowedTypes, const VkMemoryPropertyFlags& propertyFlags)
{
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
	{
		if ((allowedTypes & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & propertyFlags))
		{
			return i;
		}
	}

	throw std::runtime_error("Failed to find a memory type index.");
}


void createBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage,
	VkMemoryPropertyFlags bufferProperties, VkBuffer* buffer, VkDeviceMemory* bufferMemory)
{
	VkBufferCreateInfo bufferCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = bufferSize,
		.usage = bufferUsage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	VkResult result = vkCreateBuffer(device, &bufferCreateInfo, nullptr, buffer);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a vertex buffer.");
	}

	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(device, *buffer, &memoryRequirements);

	VkMemoryAllocateInfo memoryAllocateInfo
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memoryRequirements.size,
		.memoryTypeIndex = findMemoryTypeIndex(physicalDevice, memoryRequirements.memoryTypeBits, bufferProperties)
	};

	result = vkAllocateMemory(device, &memoryAllocateInfo, nullptr, bufferMemory);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate memory for vertex buffer.");
	}

	result = vkBindBufferMemory(device, *buffer, *bufferMemory, 0);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to bind vertex buffer to memory.");
	}
}

VkCommandBuffer beginCommandBuffer(VkDevice device, VkCommandPool commandPool)
{
	VkCommandBufferAllocateInfo commandBufferAllocateInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = commandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer);

	VkCommandBufferBeginInfo commandBufferBeginInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};

	vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);

	return commandBuffer;
}

void endAndSubmitCommandBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer)
{
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo transferSubmitInfo
	{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &commandBuffer
	};

	VkResult result = vkQueueSubmit(queue, 1, &transferSubmitInfo, VK_NULL_HANDLE);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to submit command to transfer queue.");
	}
	vkQueueWaitIdle(queue);

	vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void copyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool, 
	VkBuffer sourceBuffer, VkBuffer destinationBuffer, VkDeviceSize bufferSize)
{
	VkBufferCopy bufferCopyRegion
	{
		.srcOffset = 0,
		.dstOffset = 0,
		.size = bufferSize
	};

	VkCommandBuffer transferCommandBuffer = beginCommandBuffer(device, transferCommandPool);
	{
		vkCmdCopyBuffer(transferCommandBuffer, sourceBuffer, destinationBuffer, 1, &bufferCopyRegion);
	}
	endAndSubmitCommandBuffer(device, transferCommandPool, transferQueue, transferCommandBuffer);
}

void copyImageBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool, VkBuffer sourceBuffer, VkImage image, uint32_t width, uint32_t height)
{
	VkBufferImageCopy imageCopyRegion
	{
		.bufferOffset = 0,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource = VkImageSubresourceLayers
		{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1
		},
		.imageOffset = { 0, 0, 0 },
		.imageExtent = { width, height, 1 }
	};

	VkCommandBuffer transferCommandBuffer = beginCommandBuffer(device, transferCommandPool);
	{
		vkCmdCopyBufferToImage(transferCommandBuffer, sourceBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, & imageCopyRegion);
	}
	endAndSubmitCommandBuffer(device, transferCommandPool, transferQueue, transferCommandBuffer);
}


void transitionImageLayout(VkDevice device, VkQueue queue, VkCommandPool commandPool, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkImageMemoryBarrier imageMemoryBarrier
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		//.srcAccessMask = VK_ACCESS_NONE,
		//.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.oldLayout = oldLayout,
		.newLayout = newLayout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = image,
		.subresourceRange = VkImageSubresourceRange
		{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		}
	};

	VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_NONE;
	VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_NONE;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_NONE;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}

	VkCommandBuffer commandBuffer = beginCommandBuffer(device, commandPool);
	{
		vkCmdPipelineBarrier(
			commandBuffer,
			srcStage, dstStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &imageMemoryBarrier
		);
	}
	endAndSubmitCommandBuffer(device, commandPool, queue, commandBuffer);
}
