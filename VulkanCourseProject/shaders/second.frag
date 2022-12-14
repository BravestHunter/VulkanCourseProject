#version 450

layout(input_attachment_index = 0, binding = 0) uniform subpassInput inputColor;
layout(input_attachment_index = 1, binding = 1) uniform subpassInput inputDepth;

layout(location = 0) out vec4 outColor;

int borderCoord = 400;
float lowerBound = 0.95;
float upperBound = 1.0;

void main()
{
	if (gl_FragCoord.x > borderCoord)
	{
		float depth = subpassLoad(inputDepth).r;
		float depthScaled = 1.0 - ((depth - lowerBound) / (upperBound - lowerBound));
		outColor = vec4(depthScaled, 0.0, 0.0, 1.0);
	}
	else
	{
		outColor = subpassLoad(inputColor).rgba;
	}
}
