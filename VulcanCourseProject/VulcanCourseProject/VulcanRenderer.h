#pragma once

#include <stdexcept>
#include <vector>
#include <memory>

#include <glm/gtc/matrix_transform.hpp>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Utilities.h"
#include "Mesh.h"


class VulcanRenderer
{
public:
	int Init(GLFWwindow* window);
	void Deinit();

	void Draw();

	void UpdateModel(const int& index, const glm::mat4& model);

private:
	int currentFrame_ = 0;

	GLFWwindow* window_;
	VkInstance vkInsatance_;
	struct {
		VkPhysicalDevice physicalDevice;
		VkDevice logicalDevice;
	} mainDevice;
	VkQueue graphicsQueue_;
	VkQueue presentationQueue_;
	VkSurfaceKHR surface_;
	VkSwapchainKHR swapchain_;
	std::vector<SwapchainImage> swapchainImages_;
	std::vector<VkFramebuffer> swapchainFramebuffers_;
	std::vector<VkCommandBuffer> commandBuffers_;

	VkFormat depthBufferImageFormat_;
	VkImage depthBufferImage_;
	VkDeviceMemory depthBufferImageMemory_;
	VkImageView depthBufferImageView_;

	VkDescriptorSetLayout descriptorSetLayout_;
	VkPushConstantRange pushConstantRange_;

	std::vector<VkBuffer> vpUniformBuffer_;
	std::vector<VkDeviceMemory> vpUniformBufferMemory_;

	std::vector<VkBuffer> modelDynamicUniformBuffer_;
	std::vector<VkDeviceMemory> modelDynamicUniformBufferMemory_;

	//VkDeviceSize minUniformBufferOffset_;
	//size_t modelUniformAlignment_;
	//Model* modelTransferSpace_;

	VkDescriptorPool descriptorPool_;
	std::vector<VkDescriptorSet> descriptorSets_;

	VkPipeline graphicsPipeline_;
	VkPipelineLayout pipelineLayout_;
	VkRenderPass renderPass_;

	VkCommandPool graphicsCommandPool_;

	VkFormat swapchainImageFormat_;
	VkExtent2D swapchainExtent_;

	std::vector<VkSemaphore> imageAvailableSemaphores_;
	std::vector<VkSemaphore> renderFinishedSemaphores_;
	std::vector<VkFence> drawFences_;

	std::vector<std::unique_ptr<Mesh>> meshes_;

	struct UboViewProjection {
		glm::mat4 projection;
		glm::mat4 view;
	} uboViewProjection_;

	void CreateVkInstance();
	void GetPhysicalDevice();
	void CreateLogicalDevice();
	void CreateSurface(GLFWwindow* window);
	void CreateSwapchain();
	void CreateRenderPass();
	void CreateDescriptorSetLayout();
	void CreatePushConstantRange();
	void CreateCraphicsPipeline();
	void CreateDepthBufferImage();
	void CreateFramebuffers();
	void CreateCommandPool();
	void CreateCommandBuffers();
	void CreateSynchronization();
	void CreateUniformBuffers();
	void CreateDescriptorPool();
	void CreateDescriptorSets();

	bool CheckInstanceExtensionSupport(std::vector<const char*> extensions);
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);
	bool CheckInstanceDeviceSupport(VkPhysicalDevice device);
	bool CheckValidationLayerSupport(std::vector<const char*> layers);
	bool CheckPhysicalDeviceSuitable(VkPhysicalDevice device);

	void RecordCommands(uint32_t imageIndex);
	void UpdateUniformBuffers(uint32_t imageIndex);
	void CreateMeshes();

	QueueFamilyIndices GetQueueFamilies(VkPhysicalDevice device);
	SwapchainDetails GetSwapchainDetails(VkPhysicalDevice device);

	VkSurfaceFormatKHR ChooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
	VkPresentModeKHR ChooseBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes);
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);
	VkFormat ChooseSupportedFormat(const std::vector<VkFormat>& formats, const VkImageTiling tiling, const VkFormatFeatureFlags featureFlags);

	VkImage CreateImage(const uint32_t width, const uint32_t height, const VkFormat format, const VkImageTiling tiling, 
		const VkImageUsageFlags useFlags, const VkMemoryPropertyFlags propFlags, VkDeviceMemory* imageMemory);
	VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
	VkShaderModule CreateShaderModule(const std::vector<char>& code);

	void AllocateDynamicBufferTransferSpace();
};
