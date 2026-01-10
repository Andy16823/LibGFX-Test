#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 vcolor;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 texCoord;

layout(set = 0, binding = 0) uniform UboViewProjection {
    mat4 projection;
    mat4 view;
} uboViewProjection;

layout(location = 0) out vec3 color;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    gl_Position = uboViewProjection.projection * uboViewProjection.view * vec4(pos, 1.0);
    color = vcolor;
    fragTexCoord = texCoord;
}