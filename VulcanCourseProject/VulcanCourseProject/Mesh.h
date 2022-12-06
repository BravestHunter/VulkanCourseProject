#pragma once

#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Utilities.h"


struct UboModel
{
	glm::mat4 model;
};

class Mesh
{
public:
	Mesh(const VkPhysicalDevice physicalDevice, const VkDevice& device, const VkQueue& transferQueue, 
		const VkCommandPool& transferCommandPool, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

	void SetModel(const UboModel& model);
	const UboModel& GetModel() const;

	VkBuffer GetVertexBuffer() const;
	size_t GetVertexCount() const;

	VkBuffer GetIndexBuffer() const;
	size_t GetIndexCount() const;

	void DestroyBuffers();

private:
	UboModel uboModel_;

	VkPhysicalDevice physicalDevice_;
	VkDevice device_;

	VkBuffer vertexBuffer_;
	VkDeviceMemory vertexBufferMemory_;
	size_t vertexCount_;

	VkBuffer indexBuffer_;
	VkDeviceMemory indexBufferMemory_;
	size_t indexCount_;

	void CreateVertexBuffer(const std::vector<Vertex>& vertices, const VkQueue& transferQueue, const VkCommandPool& transferCommandPool);
	void CreateIndexBuffer(const std::vector<uint32_t>& indices, const VkQueue& transferQueue, const VkCommandPool& transferCommandPool);
};


inline void Mesh::SetModel(const UboModel& model)
{
	uboModel_ = model;
}

inline const UboModel& Mesh::GetModel() const
{
	return uboModel_;
}

inline size_t Mesh::GetVertexCount() const
{
	return vertexCount_;
}

inline VkBuffer Mesh::GetVertexBuffer() const
{
	return vertexBuffer_;
}

inline VkBuffer Mesh::GetIndexBuffer() const
{
	return indexBuffer_;
}

inline size_t Mesh::GetIndexCount() const
{
	return indexCount_;
}
