#version 450

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aColor;

layout(binding = 0) uniform UboViewProjection {
	mat4 projection;
	mat4 view;
} uboViewProjection;

// Not used
layout(binding = 1) uniform UboModel {
	mat4 model;
} uboModel;

layout (push_constant) uniform PushModel {
	mat4 model;
} pushModel;

layout(location = 0) out vec3 fragColor;

void main()
{
	gl_Position = uboViewProjection.projection * uboViewProjection.view * pushModel.model * vec4(aPosition, 1.0);
	fragColor = aColor;
}
