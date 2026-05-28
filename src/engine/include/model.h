#pragma once

// C++
#include <vector>
#include <unordered_map>
#include <string>
#include <algorithm>

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Axis-aligned bounding box used for spatial acceleration structures
struct AABB
{
    glm::vec3 min;              // minimum corner of the box
    glm::vec3 max;              // maximum corner of the box
};

// Triangle primitive used in acceleration structures
struct Triangle
{
    glm::vec3 v0, v1, v2;       // triangle vertices in local/object space
    glm::vec3 edge1, edge2;     // Precomputed edges for faster intersection tests
    AABB bounds;                // Local-space bounding box of the triangle
};

// Node in a bottom-level BVH (BVH over a single mesh)
struct BVHNode
{
    AABB bounds;                // bounding box enclosing this node's triangles
    int left = -1;              // index of left child (-1 if none)
    int right = -1;             // index of right child (-1 if none)

    // Leaf data: range of triangles stored in this node
    int firstTri = 0;
    int triCount = 0;
};


// Node in the top-level acceleration structure (TLAS over objects)
struct TLASNode
{
    AABB bounds;                        // bounds of the object(s) under this node

    int left = -1;                      // left child node index
    int right = -1;                     // right child node index

    int objIndex = -1;                  // valid only for leaf nodes (points to object)

    // Returns true if this node is a leaf (contains a single object)
    bool isLeaf() const
    {
        return objIndex >= 0;
    }
};

// Primitive used for building the TLAS (object-level bounding data)
struct TLASPrimitive
{
    AABB bounds;                        // world-space bounds of the object
    glm::vec3 centroid;                 // center point used for BVH splitting
    int objIndex;                       // index into object array
};

// Mesh data + bottom-level BVH (BLAS)
struct Mesh
{
    std::vector<glm::vec3> vertices;    // vertex positions
    std::vector<uint32_t> indices;      // triangle index buffer
    std::vector<glm::vec3> centroids;   // triangle centroids (for BVH build)
    std::vector<Triangle> triangles;    // expanded triangle data
    std::vector<int> triIndices;        // index remapping used during BVH build
    std::vector<BVHNode> bvh;           // BVH node storage
    int bvhRoot = -1;                   // root node index of BVH
};


// Scene object with transform + mesh/material references
struct Object
{
    std::string name = "New Object";

    // Local transform components
    glm::vec3 position{ 0.f };
    glm::quat rotation = glm::quat(1.0f, 0.f, 0.f, 0.f);
    glm::vec3 scale{ 1.0f };

    // Cached transformation matrices
    glm::mat4 transform{ 1.0f };        // local-to-world
    glm::mat4 invTransform{ 1.0f };     // world-to-local
    glm::mat3 normalMatrix{ 1.0f };     // correct normals under non-uniform scale

    AABB worldBounds;                   // object-space BVH bounds transformed into world space

    int materialIndex = 0;              // material reference
    int meshIndex = 0;                  // mesh reference

    // Recomputes transform matrices after position/rotation/scale changes
    void recalculateMatrixes()
    {
        // Prevent invalid scale values (avoid zero or negative scaling issues)
        scale = glm::max(scale, glm::vec3(0.001f));

        // Ensure quaternion stays normalized for stable rotations
        rotation = glm::normalize(rotation);

        // Build TRS matrix (Translate * Rotate * Scale)
        transform =
            glm::translate(glm::mat4(1.0f), position) *
            glm::mat4_cast(rotation) *
            glm::scale(glm::mat4(1.0f), scale);

        // Precompute inverse for raycasting / object-space transforms
        invTransform = glm::inverse(transform);

        // Normal matrix ensures correct normal transformation under non-uniform scale
        normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));
    }
};

Mesh loadModel(const std::string& filePath);
int buildBVHNode(Mesh& mesh, int start, int count);
void buildTriangles(Mesh& mesh);
void buildBVH(Mesh& mesh);
AABB computeBounds(const Mesh& mesh, int start, int count);
void buildCentroids(Mesh& mesh);
glm::vec3 getAABBcenter(const AABB& box);
glm::vec3 getAABBExtent(const AABB& box);
void expandAABB(AABB& dst, const AABB& src);
void expandAABB(AABB& box, const glm::vec3& point);
AABB transformAABB(const AABB& box, const glm::mat4& transform);
AABB createEmptyAABB();
float getAABBSurfaceArea(const AABB& box);
int getAABBLongestAxis(const AABB& box);
bool isValidAABB(const AABB& box);
bool constainsPoint(const AABB& box, const glm::vec3& p);
bool overlapsAABB(const AABB& a, const AABB& b);