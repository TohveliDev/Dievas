#pragma once

// C++
#include <random>

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// Lightweight deterministic random number generator (PCG-inspired hash-based RNG)
class RNG
{
public:

    // Hash function (PCG-style) that scrambles a 32-bit integer seed
    // Produces high-quality pseudo-random distribution from integer state
    static uint32_t PCG_Hash(uint32_t input)
    {
        uint32_t state = input * 747796405u + 2891336453u;
        uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
        return (word >> 22u) ^ word;
    }

    // Generates a uniform random float in [0, 1)
    // Advances the seed each call (stateful RNG via reference)
    static float randomFloat(uint32_t& seed)
    {
        seed = PCG_Hash(seed);
        return (float)seed / (float)std::numeric_limits<uint32_t>::max();
    }

    // Generates a random direction in a unit sphere (uniformly distributed directions)
    static glm::vec3 inUnitSphere(uint32_t& seed)
    {
        return glm::normalize(glm::vec3(
            randomFloat(seed) * 2.0f - 1.0f,
            randomFloat(seed) * 2.0f - 1.0f,
            randomFloat(seed) * 2.0f - 1.0f)
        );
    }

    // Generates a random direction biased toward a given direction
    // roughness = 0 -> returns mostly original direction
    // roughness = 1 -> fully random direction
    static glm::vec3 randomInCone(glm::vec3 direction, float roughness, uint32_t& seed)
    {
        glm::vec3 rand = RNG::inUnitSphere(seed);

        // Linear interpolation between direction and random vector
        return glm::normalize(glm::mix(direction, rand, roughness));
    }

    // Generates a uniformly distributed random unit vector on a sphere
    // Uses spherical coordinates (z + azimuth angle)
    static glm::vec3 randomUnitVector(uint32_t& seed)
    {
        float z = randomFloat(seed) * 2.0f - 1.0f;                  // height on sphere [-1, 1]
        float a = randomFloat(seed) * 2.0f * glm::pi<float>();      // azimuth angle [0, 2pi]
        float r = glm::sqrt(1.0f - z * z);                          // radius at that height

        return glm::vec3(
            r * glm::cos(a),
            r * glm::sin(a),
            z);
    }
};
