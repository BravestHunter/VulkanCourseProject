#pragma once

#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Utilities.h"


struct Model
{
	glm::mat4 model;
};

class Mesh
{
public:
	Mesh(const VkPhysicalDevice physicalDevice, const VkDevice& device, const VkQueue& transferQueue, 
		const VkCommandPool& transferCommandPool, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, int textureId);

	void SetModel(const Model& model);
	const Model& GetModel() const;
	const int GetTextureId() const;

	VkBuffer GetVertexBuffer() const;
	size_t GetVertexCount() const;
	VkBuffer GetIndexBuffer() const;
	size_t GetIndexCount() const;

	void DestroyBuffers();

private:
	Model model_;
	int textureId_;

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


inline void Mesh::SetModel(const Model& model)
{
	model_ = model;
}

inline const Model& Mesh::GetModel() const
{
	return model_;
}

inline const int Mesh::GetTextureId() const
{
	return textureId_;
}


inline VkBuffer Mesh::GetVertexBuffer() const
{
	return vertexBuffer_;
}

inline size_t Mesh::GetVertexCount() const
{
	return vertexCount_;
}

inline VkBuffer Mesh::GetIndexBuffer() const
{
	return indexBuffer_;
}

inline size_t Mesh::GetIndexCount() const
{
	return indexCount_;
}
