#include "Mesh.h"

Mesh::Mesh(const VkPhysicalDevice physicalDevice, const VkDevice& device, const std::vector<Vertex>& vertices) :
	physicalDevice_(physicalDevice),
	device_(device),
	vertexCount_(vertices.size())
{
	CreateVertexBuffer(vertices);
}


void Mesh::DestroyVertexBuffer()
{
	vkDestroyBuffer(device_, vertexBuffer_, nullptr);
	vkFreeMemory(device_, vertexBufferMemory_, nullptr);
}


void Mesh::CreateVertexBuffer(const std::vector<Vertex>& vertices)
{
	VkBufferCreateInfo bufferCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(Vertex) * vertices.size(),
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	VkResult result = vkCreateBuffer(device_, &bufferCreateInfo, nullptr, &vertexBuffer_);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a vertex buffer.");
	}

	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(device_, vertexBuffer_, &memoryRequirements);

	VkMemoryAllocateInfo memoryAllocateInfo
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memoryRequirements.size,
		.memoryTypeIndex = FindMemoryTypeIndex(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
	};

	result = vkAllocateMemory(device_, &memoryAllocateInfo, nullptr, &vertexBufferMemory_);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate memory for vertex buffer.");
	}

	result = vkBindBufferMemory(device_, vertexBuffer_, vertexBufferMemory_, 0);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to bind vertex buffer to memory.");
	}

	void* data;
	vkMapMemory(device_, vertexBufferMemory_, 0, bufferCreateInfo.size, 0, &data);
	memcpy(data, vertices.data(), static_cast<size_t>(bufferCreateInfo.size));
	vkUnmapMemory(device_, vertexBufferMemory_);
}


uint32_t Mesh::FindMemoryTypeIndex(const uint32_t& allowedTypes, const VkMemoryPropertyFlags& propertyFlags)
{
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memoryProperties);

	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
	{
		if ((allowedTypes & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & propertyFlags))
		{
			return i;
		}
	}

	throw std::runtime_error("Failed to find a memory type index.");
}
