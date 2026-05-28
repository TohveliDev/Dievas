#pragma once

// GLM
#include <glm/glm.hpp>

// Base structure for a Ray
struct Ray
{
	glm::vec3 origin;
	glm::vec3 direction;
};