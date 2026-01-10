#version 450

layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec3 color;
layout(location = 1) in vec2 fragTexCoord;

void main() { 
    fragColor = vec4(color, 1.0);
}