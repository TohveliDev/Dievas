#pragma once

// Engine
#include <model.h>
#include <material.h>

// C++
#include <vector>

// GLM
#include <glm/glm.hpp>

struct Scene
{
	std::vector<Mesh> meshes;
	std::vector<Object> objects;
	std::vector<Material> materials;

	std::vector<TLASNode> tlas;
	std::vector<int> tlasIndices;
	std::vector<TLASPrimitive> tlasPrimitives;
	int tlasRoot = -1;

	void addModelToScene(const std::string& path, int materialIndex, const glm::mat4& transform)
	{
		Mesh mesh = loadModel(path);

		meshes.push_back(mesh);
		int meshIndex = static_cast<int>(meshes.size()) - 1;

		Object obj;
		obj.meshIndex = meshIndex;
		obj.materialIndex = materialIndex;
		obj.transform = transform;

		objects.push_back(obj);
	}

	void updateObjectBounds()
	{
		for (size_t i = 0; i < objects.size(); i++)
		{
			Object& obj = objects[i];

			const Mesh& mesh = meshes[obj.meshIndex];

			const AABB& localBounds =
				mesh.bvh[mesh.bvhRoot].bounds;

			obj.worldBounds =
				transformAABB(localBounds, obj.transform);
		}
	}

	void buildTLASPrimitives()
	{
		tlasPrimitives.clear();

		for (size_t i = 0; i < objects.size(); i++)
		{
			TLASPrimitive prim;

			prim.bounds = objects[i].worldBounds;

			prim.centroid =
				getAABBcenter(prim.bounds);

			prim.objIndex = (int)i;

			tlasPrimitives.push_back(prim);
		}
	}
};