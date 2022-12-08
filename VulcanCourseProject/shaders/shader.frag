#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 texCoords;

layout(set = 1, binding = 0) uniform sampler2D textureSampler;


layout(location = 0) out vec4 outColor;

void main()
{
	vec4 textureColor = texture(textureSampler, texCoords);
	outColor = vec4(mix(textureColor.rgb, fragColor, 0.5), textureColor.a);
}
