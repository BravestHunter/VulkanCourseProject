#pragma once

#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Utilities.h"


class Mesh
{
public:
	Mesh(const VkPhysicalDevice physicalDevice, const VkDevice& device, const std::vector<Vertex>& vertices);

	VkBuffer GetVertexBuffer() const;
	size_t GetVertexCount() const;

	void DestroyVertexBuffer();

private:
	VkPhysicalDevice physicalDevice_;
	VkDevice device_;
	VkBuffer vertexBuffer_;
	VkDeviceMemory vertexBufferMemory_;
	size_t vertexCount_;

	void CreateVertexBuffer(const std::vector<Vertex>& vertices);

	uint32_t FindMemoryTypeIndex(const uint32_t& allowedTypes, const VkMemoryPropertyFlags& propertyFlags);
};


inline size_t Mesh::GetVertexCount() const
{
	return vertexCount_;
}

inline VkBuffer Mesh::GetVertexBuffer() const
{
	return vertexBuffer_;
}

