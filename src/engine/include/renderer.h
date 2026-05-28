#pragma once

// Engine
#include <image.h>
#include <camera.h>
#include <ray.h>
#include <scene.h>

// C++
#include <memory>

// GLM
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/compatibility.hpp>

class Renderer
{
public:

	// Structure for Renderer settings, that can be toggled via UI
	struct Settings
	{
		bool accumulate = false;
		bool skybox = false;
	};

public:
	Renderer() = default;

	void onResize(uint32_t w, uint32_t h);
	void render(const Scene& scene, const Camera& camera);

	void setBounces(int b) { m_bounces = b; }
	void setClearCol(glm::vec3 col) { m_clearCol = col; }

	void setZenithCol(glm::vec3 col) { m_zenithColor = col; }
	void setHorizonCol(glm::vec3 col) { m_horizonColor = col; }
	void setSunCol(glm::vec3 col) { m_sunColor = col; }
	void setGlowCol(glm::vec3 col) { m_sunGlowColor = col; }
	void setSunDirection(glm::vec3 dir) { m_sunDirection = dir; }
	void setSkyIntensity(float i) { m_skyIntensity = i; }
	void setSunIntensity(float i) { m_sunIntensity = i; }
	void setGlowIntensity(float i) { m_sunGlowIntensity = i; }
	void setHorizonIntensity(float i) { m_horizonIntensity = i; }
	void setSunDiscSize(float s) { m_sunDiscSize = s; }
	void setGlowSize(float s) { m_sunGlowSize = s; }
	void setHorizonFalloff(float f) { m_horizonFalloff = f; }

	const glm::vec3 getClearCol() const { return m_clearCol; }
	const int getBounces() const { return m_bounces; }

	const glm::vec3 getZenithColor() const { return m_zenithColor; }
	const glm::vec3 getHorizonColor() const { return m_horizonColor; }
	const glm::vec3 getSunColor() const { return m_sunColor; }
	const glm::vec3 getGlowColor() const { return m_sunGlowColor; }
	const glm::vec3 getSunDirection() const { return m_sunDirection; }

	const float getSkyIntensity() const { return m_skyIntensity; }
	const float getSunIntensity() const { return m_sunIntensity; }
	const float getGlowIntensity() const { return m_sunGlowIntensity; }
	const float getHorizonIntensity() const { return m_horizonIntensity; }
	const float getSunDiscSize() const { return m_sunDiscSize; }
	const float getSunGlowSize() const { return m_sunGlowSize; }
	const float getHorizonFalloff() const { return m_horizonFalloff; }

	void recalculateScene(Scene& scene);

	std::shared_ptr<Image> getImage() const { return m_image; }

	void resetFrameIndex() { m_frameIndex = 1; }
	uint32_t getFrameIndex() { return m_frameIndex; }
	Settings& getSettings() { return m_settings; }

private:

	// Stores information about a ray-scene intersection used for shading and material evaluation
	struct HitPayload
	{
		float hitDistance;			// Distance from ray origin to intersection point
		int objIndex;				// Index of the hit object in the scene object array
		int triangleIndex;			// Index of the hit triangle within the mesh
		bool frontFace;				// True if the ray hit the front face (used for normal orientation)

		glm::vec3 worldPosition;	// World-space intersection position
		glm::vec3 worldNormal;		// World-space shading normal (already corrected for face orientation)
									
		glm::vec3 tangent;			// Orthonormal tangent basis used for shading and texture space transforms
		glm::vec3 bitangent;
	};

	// Represents a sampled direction from a BSDF along with its contribution.
	struct BSDFSample
	{
		glm::vec3 direction;	// Sampled outgoing light direction (world space)
		glm::vec3 f;			// Evaluated BSDF value f(x, wi, wo) for this sample
		glm::vec3 weight;		// Final Monte Carlo throughput weight (f * cos / pdf)
		float pdf;				// Probability density function value for the sampled direction
		bool specular;			// True if this sample came from a specular lobe (used for MIS / logic branching)
	};

	glm::vec4 rayGen(uint32_t x, uint32_t y, const auto& objects, const auto& materials, const auto& meshes);
	bool intersectTriangle(const Ray& ray, const Triangle& tri, float& t);
	float intersectAABB_T(const AABB& box, const Ray& ray);
	bool traverseBVH(const Mesh& mesh, int nodeIndex, const Ray& ray, float& closestT, int& hitTriangle);
	bool traverseTLAS(int nodeIndex, const Ray& ray, float& closestT, int& hitObject, int& hitTriangle);

	bool intersectAABB(const AABB& box, const Ray& ray, float maxDistance);
	glm::vec3 sampleSky(const glm::vec3& direction);

	BSDFSample sampleDiffuse(const Material& mat, const glm::vec3& N, const glm::vec3& V, const glm::vec3& T, const glm::vec3& B, uint32_t& seed);
	BSDFSample sampleGGX(const Material& mat, const glm::vec3& N, const glm::vec3& V, const glm::vec3& T, const glm::vec3& B, uint32_t& seed);
	BSDFSample sampleBSDF(const Material& mat, const glm::vec3& N, const glm::vec3& V, const glm::vec3& T, const glm::vec3& B, uint32_t& seed);

	HitPayload traceRay(const Ray& ray);
	HitPayload closestHit(const Ray& ray, float hitDistance, int objIndex, int triIndex);
	HitPayload miss(const Ray& ray);
	void buildOrthonormalBasis(const glm::vec3& normal, glm::vec3& tangent, glm::vec3& bitangent);
	int buildTLASNode(Scene& scene, int start, int count);
	void buildTLAS(Scene& scene);
	void rotateTangentFrame(glm::vec3& T, glm::vec3& B, const glm::vec3& N, float angle);

private:
	int m_bounces = 5;
	Settings m_settings;

	std::vector<uint32_t> m_imageHorizontalIterator, m_imageVerticalIterator;

	const Scene* m_activeScene = nullptr;
	const Camera* m_activeCamera = nullptr;

	std::shared_ptr<Image> m_image;
	uint32_t* m_imageData = nullptr;
	glm::vec4* m_accumulationData = nullptr;

	glm::vec3 m_clearCol{0.17f, 0.17f, 0.17f};

	glm::vec3 m_zenithColor{ 0.1f, 0.3f, 0.8f };
	glm::vec3 m_horizonColor{ 0.9f, 0.6f, 0.4f };
	glm::vec3 m_sunColor{ 1.0f, 0.95f, 0.8f };
	glm::vec3 m_sunGlowColor{ 1.0f, 0.9f, 0.6f };
	glm::vec3 m_sunDirection{ 0.3f, 0.7f, 0.2f };

	float m_skyIntensity = 1.0f;
	float m_sunIntensity = 8.0f;
	float m_sunGlowIntensity = 0.4f;
	float m_horizonIntensity = 0.15f;
	float m_sunDiscSize = 1024.0f;
	float m_sunGlowSize = 32.0f;
	float m_horizonFalloff = 8.0f;

	uint32_t m_frameIndex = 1;
};