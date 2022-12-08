#include "Mesh.h"

Mesh::Mesh(const VkPhysicalDevice physicalDevice, const VkDevice& device, const VkQueue& transferQueue, 
	const VkCommandPool& transferCommandPool, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, int textureId) :
	model_({ glm::mat4(1.0f) }),
	textureId_(textureId),
	physicalDevice_(physicalDevice),
	device_(device),
	vertexCount_(vertices.size()),
	indexCount_(indices.size())
{
	CreateVertexBuffer(vertices, transferQueue, transferCommandPool);
	CreateIndexBuffer(indices, transferQueue, transferCommandPool);
}


void Mesh::DestroyBuffers()
{
	vkDestroyBuffer(device_, vertexBuffer_, nullptr);
	vkFreeMemory(device_, vertexBufferMemory_, nullptr);

	vkDestroyBuffer(device_, indexBuffer_, nullptr);
	vkFreeMemory(device_, indexBufferMemory_, nullptr);
}


void Mesh::CreateVertexBuffer(const std::vector<Vertex>& vertices, const VkQueue& transferQueue, const VkCommandPool& transferCommandPool)
{
	VkDeviceSize bufferSize = sizeof(Vertex) * vertices.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(physicalDevice_, device_, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);

	void* data;
	vkMapMemory(device_, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
	vkUnmapMemory(device_, stagingBufferMemory);

	createBuffer(physicalDevice_, device_, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vertexBuffer_, &vertexBufferMemory_);

	copyBuffer(device_, transferQueue, transferCommandPool, stagingBuffer, vertexBuffer_, bufferSize);

	vkDestroyBuffer(device_, stagingBuffer, nullptr);
	vkFreeMemory(device_, stagingBufferMemory, nullptr);
}

void Mesh::CreateIndexBuffer(const std::vector<uint32_t>& indices, const VkQueue& transferQueue, const VkCommandPool& transferCommandPool)
{
	VkDeviceSize bufferSize = sizeof(uint32_t) * indices.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(physicalDevice_, device_, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);

	void* data;
	vkMapMemory(device_, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices.data(), static_cast<size_t>(bufferSize));
	vkUnmapMemory(device_, stagingBufferMemory);

	createBuffer(physicalDevice_, device_, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &indexBuffer_, &indexBufferMemory_);

	copyBuffer(device_, transferQueue, transferCommandPool, stagingBuffer, indexBuffer_, bufferSize);

	vkDestroyBuffer(device_, stagingBuffer, nullptr);
	vkFreeMemory(device_, stagingBufferMemory, nullptr);
}
