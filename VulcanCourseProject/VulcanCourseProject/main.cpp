#include <iostream>
#include <string>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "VulcanRenderer.h"


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

	VulcanRenderer renderer;

	if (renderer.Init(window) == EXIT_FAILURE)
	{
		return EXIT_FAILURE;
	}

	while (glfwWindowShouldClose(window) == false)
	{
		glfwPollEvents();
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
