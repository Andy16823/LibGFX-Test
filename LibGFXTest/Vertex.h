#pragma once
#include <glm/glm.hpp>

struct Vertex3D
{
	glm::vec3 position;
	glm::vec3 color;
	glm::vec3 normal;
	glm::vec2 texCoord;
};