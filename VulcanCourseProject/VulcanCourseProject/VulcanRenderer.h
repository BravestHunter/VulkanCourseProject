#pragma once

#include <stdexcept>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Utilities.h"


class VulcanRenderer
{
public:
	int Init(GLFWwindow* window);
	void Deinit();

private:
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

	VkPipeline graphicsPipeline_;
	VkPipelineLayout pipelineLayout_;
	VkRenderPass renderPass_;

	VkFormat swapchainImageFormat_;
	VkExtent2D swapchainExtent_;

	void CreateVkInstance();
	void GetPhysicalDevice();
	void CreateLogicalDevice();
	void CreateSurface(GLFWwindow* window);
	void CreateSwapchain();
	void CreateRenderPass();
	void CreateCraphicsPipeline();

	bool CheckInstanceExtensionSupport(std::vector<const char*> extensions);
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);
	bool CheckInstanceDeviceSupport(VkPhysicalDevice device);
	bool CheckValidationLayerSupport(std::vector<const char*> layers);
	bool CheckPhysicalDeviceSuitable(VkPhysicalDevice device);

	QueueFamilyIndices GetQueueFamilies(VkPhysicalDevice device);
	SwapchainDetails GetSwapchainDetails(VkPhysicalDevice device);

	VkSurfaceFormatKHR ChooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
	VkPresentModeKHR ChooseBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes);
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);

	VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
	VkShaderModule CreateShaderModule(const std::vector<char>& code);
};
