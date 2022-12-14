#include <iostream>
#include <string>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "VulkanRenderer.h"


bool isVulcan—ompatible();
GLFWwindow* createWindow(const int width = 800, const int height = 600, std::string name = "Vulcan window");

int main()
{
	if (isVulcan—ompatible() == false)
	{
		return EXIT_FAILURE;
	}

	glfwInit();

	GLFWwindow* window = createWindow();

	VulkanRenderer renderer;

	if (renderer.Init(window) == EXIT_FAILURE)
	{
		return EXIT_FAILURE;
	}

	float angle = 0.0f;
	float lastTime = 0.0f;
	float deltaTime = 0.0f;

	while (glfwWindowShouldClose(window) == false)
	{
		glfwPollEvents();

		float now = glfwGetTime();
		float deltaTime = now - lastTime;
		lastTime = now;

		angle += 10.0f * deltaTime;
		if (angle > 360.0f)
		{
			angle -= 360.0f;
		}

		glm::mat4 transform = glm::mat4(1.0f);
		transform = glm::rotate(transform, glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));
		transform = glm::scale(transform, glm::vec3(0.01f));
		renderer.UpdateModel(0, transform);

		renderer.Draw();
	}

	renderer.Deinit();

	glfwDestroyWindow(window);
	glfwTerminate();

	return EXIT_SUCCESS;
}


bool isVulcan—ompatible()
{
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	if (extensionCount == 0)
	{
		return false;
	}

	return true;
}

GLFWwindow* createWindow(const int width, const int height, std::string name)
{
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	return glfwCreateWindow(width, height, name.c_str(), nullptr, nullptr);
}
