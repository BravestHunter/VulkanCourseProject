#pragma once


struct QueueFamilyIndices
{
	int graphicsFamily = -1;

	bool IsValid()
	{
		return graphicsFamily >=0;
	}
};
