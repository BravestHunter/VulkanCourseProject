#include <iostream>
#include <string>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "VulcanRenderer.h"


bool isVulcanÑompatible();
GLFWwindow* createWindow(const int width = 800, const int height = 600, std::string name = "Vulcan window");

int main()
{
	if (isVulcanÑompatible() == false)
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

		for (int i = 0; i < 4; i++)
		{
			glm::mat4 transform = glm::mat4(1.0f);
			transform = glm::rotate(transform, glm::radians(angle * (i + 1)), glm::vec3(0.0f, 0.0f, 1.0f));
			renderer.UpdateModel(i, transform);
		}

		renderer.Draw();
	}

	renderer.Deinit();

	glfwDestroyWindow(window);
	glfwTerminate();

	return EXIT_SUCCESS;
}


bool isVulcanÑompatible()
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
