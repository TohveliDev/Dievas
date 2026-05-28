#pragma once

// C++
#include <string>

// GLM
#include <glm/glm.hpp>

struct Material
{
	std::string name;
	glm::vec3 albedo{ 1.0f };			// Base color for dielectric materials
	float roughness = 0.0f;				// Surface Roughness (0.0 = mirror smooth, 1.0 = fully rough)
	float metallic = 0.0f;				// Metallic factor (0.0 = dielectric, 1.0 = metal)
	float anisotropy = 0.0f;			// Stength of anistropic highlights
	float anisotropyRotation = 0.0f;	// Rotation angle of anistrophic direction
	float emissionPower = 0.0f;			// Emissive intensity multiplier
	glm::vec3 emissionColor{ 0.0f };	// Emissive radiance color
	float transmission = 0.0f;			// Amount of light transmitted through the material (0.0 = solid, 1.0 = transparent)
	float ior = 1.0f;					// Index of Reffraction
	glm::vec3 absorption{ 0.f };		// Beer-Lambert absorption coefficients

	glm::vec3 getEmission() const { return emissionColor * emissionPower; }

	// Calculates the Beer-Lampert absorption based on the Albedo
	void calculateAbsorption()
	{
		const float epsilon = 1e-4f;

		glm::vec3 color = glm::clamp(albedo, glm::vec3(epsilon), glm::vec3(0.999f)); // Prevents infinite values, if Albedo is 0.0f

		absorption = -log(color);
	}
};