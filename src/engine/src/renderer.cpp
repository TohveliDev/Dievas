// Engine
#include <renderer.h>
#include <random.h>

// C++
#include <execution>

namespace Utils
{
	// RGBA vec4 -> ABGR uint32_t
	static uint32_t convertToRGBA(const glm::vec4& color)
	{
		glm::vec4 c = glm::clamp(color, 0.0f, 1.0f);

		uint8_t r = (uint8_t)(c.r * 255.0f);
		uint8_t g = (uint8_t)(c.g * 255.0f);
		uint8_t b = (uint8_t)(c.b * 255.0f);
		uint8_t a = (uint8_t)(c.a * 255.0f);

		return (a << 24) | (b << 16) | (g << 8) | r;
	}

	// Transforms a world-space ray into local space
	static Ray toLocalRay(const Ray& ray, const Object& obj)
	{
		Ray local;

		glm::mat4 inv = obj.invTransform;

		// Transform origin using homogenous position coordinates
		local.origin = glm::vec3(inv * glm::vec4(ray.origin, 1.0f));

		// Transform direction without translation component
		local.direction = glm::normalize(glm::vec3(inv * glm::vec4(ray.direction, 0.0f)));

		// Prevent invalid zero-length directions after transformation
		if (glm::length2(local.direction) < 1e-12f)
			local.direction = glm::vec3(0, 0, 1);

		return local;
	}

	// Importance-samples an anisotropic GGX microfacet normal.
	// 
	// N - Surface normal
	// T - Tangent direction
	// B - Bitangent direction
	// ax - Roughness along the tangent axis
	// ay - Roughness along the bitangent axis
	static glm::vec3 sampleAnistrophicGGX(const glm::vec3& N, const glm::vec3& T, const glm::vec3& B, float ax, float ay, uint32_t& seed)
	{
		float u1 = RNG::randomFloat(seed);
		float u2 = RNG::randomFloat(seed);

		// Sample the anistropic azimuthal distribution
		float phi = atan2(ay * sin(2.0f * glm::pi<float>() * u1), ax * cos(2.0f * glm::pi<float>() * u1));

		// Remapping to the full hemisphere range
		if (u1 > 0.5f)
			phi += glm::pi<float>();

		float sinPhi = sin(phi);
		float cosPhi = cos(phi);

		// Direction-dependent roughness term
		float alpha2 = 1.0f / ((cosPhi * cosPhi) / (ax * ax) + (sinPhi * sinPhi) / (ay * ay));

		// GGX slope distribution sampling
		float tanTheta2 = u2 / (1.0f - u2) * alpha2;

		float cosTheta = 1.0f / sqrt(1.0f + tanTheta2);

		float sinTheta = sqrt(glm::max(0.0f, 1.0f - cosTheta * cosTheta));

		// Transform sampled half-vector from tangent space into world space
		glm::vec3 H = glm::normalize(T * (sinTheta * cosPhi) + B * (sinTheta * sinPhi) + N * cosTheta);

		return H;
	}

	// GGX / Trowbridge-Reitz normal distribution (NDF)
	//
	// NdotH - Cosine between surface normal and half-vector
	// alpha - Surface roughness parameter
	static float D_GGX(float NdotH, float alpha)
	{
		float a2 = alpha * alpha;
		float d = (NdotH * NdotH) * (a2 - 1.0f) + 1.0f;

		return a2 / (glm::pi<float>() * d * d);
	}

	// Smith masking-shadowing term for a single direction
	static float G1(float NdotV, float alpha)
	{
		float a2 = alpha * alpha;

		float denom = NdotV + sqrt(a2 + (1.0f - a2) * NdotV * NdotV);

		return (2.0f * NdotV) / denom;
	}

	// Full Smith geometry term
	static float G_Smith(float NdotV, float NdotL, float alpha)
	{
		return G1(NdotV, alpha) * G1(NdotL, alpha);
	}

	// Cosine-weighted hemisphere sampling.
	static glm::vec3 sampleCosineHemisphere(float u1, float u2)
	{
		float r = sqrt(u1);
		float theta = 2.0f * glm::pi<float>() * u2;

		float x = r * cos(theta);
		float y = r * sin(theta);
		float z = sqrt(1.0f - u1);

		return glm::vec3(x, y, z);
	}

	// Schlick Fresnel approximation.
	//
	// cosTheta - Cosine between view direction and half-vector
	// F0 - Reflectance at normal incidence
	static glm::vec3 fresnelSchlick(float cosTheta, glm::vec3 F0)
	{
		float x = 1.0f - cosTheta;
		return F0 + (1.0f - F0) * (x * x * x * x * x);
	}

	// Computes the bounding box for a contiguous range of TLAS primitives.
	static AABB computeTLASBounds(const Scene& scene, int start, int count)
	{
		AABB box = createEmptyAABB();

		for (int i = 0; i < count; i++)
		{
			int primIndex = scene.tlasIndices[start + i];

			const TLASPrimitive& prim = scene.tlasPrimitives[primIndex];

			expandAABB(box, prim.bounds);
		}

		return box;
	}

	// Transforms a tangent-space direction into world space.
	// T, B, N form an orthonormal tangent basis.
	static glm::vec3 toWorld(const glm::vec3& local, const glm::vec3& T, const glm::vec3& B, const glm::vec3& N)
	{
		return glm::normalize(
			local.x * T +
			local.y * B +
			local.z * N);
	}

	// Probability density function for GGX half-vector sampling
	static float pdfGGX(float D, float NdotH, float HdotV)
	{
		return D * NdotH / (4.0f * HdotV);
	}

	// Evaluates the Cook-Torrance specular BRDF term.
	// Combines Fresnel term (F), GGX normal distribution (D) and Smith geometry term (G)
	static glm::vec3 evaluateSpecular(const glm::vec3& F, float D, float G, float NdotL, float NdotV)
	{
		return (D * G * F) / glm::max(4.0f * NdotL * NdotV, 1e-5f);
	}
}

// When the viewport is resized, delete the previous image data and create new one with the correct resolution
void Renderer::onResize(uint32_t w, uint32_t h)
{
	if (m_image)
	{
		if (m_image->getWidth() == w && m_image->getHeight() == h)
		{
			return;
		}

		m_image->resize(w, h);
	}

	else
	{
		m_image = std::make_shared<Image>(w, h, ImageFormat::RGBA);
	}

	delete[] m_imageData;
	m_imageData = new uint32_t[w * h];

	delete[] m_accumulationData;
	m_accumulationData = new glm::vec4[w * h];

	m_frameIndex = 1;
	m_imageHorizontalIterator.resize(w);
	m_imageVerticalIterator.resize(h);

	for (uint32_t i = 0; i < w; i++)
		m_imageHorizontalIterator[i] = i;

	for (uint32_t j = 0; j < h; j++)
		m_imageVerticalIterator[j] = j;
}

// Renders a singular frame of the scene using progressive accumulation.
void Renderer::render(const Scene& scene, const Camera& camera)
{
    m_activeCamera = &camera;
    m_activeScene = &scene;
	const auto& objects = scene.objects;
	const auto& materials = scene.materials;
	const auto& meshes = scene.meshes;

    const uint32_t width  = m_image->getWidth();
    const uint32_t height = m_image->getHeight();
    const uint32_t pixelCount = width * height;

	// Reset accumulation buffer on the first frame of a "sequence"
    if (m_frameIndex == 1)
        std::memset(m_accumulationData, 0, pixelCount * sizeof(glm::vec4));

    const float invFrame = 1.0f / (float)m_frameIndex;

	// Rendering is executed in parallel
    std::for_each(std::execution::par,
        m_imageVerticalIterator.begin(),
        m_imageVerticalIterator.end(),
        [&](uint32_t y)
    {
        const uint32_t row = y * width;

        for (uint32_t x = 0; x < width; x++)
        {
            const uint32_t i = row + x;

			// Generate primary ray and evaluate scene lighting
            glm::vec4 color = rayGen(x, y, objects, materials, meshes);

			// Accumulate for progressive refinement
            glm::vec4& accum = m_accumulationData[i];
            accum += color;

			// Compute running average and write final pixel
			glm::vec4 result = accum * invFrame;
			m_imageData[i] = Utils::convertToRGBA(result);
        }
    });

    m_image->setData(m_imageData);

	// Advance accumulateion frame (if enabled)
    m_frameIndex = m_settings.accumulate ? (m_frameIndex + 1) : 1;
}

// Path-traces a single camera ray and returns the radiance contribution
glm::vec4 Renderer::rayGen(uint32_t x, uint32_t y, const auto& objects, const auto& materials, const auto& meshes)
{
	Ray ray;

	ray.origin = m_activeCamera->getPosition();

	ray.direction = m_activeCamera->getRayDirections()[x + y * m_image->getWidth()];

	uint32_t seed = x + y * m_image->getWidth();

	seed ^= m_frameIndex * 9781u;

	glm::vec3 L(0.0f);
	glm::vec3 T(1.0f);

	for (int bounce = 0; bounce < m_bounces; bounce++)
	{
		seed = RNG::PCG_Hash(seed + bounce);

		HitPayload hit = traceRay(ray);

		// Miss: Sample enviroment lighting
		if (hit.hitDistance < 0.0f)
		{
			glm::vec3 env = m_settings.skybox
				? sampleSky(ray.direction)
				: m_clearCol;

			L += T * env;

			break;
		}

		const Object& obj = objects[hit.objIndex];

		const Material& mat = materials[obj.materialIndex];

		// Add emissive contribution at surface hit
		L += T * mat.getEmission();

		glm::vec3 N = hit.worldNormal;

		glm::vec3 V = glm::normalize(-ray.direction);

		// Dielectric / Tarnsmission branch
		if (mat.transmission > 0.0f)
		{
			glm::vec3 I = glm::normalize(ray.direction);

			float eta = hit.frontFace
				? (1.0f / mat.ior)
				: mat.ior;

			float cosI = glm::clamp(glm::dot(-I, N), 0.0f, 1.0f);

			float F0 = (mat.ior - 1.0f) / (mat.ior + 1.0f);

			F0 *= F0;

			// Schlick-style reflection probability
			float reflectProb = glm::mix(F0, 1.0f, pow(1.0f - cosI, 5.0f));

			glm::vec3 R = glm::reflect(I, N);

			glm::vec3 refractDir = glm::refract(I, N, eta);

			bool totalInternalReflection = glm::length2(refractDir) < 1e-8f;

			bool reflect = totalInternalReflection || (RNG::randomFloat(seed) < reflectProb);

			if (reflect)
			{
				ray.direction = glm::normalize(R);

				ray.origin = hit.worldPosition + N * 1e-4f;
			}

			else
			{
				ray.direction = glm::normalize(refractDir);

				ray.origin = hit.worldPosition - N * 1e-4f;


				if (glm::length(mat.absorption) > 0.0f)
				{
					glm::vec3 transmittance = glm::exp(-mat.absorption * hit.hitDistance);

					T *= transmittance;
				}
			}

			continue;
		}

		// Opaque BSDF sampling
		BSDFSample bsdf = sampleBSDF(mat, N, V, hit.tangent, hit.bitangent, seed);

		if (bsdf.pdf <= 1e-8f)
			break;

		// Apply BSDF weighting
		T *= bsdf.weight;

		// Energy clamping to reduce "fireflies"
		float maxComp = glm::max(T.r, glm::max(T.g, T.b));

		if (maxComp > 10.0f)
			T *= 10.0f / maxComp;

		// Reject all invalid paths
		if (!glm::all(glm::isfinite(T)))
			break;

		ray.origin = hit.worldPosition + N * 1e-4f;

		ray.direction = glm::normalize(bsdf.direction);

		// Russian roulette termination
		if (bounce > 3)
		{
			float p = glm::clamp(glm::max(T.r, glm::max(T.g, T.b)), 0.05f, 0.95f);

			if (RNG::randomFloat(seed) > p)
				break;

			T /= p;
		}
	}

	return glm::vec4(L, 1.0f);
}

// Ray–triangle intersection using the Möller–Trumbore algorithm
bool Renderer::intersectTriangle(const Ray& ray, const Triangle& tri, float& t)
{
	static constexpr float EPSILON = 1e-6f;

	glm::vec3 h = glm::cross(ray.direction, tri.edge2);
	float a = glm::dot(tri.edge1, h);

	// Ray is parallel to triangle plane
	if (a > -EPSILON && a < EPSILON)
		return false;

	float f = 1.0f / a;
	glm::vec3 s = ray.origin - tri.v0;

	float u = f * glm::dot(s, h);
	if (u < 0.0f || u > 1.0f)
		return false;

	glm::vec3 q = glm::cross(s, tri.edge1);

	float v = f * glm::dot(ray.direction, q);

	// Outside triangle bounds (v or u+v)
	if (v < 0.0f || u + v > 1.0f)
		return false;

	t = f * glm::dot(tri.edge2, q);

	// Valid only if intersection is in front of ray origin
	return t > EPSILON;
}

// Ray–AABB intersection using the slab method.
float Renderer::intersectAABB_T(const AABB& box, const Ray& ray)
{
	float tMin = 0.0f;
	float tMax = FLT_MAX;

	for (int i = 0; i < 3; i++)
	{
		float d = ray.direction[i];

		// Avoid division by zero for near-parallel rays
		float invD = (fabs(d) < 1e-8f) ? 1e30f : 1.0f / d;

		float t0 = (box.min[i] - ray.origin[i]) * invD;
		float t1 = (box.max[i] - ray.origin[i]) * invD;

		// Ensure correct ordering for negative directions
		if (invD < 0.0f)
			std::swap(t0, t1);

		// Update intersection interval
		tMin = glm::max(tMin, t0);
		tMax = glm::min(tMax, t1);

		// Early exit: no overlap
		if (tMax < tMin)
			return FLT_MAX;
	}

	return tMin;
}

// Traverses a BVH to find the closest ray–triangle intersection.
bool Renderer::traverseBVH(const Mesh& mesh, int nodeIndex, const Ray& ray, float& closestT, int& hitTriangle)
{
	if (nodeIndex < 0)
		return false;

	const BVHNode& node = mesh.bvh[nodeIndex];

	// Early exit if ray misses bounding box
	if (!intersectAABB(node.bounds, ray, closestT))
		return false;

	bool hit = false;

	// Leaf node: test all triangles
	if (node.triCount > 0)
	{
		for (int i = 0; i < node.triCount; i++)
		{
			int triIndex = mesh.triIndices[node.firstTri + i];

			const Triangle& tri = mesh.triangles[triIndex];

			float t;

			if (intersectTriangle(ray, tri, t))
			{
				if (t > 0.0f && t < closestT)
				{
					closestT = t;
					hitTriangle = triIndex;
					hit = true;
				}
			}
		}
	}

	else
	{
		// Internal node: traverse children
		bool leftHit = traverseBVH(mesh, node.left, ray, closestT, hitTriangle);
		bool rightHit = traverseBVH(mesh, node.right, ray, closestT, hitTriangle);

		hit = leftHit || rightHit;
	}

	return hit;
}

// Traverses the Top-Level Acceleration Structure (TLAS) to find the closest ray–object intersection in the scene.
bool Renderer::traverseTLAS(int nodeIndex, const Ray& ray, float& closestT, int& hitObject, int& hitTriangle)
{
	if (nodeIndex < 0)
		return false;

	const TLASNode& node =
		m_activeScene->tlas[nodeIndex];

	// Early exit if ray misses this node's bounding box
	if (!intersectAABB(node.bounds, ray, closestT))
		return false;

	bool hit = false;

	// Leaf node: represents a scene object (instance)
	if (node.isLeaf())
	{
		const Object& obj = m_activeScene->objects[node.objIndex];

		const Mesh& mesh = m_activeScene->meshes[obj.meshIndex];

		// Transform ray into object local space
		Ray localRay = Utils::toLocalRay(ray, obj);

		float localT = FLT_MAX;

		int triIndex = -1;

		// Traverse mesh BVH (BLAS)
		if (traverseBVH(mesh, mesh.bvhRoot, localRay, localT, triIndex))
		{
			// Convert local hit point back to world space
			glm::vec3 localHit = localRay.origin + localRay.direction * localT;

			glm::vec3 worldHit = glm::vec3(obj.transform * glm::vec4(localHit, 1.0f));

			// Convert distance to world-space metric
			float worldT = glm::length(worldHit - ray.origin);

			// Keep closest hit across TLAS traversal
			if (worldT < closestT)
			{
				closestT = worldT;
				hitObject = node.objIndex;
				hitTriangle = triIndex;

				return true;
			}
		}

		return false;
	}

	// Internal node: traverse children in order of proximity
	int left = node.left;
	int right = node.right;

	float tL = intersectAABB_T(m_activeScene->tlas[left].bounds, ray);

	float tR = intersectAABB_T(m_activeScene->tlas[right].bounds, ray);

	// Visit closer child first for better early termination
	if (tL < tR)
	{
		hit |= traverseTLAS(
			left,
			ray,
			closestT,
			hitObject,
			hitTriangle);

		hit |= traverseTLAS(
			right,
			ray,
			closestT,
			hitObject,
			hitTriangle);
	}

	else
	{
		hit |= traverseTLAS(
			right,
			ray,
			closestT,
			hitObject,
			hitTriangle);

		hit |= traverseTLAS(
			left,
			ray,
			closestT,
			hitObject,
			hitTriangle);
	}

	return hit;
}

// Ray–AABB intersection test using the slab method.
bool Renderer::intersectAABB(const AABB& box, const Ray& ray, float maxDistance)
{
	float tMin = 0.0f;
	float tMax = maxDistance;

	for (int i = 0; i < 3; i++)
	{
		float d = ray.direction[i];

		// Avoid division by zero for near-parallel rays
		float invD = (fabs(d) < 1e-8f) ? 1e30f : 1.0f / d;

		float t0 = (box.min[i] - ray.origin[i]) * invD;

		float t1 = (box.max[i] - ray.origin[i]) * invD;

		// Ensure correct ordering for negative directions
		if (invD < 0.0f)
			std::swap(t0, t1);

		// Narrow intersection interval
		tMin = glm::max(tMin, t0);
		tMax = glm::min(tMax, t1);

		// Early exit: no valid overlap
		if (tMax < tMin)
			return false;
	}

	return true;
}

// Procedurally samples sky color using atmospheric gradient and sun lighting.
glm::vec3 Renderer::sampleSky(const glm::vec3& direction)
{
	glm::vec3 sky(1.0f);
	glm::vec3 dirNormal = glm::normalize(direction);
	glm::vec3 sunDirNormal = glm::normalize(m_sunDirection);

	// Convert vertical direction into a 0–1 gradient factor
	float t = glm::clamp(dirNormal.y * 0.5f + 0.5f, 0.0f, 1.0f);

	glm::vec3 color = glm::mix(m_horizonColor, m_zenithColor, t);

	// Add extra atmospheric scattering near the horizon
	float horizon = glm::pow(1.0f - std::abs(dirNormal.y), m_horizonFalloff);

	color += m_horizonColor * horizon * m_horizonIntensity;

	// Measure alignment between view direction and sun direction
	float sunAmount = glm::max(glm::dot(dirNormal, sunDirNormal), 0.0f);

	// Large soft halo surrounding the sun
	float sunGlow = glm::pow(sunAmount, m_sunGlowSize);

	// Small intense core representing the sun disc
	float sunDisc = glm::pow(sunAmount, m_sunDiscSize);

	color += m_sunGlowColor * sunGlow * m_sunGlowIntensity;

	color += m_sunColor * sunDisc * m_sunIntensity;

	return color * m_skyIntensity;
}

// Samples a Lambertian diffuse BRDF using cosine-weighted hemisphere sampling.
Renderer::BSDFSample Renderer::sampleDiffuse(const Material& mat, const glm::vec3& N, const glm::vec3& V, const glm::vec3& T, const glm::vec3& B, uint32_t& seed)
{
	float u1 = RNG::randomFloat(seed);
	float u2 = RNG::randomFloat(seed);

	// Generate cosine-weighted direction in tangent space
	glm::vec3 localDir = Utils::sampleCosineHemisphere(u1, u2);

	// Transform sampled direction into world space
	glm::vec3 L = Utils::toWorld(localDir, T, B, N);

	float NdotL = glm::max(glm::dot(N, L), 0.0f);

	// PDF for cosine-weighted hemisphere sampling
	float pdf = NdotL / glm::pi<float>();

	// Base reflectivity at normal incidence
	glm::vec3 F0 = glm::mix(glm::vec3(0.04f), mat.albedo, mat.metallic);

	float NdotV = glm::max(glm::dot(N, V), 0.0f);

	// Fresnel term reduces diffuse contribution at grazing angles
	glm::vec3 F = Utils::fresnelSchlick(NdotV, F0);
	
	// Diffuse energy conservation term
	glm::vec3 kd = (glm::vec3(1.0f) - F) * (1.0f - mat.metallic);

	// Lambertian BRDF
	glm::vec3 f = kd * mat.albedo / glm::pi<float>();

	BSDFSample s;
	s.direction = L;
	s.pdf = pdf;
	s.weight = f * NdotL / pdf; // Throughput contribution for this sample
	s.specular = false;

	return s;
}

// Samples a GGX microfacet specular BRDF with anisotropic roughness support
Renderer::BSDFSample Renderer::sampleGGX(const Material& mat, const glm::vec3& N, const glm::vec3& V, const glm::vec3& T, const glm::vec3& B, uint32_t& seed)
{
	// Prevent perfectly smooth surfaces from producing unstable results
	float roughness = glm::max(mat.roughness, 0.02f);

	// Convert anisotropy into separate X/Y roughness scaling
	float aspect = sqrt(1.0f - mat.anisotropy * 0.9f);

	float ax = glm::max(0.001f, (roughness * roughness) / aspect);

	float ay = glm::max(0.001f, (roughness * roughness) * aspect);

	// Sample GGX half-vector in tangent space
	glm::vec3 H = Utils::sampleAnistrophicGGX(
			N,
			T,
			B,
			ax,
			ay,
			seed);

	float VdotH = glm::dot(V, H);

	// Reject invalid backfacing half-vectors
	if (VdotH <= 0.0f)
		return {};

	glm::vec3 L = glm::reflect(-V, H);

	L = glm::normalize(L);

	float NdotL = glm::max(glm::dot(N, L), 0.0f);

	float NdotV = glm::max(glm::dot(N, V), 0.0f);

	float NdotH = glm::max(glm::dot(N, H), 0.0f);

	float HdotV = glm::max(glm::dot(H, V), 0.0f);

	// Reject invalid geometric configurations
	if (NdotL <= 0.0f ||
		NdotV <= 0.0f ||
		NdotH <= 0.0f ||
		HdotV <= 0.0f)
	{
		return {};
	}

	// Approximate isotropic alpha value for BRDF evaluation
	float alpha = 0.5f * (ax + ay);

	// Base reflectivity at normal incidence
	glm::vec3 F0 = glm::mix(glm::vec3(0.04f), mat.albedo, mat.metallic);

	// Fresnel term
	glm::vec3 F = Utils::fresnelSchlick(HdotV, F0);

	// GGX normal distribution function
	float D = Utils::D_GGX(NdotH, alpha);

	// Smith masking-shadowing term
	float G = Utils::G_Smith(NdotV, NdotL, alpha);

	glm::vec3 f = Utils::evaluateSpecular(
			F,
			D,
			G,
			NdotL,
			NdotV);

	// PDF for GGX half-vector sampling
	float pdf = Utils::pdfGGX(D, NdotH, HdotV);

	if (pdf <= 1e-8f)
		return {};

	BSDFSample s;

	s.direction = L;
	s.f = f;
	s.pdf = pdf;
	s.weight = f * NdotL / pdf; // Final throughput contribution for this sample
	s.specular = true;

	return s;
}

// Samples a combined diffuse + GGX specular BRDF using multiple importance sampling
Renderer::BSDFSample Renderer::sampleBSDF(const Material& mat, const glm::vec3& N, const glm::vec3& V, const glm::vec3& T, const glm::vec3& B, uint32_t& seed)
{
	glm::vec3 F0 = glm::mix(glm::vec3(0.04f), mat.albedo, mat.metallic);

	// Bias sampling toward specular surfaces with higher reflectance
	float specChance = glm::clamp(glm::max(F0.r, glm::max(F0.g, F0.b)), 0.05f, 0.95f);

	float diffChance = 1.0f - specChance;

	bool sampleSpec = RNG::randomFloat(seed) < specChance;

	// Sample either diffuse or specular lobe
	BSDFSample s = sampleSpec
		? sampleGGX(mat, N, V, T, B, seed)
		: sampleDiffuse(mat, N, V, T, B, seed);

	if (s.pdf <= 0.0f)
		return {};

	float pdfSpec = 0.0f;
	float pdfDiff = 0.0f;

	glm::vec3 L = s.direction;

	float NdotL = glm::max(glm::dot(N, L), 0.0f);

	if (NdotL <= 0.0f)
		return {};

	// Diffuse cosine-weighted PDF
	pdfDiff = NdotL / glm::pi<float>();

	glm::vec3 H = glm::normalize(V + L);

	float NdotH = glm::max(glm::dot(N, H), 0.0f);

	float HdotV = glm::max(glm::dot(H, V), 0.0f);

	float alpha = glm::max(mat.roughness * mat.roughness, 0.001f);

	float D = Utils::D_GGX(NdotH, alpha);

	// Specular GGX sampling PDF
	pdfSpec = Utils::pdfGGX(D, NdotH, HdotV);

	// Combine PDFs from both sampling strategies
	float pdf = specChance * pdfSpec + diffChance * pdfDiff;

	if (pdf <= 1e-8f)
		return {};

	glm::vec3 F = Utils::fresnelSchlick(HdotV, F0);

	// Diffuse energy conservation term
	glm::vec3 kd = (glm::vec3(1.0f) - F) * (1.0f - mat.metallic);

	glm::vec3 diffuse = kd * mat.albedo / glm::pi<float>();

	float NdotV = glm::max(glm::dot(N, V), 0.0f);

	float G = Utils::G_Smith(NdotV, NdotL, alpha);

	glm::vec3 specular = Utils::evaluateSpecular(
			F,
			D,
			G,
			NdotL,
			NdotV);

	
	s.f = diffuse + specular;		// Evaluate full BRDF regardless of sampled lobe
	s.pdf = pdf;
	s.weight = s.f * NdotL / pdf;	// Final MIS-weighted throughput contribution

	return s;
}

// Recalculates the Transform Matrix of each object present in the scene and then rebuilds the TLAS
void Renderer::recalculateScene(Scene& scene)
{
	for (auto& obj : scene.objects)
	{
		obj.recalculateMatrixes();
	}
	
	buildTLAS(scene);
}

// Traces a ray through the scene acceleration structure and returns hit information
Renderer::HitPayload Renderer::traceRay(const Ray& ray)
{
	int hitObject = -1;
	int hitTriangle = -1;

	float closestT = FLT_MAX;

	// Traverse the top-level BVH and track the closest intersection
	traverseTLAS(
		m_activeScene->tlasRoot,
		ray,
		closestT,
		hitObject,
		hitTriangle);

	if (hitObject < 0)
		return miss(ray);

	// Build full shading payload for the closest hit
	return closestHit(
		ray,
		closestT,
		hitObject,
		hitTriangle);
}

// Builds the final shading payload for the closest ray-triangle intersection
Renderer::HitPayload Renderer::closestHit(const Ray& ray, float hitDistance, int objIndex, int triIndex)
{
	HitPayload payload;

	const Object& obj = m_activeScene->objects[objIndex];

	const Material& mat = m_activeScene->materials[obj.materialIndex];

	const Mesh& mesh = m_activeScene->meshes[obj.meshIndex];

	const Triangle& tri = mesh.triangles[triIndex];

	payload.hitDistance = hitDistance;
	payload.objIndex = objIndex;
	payload.triangleIndex = triIndex;

	// Compute world-space hit position
	payload.worldPosition = ray.origin + ray.direction * hitDistance;

	// Build geometric normal from triangle edges
	glm::vec3 localNormal = glm::normalize(glm::cross(tri.edge1, tri.edge2));

	// Transform normal into world space
	payload.worldNormal = glm::normalize(obj.normalMatrix * localNormal);

	// Use first triangle edge as tangent basis direction
	glm::vec3 localTangent = glm::normalize(tri.edge1);

	glm::vec3 worldTangent = glm::normalize(glm::mat3(obj.transform) * localTangent);

	// Re-orthogonalize tangent against the normal
	worldTangent =glm::normalize(worldTangent - payload.worldNormal * glm::dot(payload.worldNormal, worldTangent));

	glm::vec3 worldBitangent = glm::normalize(glm::cross(payload.worldNormal, worldTangent));

	float rot = mat.anisotropyRotation;

	// Rotate tangent frame for anisotropic materials
	if (rot != 0.0f)
	{
		Renderer::rotateTangentFrame(
			worldTangent,
			worldBitangent,
			payload.worldNormal,
			rot);
	}

	payload.tangent = worldTangent;
	payload.bitangent = worldBitangent;

	float ndotd = glm::dot(ray.direction, payload.worldNormal);

	// Determine whether the ray hit the front or back face
	payload.frontFace = ndotd < 0.0f;

	// Flip normals for backface hits to keep shading consistent
	if (!payload.frontFace)
		payload.worldNormal = -payload.worldNormal;

	return payload;
}

// Creates a HitPayload with no information, as no geometry was hit
Renderer::HitPayload Renderer::miss(const Ray& ray)
{
	Renderer::HitPayload payload;
	payload.hitDistance = -1;
	return payload;
}

// Builds an orthonormal tangent frame from a surface normal
void Renderer::buildOrthonormalBasis(const glm::vec3& normal, glm::vec3& tangent, glm::vec3& bitangent)
{
	// Use world Z axis unless the normal is nearly parallel to it
	if (fabs(normal.z) < 0.999f)
	{
		tangent = glm::normalize(glm::cross(normal, glm::vec3(0, 0, 1)));
	}

	// Fallback axis avoids degenerate cross products near poles
	else
	{
		tangent = glm::normalize(glm::cross(normal, glm::vec3(0, 1, 0)));
	}

	// Complete the orthonormal basis
	bitangent = glm::normalize(glm::cross(normal, tangent));
}

// Recursively builds a TLAS BVH node using centroid median splitting
int Renderer::buildTLASNode(Scene& scene, int start, int count)
{
	TLASNode node;

	// Compute bounds for all primitives contained in this node
	node.bounds = Utils::computeTLASBounds(scene, start, count);

	int nodeIndex = (int)scene.tlas.size();

	scene.tlas.push_back(node);

	// Leaf node: store object reference directly
	if (count == 1)
	{
		int primIndex = scene.tlasIndices[start];

		scene.tlas[nodeIndex].objIndex = scene.tlasPrimitives[primIndex].objIndex;

		return nodeIndex;
	}

	AABB centroidBounds = createEmptyAABB();

	// Compute bounds of primitive centroids for split selection
	for (int i = 0; i < count; i++)
	{
		int primIndex = scene.tlasIndices[start + i];

		const glm::vec3& c = scene.tlasPrimitives[primIndex].centroid;

		expandAABB(centroidBounds, c);
	}

	glm::vec3 extent = centroidBounds.max - centroidBounds.min;

	// Split along the axis with the largest spatial extent
	int axis =
		(extent.x > extent.y && extent.x > extent.z) ? 0 :
		(extent.y > extent.z) ? 1 : 2;

	int mid = start + count / 2;

	// Partition primitives around the median centroid
	std::nth_element(
		scene.tlasIndices.begin() + start,
		scene.tlasIndices.begin() + mid,
		scene.tlasIndices.begin() + start + count,

		[&](int a, int b)
		{
			return
				scene.tlasPrimitives[a].centroid[axis] <
				scene.tlasPrimitives[b].centroid[axis];
		});

	// Recursively build child nodes
	int left = buildTLASNode(scene, start, mid - start);

	int right = buildTLASNode(scene, mid, count - (mid - start));

	scene.tlas[nodeIndex].left = left;
	scene.tlas[nodeIndex].right = right;

	return nodeIndex;
}

// Builds the top-level acceleration structure (TLAS) for the current scene
void Renderer::buildTLAS(Scene& scene)
{
	// Reset previous TLAS data
	scene.tlas.clear();
	scene.tlasPrimitives.clear();
	scene.tlasIndices.clear();
	scene.tlasRoot = -1;

	// Early exit for empty scenes
	if (scene.objects.empty())
		return;

	// Compute per-object bounds used for TLAS construction
	scene.updateObjectBounds();

	// Convert objects into BVH primitives (centroid + bounds)
	scene.buildTLASPrimitives();

	// Initialize index buffer for BVH partitioning
	scene.tlasIndices.resize(scene.tlasPrimitives.size());

	for (size_t i = 0; i < scene.tlasPrimitives.size(); i++)
		scene.tlasIndices[i] = (int)i;

	// Recursively build BVH from all primitives
	scene.tlasRoot = buildTLASNode(scene, 0, (int)scene.tlasPrimitives.size());
}

// Rotates a tangent-space basis around the normal axis (used for anisotropy)
void Renderer::rotateTangentFrame(glm::vec3& T, glm::vec3& B, const glm::vec3& N, float angle)
{
	// 2D rotation matrix applied in tangent-bitangent plane
	float c = cos(angle);
	float s = sin(angle);

	glm::vec3 newT = T * c + B * s;
	glm::vec3 newB = -T * s + B * c;

	// Re-normalize to prevent drift from accumulated floating-point error
	T = glm::normalize(newT);
	B = glm::normalize(newB);
}