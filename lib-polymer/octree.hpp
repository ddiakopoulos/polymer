// This is free and unencumbered software released into the public domain.

#ifndef scene_octree_hpp
#define scene_octree_hpp

#include "math-core.hpp"
#include "gl-api.hpp"
#include "util.hpp"
#include "algo_misc.hpp"

#include <list>
#include <memory>

using namespace polymer;

/*
 * An octree is a tree data structure in which each internal node has exactly
 * eight children. Octrees are most often used to partition a three
 * dimensional space by recursively subdividing it into eight octants.
 * This implementation stores 8 pointers per node, instead of the other common
 * approach, which is to use a flat array with an offset. The `inside` method
 * defines the comparison function (loose in this case). The main usage of this
 * class is for basic frustum culling.
 */

// Instead of a strict bounds check which might force an object into a parent cell, this function
// checks centers, aka a "loose" octree. 
inline bool inside(const Bounds3D & node, const Bounds3D & other)
{
    // Compare centers
    if (!(linalg::all(greater(other.max(), node.center())) && linalg::all(less(other.min(), node.center())))) return false;

    // Otherwise ensure we shouldn't move to parent
    return linalg::all(less(node.size(), other.size()));
}

// Forward declare
template<typename T>
struct Octant;

template<typename T>
struct SceneNodeContainer
{
    T & object;
    Octant<T> * octant{ nullptr };
    Bounds3D worldspaceBounds;
    SceneNodeContainer(T & obj, const Bounds3D & bounds) : object(obj), worldspaceBounds(bounds) {}
    bool operator== (const SceneNodeContainer<T> & other) { return &object == &other.object; }
};

template<typename T>
struct Octant
{
    std::list<SceneNodeContainer<T>> objects;

    Octant<T> * parent;
    Octant(Octant<T> * parent) : parent(parent) {}

    Bounds3D box;
    VoxelArray<std::unique_ptr<Octant<T>>> arr = { { 2, 2, 2 } };
    uint32_t occupancy{ 0 };

    int3 get_indices(const Bounds3D & other) const
    {
        const float3 a = other.center();
        const float3 b = box.center();
        int3 index;
        index.x = (a.x > b.x) ? 1 : 0;
        index.y = (a.y > b.y) ? 1 : 0;
        index.z = (a.z > b.z) ? 1 : 0;
        return index;
    }

    void increase_occupancy(Octant<T> * n) const
    {
        if (n != nullptr)
        {
            n->occupancy++;
            increase_occupancy(n->parent);
        }
    }

    void decrease_occupancy(Octant<T> * n) const
    {
        if (n != nullptr)
        {
            n->occupancy--;
            decrease_occupancy(n->parent);
        }
    }

    // Returns true if the other is less than half the size of myself
    bool check_fit(const Bounds3D & other) const
    {
        return all(lequal(other.size(), box.size() * 0.5f));
    }

};

template<typename T>
struct SceneOctree
{
    enum CullStatus
    {
        INSIDE,
        INTERSECT,
        OUTSIDE
    };

    std::unique_ptr<Octant<T>> root;
    uint32_t maxDepth{ 8 };

    SceneOctree(const uint32_t maxDepth = 8, const Bounds3D rootBounds = { { -1, -1, -1 },{ +1, +1, +1 } })
    {
        root.reset(new Octant<T>(nullptr));
        root->box = rootBounds;
    }

    ~SceneOctree() { }

    float3 get_resolution()
    {
        return root->box.size() / (float)maxDepth;
    }

    void add(SceneNodeContainer<T> & sceneNode, Octant<T> * child, uint32_t depth = 0)
    {
        if (!child) child = root.get();

        const Bounds3D bounds = sceneNode.worldspaceBounds;

        if (depth < maxDepth && child->check_fit(bounds))
        {
            int3 lookup = child->get_indices(bounds);

            // No child for this octant
            if (child->arr[lookup] == nullptr)
            {
                child->arr[lookup].reset(new Octant<T>(child));

                const float3 octantMin = child->box.min();
                const float3 octantMax = child->box.max();
                const float3 octantCenter = child->box.center();

                float3 min, max;
                for (int axis : { 0, 1, 2 })
                {
                    if (lookup[axis] == 0)
                    {
                        min[axis] = octantMin[axis];
                        max[axis] = octantCenter[axis];
                    }
                    else
                    {
                        min[axis] = octantCenter[axis];
                        max[axis] = octantMax[axis];
                    }
                }

                child->arr[lookup]->box = Bounds3D(min, max);
            }

            // Recurse into a new depth
            add(sceneNode, child->arr[lookup].get(), ++depth);
        }
        // The current octant fits this 
        else
        {
            child->increase_occupancy(child);
            child->objects.push_back(sceneNode);
            sceneNode.octant = child;
        }
    }

    void create(SceneNodeContainer<T> & sceneNode)
    {
        if (!inside(sceneNode.worldspaceBounds, root->box))
        {
            throw std::invalid_argument("object is not in the bounding volume of the root node");
        }
        else
        {
            add(sceneNode, root.get());
        }
    }

    void update(SceneNodeContainer<T> & sceneNode)
    {
        if (sceneNode.octant == nullptr)
        {
            throw std::runtime_error("cannot update a scene node that is not present in the tree");
        }

        const Bounds3D box = sceneNode.worldspaceBounds;

        // Check if this scene node has bounds that are not consistent with its assigned octant
        if (!(inside(box, sceneNode.octant->box)))
        {
            remove(sceneNode);
            create(sceneNode);
        }

    }

    void remove(SceneNodeContainer<T> & sceneNode)
    {
        if (sceneNode.octant == nullptr)
        {
            throw std::runtime_error("cannot remove a scene node that is not present in the tree");
        }

        Octant<T> * oct = sceneNode.octant;
        oct->decrease_occupancy(oct);
        oct->objects.erase(std::find(oct->objects.begin(), oct->objects.end(), sceneNode));
        sceneNode.octant = nullptr;
    }

    void cull(Frustum & camera, std::vector<Octant<T> *> & visibleNodeList, Octant<T> * node, bool alreadyVisible)
    {
        if (!node) node = root.get();
        if (node->occupancy == 0) return;

        CullStatus status = OUTSIDE;

        if (alreadyVisible)
        {
            status = INSIDE;
        }
        else if (node == root.get())
        {
            status = INTERSECT;
        }
        else
        {
            if (camera.contains(node->box.center()))
            {
                status = INSIDE;
            }
        }

        alreadyVisible = (status == INSIDE);

        if (alreadyVisible)
        {
            visibleNodeList.push_back(node);
        }

        // Recurse into children
        Octant<T> * child;
        if ((child = node->arr[{0, 0, 0}].get()) != nullptr) cull(camera, visibleNodeList, child, alreadyVisible);
        if ((child = node->arr[{0, 0, 1}].get()) != nullptr) cull(camera, visibleNodeList, child, alreadyVisible);
        if ((child = node->arr[{0, 1, 0}].get()) != nullptr) cull(camera, visibleNodeList, child, alreadyVisible);
        if ((child = node->arr[{0, 1, 1}].get()) != nullptr) cull(camera, visibleNodeList, child, alreadyVisible);
        if ((child = node->arr[{1, 0, 0}].get()) != nullptr) cull(camera, visibleNodeList, child, alreadyVisible);
        if ((child = node->arr[{1, 0, 1}].get()) != nullptr) cull(camera, visibleNodeList, child, alreadyVisible);
        if ((child = node->arr[{1, 1, 0}].get()) != nullptr) cull(camera, visibleNodeList, child, alreadyVisible);
        if ((child = node->arr[{1, 1, 1}].get()) != nullptr) cull(camera, visibleNodeList, child, alreadyVisible);
    }
};

template<typename T>
inline void octree_debug_draw(
    const SceneOctree<T> & octree,
    GlShader * shader,
    GlMesh * boxMesh,
    GlMesh * sphereMesh,
    const float4x4 & viewProj,
    typename Octant<T> * node, // rumble rumble something about dependent types
    float3 octantColor)
{
    if (!node) node = octree.root.get();

    shader->bind();

    const auto boxModel = mul(make_translation_matrix(node->box.center()), make_scaling_matrix(node->box.size() / 2.f));
    shader->uniform("u_color", octantColor);
    shader->uniform("u_mvp", mul(viewProj, boxModel));
    boxMesh->draw_elements();

    for (auto obj : node->objects)
    {
        const auto & object = obj.object;
        const auto sphereModel = mul(object.p.matrix(), make_scaling_matrix(object.radius));
        shader->uniform("u_color", octantColor);
        shader->uniform("u_mvp", mul(viewProj, sphereModel));
        sphereMesh->draw_elements();
    }

    shader->unbind();

    // Recurse into children
    Octant<T> * child;
    if ((child = node->arr[{0, 0, 0}].get()) != nullptr) octree_debug_draw<T>(octree, shader, boxMesh, sphereMesh, viewProj, child, { 0, 0, 0 });
    if ((child = node->arr[{0, 0, 1}].get()) != nullptr) octree_debug_draw<T>(octree, shader, boxMesh, sphereMesh, viewProj, child, { 0, 0, 1 });
    if ((child = node->arr[{0, 1, 0}].get()) != nullptr) octree_debug_draw<T>(octree, shader, boxMesh, sphereMesh, viewProj, child, { 0, 1, 0 });
    if ((child = node->arr[{0, 1, 1}].get()) != nullptr) octree_debug_draw<T>(octree, shader, boxMesh, sphereMesh, viewProj, child, { 0, 1, 1 });
    if ((child = node->arr[{1, 0, 0}].get()) != nullptr) octree_debug_draw<T>(octree, shader, boxMesh, sphereMesh, viewProj, child, { 1, 0, 0 });
    if ((child = node->arr[{1, 0, 1}].get()) != nullptr) octree_debug_draw<T>(octree, shader, boxMesh, sphereMesh, viewProj, child, { 1, 0, 1 });
    if ((child = node->arr[{1, 1, 0}].get()) != nullptr) octree_debug_draw<T>(octree, shader, boxMesh, sphereMesh, viewProj, child, { 1, 1, 0 });
    if ((child = node->arr[{1, 1, 1}].get()) != nullptr) octree_debug_draw<T>(octree, shader, boxMesh, sphereMesh, viewProj, child, { 1, 1, 1 });
}

#endif // octree_hpp
