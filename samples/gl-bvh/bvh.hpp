/*
 * Based on: https://github.com/ssell/OcularEngine
 * Apache 2.0 License. Copyright 2014-2017 Steven T Sell (ssell@vertexfragment.com). All Rights Reserved.
 * See LICENSE file for full attribution information.
 */

#pragma once

#ifndef polymer_bvh_hpp
#define polymer_bvh_hpp

#include "math-core.hpp"

namespace polymer
{
    struct scene_object
    {

    };

    struct bvh_node
    {

    };

    class bvh_tree
    {
        typedef std::pair<uint64_t, scene_object*> bvh_morton_pair;

        bool dirty{ true };                      // Dirty flag indicating if the tree needs to be updated
        bvh_node * root {nullptr};               // Root scene node of the tree
        std::vector<bvh_node *> dirty_nodes;     // Container of all dirty nodes that need to be updated
        std::vector<scene_object *> object_list; // Convenience container for tree reconstruction (prevents the need of a full-traversal).

    public:

        bvh_tree();
        bvh_tree();

        void restructure();
        void destroy();

        bool contains(scene_object * object, bool check_new) const;
        void add(scene_object * object);
        void add(std::vector<scene_object*> const & objects);
        bool remove(scene_object * object);
        void remove(std::vector<scene_object*> const & objects);
        std::vector<scene_object*> get() const;

        ///void visible(Math::Frustum const& frustum, std::vector<scene_object'*> & objects) const;
        ///void intersect(Math::Ray const& ray, std::vector<std::pair<scene_object'*, float>>& objects) const;
        ///void intersect(Math::BoundsSphere const& bounds, std::vector<scene_object'*> & objects) const;
        ///void intersect(Math::BoundsAABB const& bounds, std::vector<scene_object'*> & objects) const;
        ///void intersect(Math::BoundsOBB const& bounds, std::vector<scene_object'*> & objects) const;
        ///void mark_dirty(UUID const& uuid);

    private: 

        // Performs a complete rebuild of the tree.
        // This is a potentially costly operation and should only be called when absolutely necessary
        // (either on initial tree construction or when a significant number of new objects have been added).
        void rebuild();

        // Individually inserts new objects into the tree.
        // If the number of new objects that need to be added is significant, then a complete tree rebuild
        // will often be faster and more accurate.
        void insert_new_objects();

        // Updates all dirty nodes (leafs) whose objects have either moved or rotated.
        void update_dirty_nodes();

        // Checks to see if the tree needs to be rebuilt.
        bool rebuild_needed() const;

         // Destroys the specified node and all children.
        void destroy(bvh_node * node) const;

        // Inserts a single new object into the tree.
        void insert_object(scene_object * object);

        ///////////////////////
        //   BVH Traversal   //
        ///////////////////////

        // Finds the leaf node that owns the specified object in the tree.
        // \param[in] node   Current node.
        // \param[in] object The object to find.
        // \return The parent node.
        bvh_node * find_parent(bvh_node * node, scene_object * object) const;

        // Finds the node with the nearest morton code to the one specified.
        // \param[in] node   Current node.
        // \param[in] morton Morton code to compare against
        // \return The nearest node.
        bvh_node * find_nearest(bvh_node * node, uint64_t const & morton) const;
    
        /*
        // \param[in]  node    Current node.
        // \param[in]  frustum Frustum to test against.
        // \param[out] objects All discovered scene_object's that intersect.
        void visible(bvh_node * node, Math::Frustum const& frustum, std::vector<scene_object *>& objects) const;

        // \param[in]  node    Current node.
        // \param[in]  ray     Ray to test against.
        // \param[out] objects All discovered scene_object's that intersect and their intersection points.
        void intersect(bvh_node * node, Math::Ray const& ray, std::vector<std::pair<scene_object *, float>>& objects) const;

        // Recursively finds all scene_object's that intersect with the specified bounds.
        // \param[in]  node    Current node.
        // \param[in]  bounds  Bounds to test against.
        // \param[out] objects All discovered scene_object's that intersect.
        void intersect(bvh_node * node, Math::BoundsSphere const& bounds, std::vector<scene_object *>& objects) const;

        // Recursively finds all scene_object's that intersect with the specified bounds.
        // \param[in]  node    Current node.
        // \param[in]  bounds  Bounds to test against.
        // \param[out] objects All discovered scene_object's that intersect.
        void intersect(bvh_node * node, Math::BoundsAABB const& bounds, std::vector<scene_object *>& objects) const;

        // Recursively finds all scene_object's that intersect with the specified bounds.
        // \param[in]  node    Current node.
        // \param[in]  bounds  Bounds to test against.
        // \param[out] objects All discovered scene_object's that intersect.
        void intersect(bvh_node * node, Math::BoundsOBB const& bounds, std::vector<scene_object *>& objects) const;
        */

        //////////////////////
        //   BVH Building   //
        //////////////////////

        // Builds the tree from the collection of objects stored in m_AllObjects.
        void build();

        // Calculates and sorts the Morton Codes for all objects in the tree.
        // \param[out] pairs Container to be filled with the sorted Morton Codes and their associated scene_object
        void create_morton_pairs(std::vector<bvh_morton_pair> & pairs) const;

        // Recursively generates the tree in a top-down manner beginning at the root.
        // \param[in] parent The node calling this recursive sequence.
        // \param[in] pairs  Sorted list of morton code/scene object pairings.
        // \param[in] first  First node in the current split
        // \param[in] last   Last node in the current split (inclusive)
        // \return Pointer to the root node of the newly generated tree.
        bvh_node * generate_tree(bvh_node * parent, std::vector<bvh_morton_pair> const & pairs, uint32_t first, uint32_t last) const;

        // Finds the index to split the remaining objects to fit the tree.
        // \param[in] pairs Sorted list of morton code/scene object pairings.
        // \param[in] first Index of first object remaining to be added to the tree
        // \param[in] last  Index of the last object remaining to be added to the tree
        // \return Best index to split the remaining objects.
        uint32_t find_split(std::vector<bvh_morton_pair> const & pairs, uint32_t first, uint32_t last) const;

        // Adjusts the bounds of the specified node to fit over it's children.
        void fit_node_bounds(bvh_node * node) const;
    };

} // end namespace polymer

#endif // end polymer_bvh_hpp