// Engine
#include <model.h>

// TinyOBJLoader
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

// Loads an OBJ model using tinyobjloader and builds a triangle mesh + BVH
Mesh loadModel(const std::string& filePath)
{
	// Raw data containers filled by tinyobjloader
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	// Parse OBJ file
	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filePath.c_str()))
	{
		throw std::runtime_error(warn + err);
	}

	// Temporary vertex format used for deduplication
	struct Vertex
	{
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec2 uv;

		// Needed for unordered_map key comparison
		bool operator==(const Vertex& other) const
		{
			return pos == other.pos &&
				normal == other.normal &&
				uv == other.uv;
		}
	};

	// Hash function for Vertex so it can be used in unordered_map
	struct VertexHash
	{
		size_t operator()(const Vertex& v) const
		{
			// Combine component hashes (not perfect but typical for graphics code)
			auto h1 = std::hash<float>()(v.pos.x) ^ (std::hash<float>()(v.pos.y) << 1);
			auto h2 = std::hash<float>()(v.pos.z) ^ (std::hash<float>()(v.pos.x) << 1);
			auto h3 = std::hash<float>()(v.uv.x) ^ (std::hash<float>()(v.uv.y) << 1);
			auto h4 = std::hash<float>()(v.normal.x) ^ (std::hash<float>()(v.normal.y) << 1);

			return h1 ^ h2 ^ h3 ^ h4;
		}
	};

	// Deduplicated vertex buffer + index buffer
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	// Maps unique vertex -> index in final vertex array
	std::unordered_map<Vertex, uint32_t, VertexHash> uniqueVertices;

	// Iterate over all shapes in the OBJ file
	for (const auto& shape : shapes)
	{
		// Iterate over all vertex indices (each refers to a corner of a triangle)
		for (const auto& index : shape.mesh.indices)
		{
			Vertex vertex{};

			// Load position from attribute buffer
			vertex.pos = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			};

			// Load normal if available
			if (index.normal_index >= 0)
			{
				vertex.normal = {
					attrib.normals[3 * index.normal_index + 0],
					attrib.normals[3 * index.normal_index + 1],
					attrib.normals[3 * index.normal_index + 2]
				};
			}

			// Load UV if available
			if (index.texcoord_index >= 0)
			{
				vertex.uv = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					attrib.texcoords[2 * index.texcoord_index + 1]
				};
			}

			// Deduplicate vertex: reuse index if already seen
			if (uniqueVertices.count(vertex) == 0)
			{
				uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
				vertices.push_back(vertex);
			}

			// Append index (either existing or newly created vertex index)
			indices.push_back(uniqueVertices[vertex]);
		}
	}

	// Final mesh object to return
	Mesh mesh;

	// Extract only positions into final vertex buffer
	for (const auto& v : vertices)
	{
		mesh.vertices.push_back(v.pos);
	}

	// Index buffer defining triangles
	mesh.indices = indices;

	// Reserve space for derived triangle data structures
	mesh.triangles.reserve(mesh.indices.size() / 3);
	mesh.centroids.reserve(mesh.indices.size() / 3);
	mesh.triIndices.reserve(mesh.indices.size() / 3);

	// Build derived geometry data (triangles + centroids)
	buildTriangles(mesh);

	// Build bottom-level acceleration structure (BVH over triangles)
	buildBVH(mesh);

	return mesh;
}

// Expands indexed mesh data into explicit triangle primitives
// and precomputes per-triangle data used for intersection + BVH building
void buildTriangles(Mesh& mesh)
{
	// Reserve memory for all triangles (3 indices per triangle)
	mesh.triangles.reserve(mesh.indices.size() / 3);

	// Iterate over index buffer in groups of 3 (each triangle)
	for (size_t i = 0; i < mesh.indices.size(); i += 3)
	{
		Triangle temp;

		// Fetch triangle vertices using index buffer
		temp.v0 = mesh.vertices[mesh.indices[i + 0]];
		temp.v1 = mesh.vertices[mesh.indices[i + 1]];
		temp.v2 = mesh.vertices[mesh.indices[i + 2]];

		// Precompute edge vectors for faster ray-triangle intersection
		temp.edge1 = temp.v1 - temp.v0;
		temp.edge2 = temp.v2 - temp.v0;

		// Store triangle in mesh
		mesh.triangles.push_back(temp);
	}
}

// Builds a bottom-level BVH (Bounding Volume Hierarchy) for a mesh
// This accelerates ray-triangle intersection by grouping triangles spatially
void buildBVH(Mesh& mesh)
{
	// Reset any previous BVH data
	mesh.bvh.clear();

	// Nothing to build if mesh has no triangles
	if (mesh.triangles.empty())
		return;

	// Pre-allocate memory to reduce reallocations during recursive build
	mesh.bvh.reserve(mesh.triangles.size() * 2);

	// Compute triangle centroids (used for splitting heuristic)
	buildCentroids(mesh);

	// Initialize triangle index buffer (used for BVH partitioning)
	mesh.triIndices.resize(mesh.triangles.size());

	for (size_t i = 0; i < mesh.triangles.size(); i++)
		mesh.triIndices[i] = (int)i;

	// Recursively build BVH over full triangle range
	mesh.bvhRoot = buildBVHNode(mesh, 0, (int)mesh.triangles.size());
}

// Computes the axis-aligned bounding box (AABB) for a range of triangles
// Used during BVH construction to determine spatial bounds of a node
AABB computeBounds(const Mesh& mesh, int start, int count)
{
	// Start with an "empty" AABB that will expand as points are added
	AABB box = createEmptyAABB();

	// Pointer to triangle index buffer for fast access
	const int* triIdx = mesh.triIndices.data();

	// Iterate over the subset of triangles assigned to this BVH node
	for (int i = 0; i < count; i++)
	{
		// Fetch triangle via indirection through triIndices
		const Triangle& tri = mesh.triangles[triIdx[start + i]];

		// Expand bounding box to include all triangle vertices
		expandAABB(box, tri.v0);
		expandAABB(box, tri.v1);
		expandAABB(box, tri.v2);
	}

	return box;
}

// Recursively builds a BVH node over a range of triangles
// Uses centroid-based spatial splitting (median split heuristic)
int buildBVHNode(Mesh& mesh, int start, int count)
{
	// Create BVH node for this range
	BVHNode node;

	// Leaf range: triangles covered by this node
	node.firstTri = start;
	node.triCount = count;

	// Compute world-space bounding box for all triangles in range
	node.bounds = computeBounds(mesh, start, count);

	// Store node in BVH array and get its index
	int nodeIndex = (int)mesh.bvh.size();
	mesh.bvh.push_back(node);

	// Leaf condition: stop splitting when few triangles remain
	if (count <= 2)
		return nodeIndex;

	// Compute bounding box of triangle centroids (for split decision)
	AABB centroidBounds = createEmptyAABB();

	for (int i = 0; i < count; i++)
	{
		int triIndex = mesh.triIndices[start + i];
		expandAABB(centroidBounds, mesh.centroids[triIndex]);
	}

	// Determine axis with largest spatial extent (heuristic split axis)
	glm::vec3 extent = centroidBounds.max - centroidBounds.min;

	int axis =
		(extent.x > extent.y && extent.x > extent.z) ? 0 :
		(extent.y > extent.z) ? 1 : 2;

	// Split index (median split)
	int mid = start + count / 2;

	// Partition triangles around median centroid along chosen axis
	std::nth_element(
		mesh.triIndices.begin() + start,
		mesh.triIndices.begin() + mid,
		mesh.triIndices.begin() + start + count,
		[&](int a, int b)
		{
			return mesh.centroids[a][axis] < mesh.centroids[b][axis];
		}
	);

	// Compute left/right subtree sizes
	int leftCount = mid - start;
	int rightCount = count - leftCount;

	// Recursively build child nodes
	mesh.bvh[nodeIndex].left = buildBVHNode(mesh, start, leftCount);
	mesh.bvh[nodeIndex].right = buildBVHNode(mesh, mid, rightCount);

	// Mark as internal node (no direct triangle storage)
	mesh.bvh[nodeIndex].triCount = 0;

	return nodeIndex;
}

// Computes triangle centroids for BVH construction
void buildCentroids(Mesh& mesh)
{
	mesh.centroids.resize(mesh.triangles.size());

	for (size_t i = 0; i < mesh.triangles.size(); i++)
	{
		const auto& t = mesh.triangles[i];
		
		// Centroid = average of triangle vertices
		mesh.centroids[i] = (t.v0 + t.v1 + t.v2) * (1.0f / 3.0f);
	}
}

// Returns the geometric center of an AABB
glm::vec3 getAABBcenter(const AABB& box)
{
	// Center = midpoint between min and max corners
	return (box.min + box.max) * 0.5f;
}

// Returns the size (width/height/depth) of the AABB
glm::vec3 getAABBExtent(const AABB& box)
{
	// Extent = diagonal vector from min to max
	return box.max - box.min;
}

// Expands destination AABB to fully contain another AABB
void expandAABB(AABB& dst, const AABB& src)
{
	// Take component-wise min/max to grow the box
	dst.min = glm::min(dst.min, src.min);
	dst.max = glm::max(dst.max, src.max);
}

// Expands AABB to include a single point
void expandAABB(AABB& box, const glm::vec3& point)
{
	// Extend bounds so the point lies inside the box
	box.min = glm::min(box.min, point);
	box.max = glm::max(box.max, point);
}

// Transforms an AABB by a matrix by transforming all 8 corners
// and recomputing a new axis-aligned bounding box that encloses them
AABB transformAABB(const AABB& box, const glm::mat4& transform)
{
	// Define the 8 corner points of the original AABB
	glm::vec3 corners[8] =
	{
		{ box.min.x, box.min.y, box.min.z },
		{ box.max.x, box.min.y, box.min.z },
		{ box.min.x, box.max.y, box.min.z },
		{ box.max.x, box.max.y, box.min.z },

		{ box.min.x, box.min.y, box.max.z },
		{ box.max.x, box.min.y, box.max.z },
		{ box.min.x, box.max.y, box.max.z },
		{ box.max.x, box.max.y, box.max.z }
	};

	// Start with an empty bounding box
	AABB result = createEmptyAABB();

	// Transform each corner and expand the result AABB
	for (int i = 0; i < 8; i++)
	{
		// Apply full 4x4 transform (including translation)
		glm::vec3 transformed = glm::vec3(transform * glm::vec4(corners[i], 1.0f));

		// Expand bounding box to include transformed point
		expandAABB(result, transformed);
	}

	return result;
}

// Creates an "invalid" AABB that can be safely expanded into
// Used as the starting point for bounding box construction
AABB createEmptyAABB()
{
	AABB box;

	// Initialize to extreme values so any real point will shrink/expand it correctly
	box.min = glm::vec3(FLT_MAX);
	box.max = glm::vec3(-FLT_MAX);

	return box;
}

// Computes surface area of an AABB
float getAABBSurfaceArea(const AABB& box)
{
	// Extent (width, height, depth) of the box
	glm::vec3 e = box.max - box.min;

	// Surface area of a rectangular prism:
	// 2(xy + yz + zx)
	return 2.0f * (
		e.x * e.y +
		e.y * e.z +
		e.z * e.x
		);
}

// Returns the axis (0 = X, 1 = Y, 2 = Z) with the largest extent
int getAABBLongestAxis(const AABB& box)
{
	glm::vec3 extent = getAABBExtent(box);

	if (extent.x > extent.y && extent.x > extent.z)
		return 0;

	if (extent.y > extent.z)
		return 1;

	return 2;
}

// Checks if an AABB is valid (min <= max on all axes)
bool isValidAABB(const AABB& box)
{
	return
		box.min.x <= box.max.x &&
		box.min.y <= box.max.y &&
		box.min.z <= box.max.z;
}

// Checks whether a point lies inside the AABB (inclusive bounds)
bool constainsPoint(const AABB& box, const glm::vec3& p)
{
	return
		p.x >= box.min.x && p.x <= box.max.x &&
		p.y >= box.min.y && p.y <= box.max.y &&
		p.z >= box.min.z && p.z <= box.max.z;
}

// Checks whether two AABBs overlap (intersection test)
// Returns true if boxes intersect on all three axes
bool overlapsAABB(const AABB& a, const AABB& b)
{
	return
		a.min.x <= b.max.x && a.max.x >= b.min.x &&
		a.min.y <= b.max.y && a.max.y >= b.min.y &&
		a.min.z <= b.max.z && a.max.z >= b.min.z;
}
