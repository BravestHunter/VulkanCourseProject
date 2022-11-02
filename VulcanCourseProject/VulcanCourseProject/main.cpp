#include <iostream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>


int VulcanCompabilityTest();


int main()
{
	return VulcanCompabilityTest();
}


int VulcanCompabilityTest()
{
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	printf("Extension count: %i\n", extensionCount);

	if (extensionCount == 0)
	{
		return -1;
	}

	return 0;
}
