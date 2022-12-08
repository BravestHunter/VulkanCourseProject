#version 450

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec2 aTexCoords;

layout(set = 0, binding = 0) uniform UboViewProjection {
	mat4 projection;
	mat4 view;
} uboViewProjection;

// Not used
layout(set = 0, binding = 1) uniform UboModel {
	mat4 model;
} uboModel;

layout (push_constant) uniform PushModel {
	mat4 model;
} pushModel;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 texCoords;

void main()
{
	gl_Position = uboViewProjection.projection * uboViewProjection.view * pushModel.model * vec4(aPosition, 1.0);

	fragColor = aColor;
	texCoords = aTexCoords;
}
