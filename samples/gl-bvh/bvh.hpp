/*
 * Based on: https://github.com/ssell/OcularEngine
 * Apache 2.0 License. Copyright 2014-2017 Steven T Sell (ssell@vertexfragment.com). All Rights Reserved.
 * See COPYING file for full attribution information.
 */

// Reference: https://henrikdahlberg.github.io/2017/11/20/cuda-bvh-builder-using-morton-curves.html
// LBVH http://graphics.snu.ac.kr/class/graphics2011/references/2007_lauterbach.pdf

#pragma once

#ifndef polymer_bvh_hpp
#define polymer_bvh_hpp

#include "math-core.hpp"
#include "math-morton.hpp"
#include "util.hpp"

#define POLYMER_BVH_DEBUG_SPAM

namespace polymer
{

    #pragma warning(push)
    #pragma warning(disable : 4244)
    constexpr inline uint32_t clz4(uint8_t v) noexcept { typedef const uint_fast8_t table_t[0x10]; return table_t{4,3,2,2,1,1,1,1,0,0,0,0,0,0,0,0}[v]; }
    constexpr inline uint32_t clz8(uint8_t v) noexcept { return ((v & 0xF0) == 0) ? 4 + clz4(v) : clz4(v >> 4); }
    constexpr inline uint32_t clz16(uint16_t v) noexcept { return ((v & 0xFF00U) == 0) ? 8 + clz8(v) : clz8(v >> 8); }
    constexpr inline uint32_t clz32(uint32_t v) noexcept { return ((v & 0xFFFF0000UL) == 0) ? 16 + clz16(v) : clz16(v >> 16); }
    constexpr inline uint32_t clz64(uint64_t v) noexcept { return ((v & 0xFFFFFFFF00000000ULL) == 0) ? 32 + clz32(v) : clz32(v >> 32); }
    #pragma warning(pop)

    enum class bvh_node_type
    {
        root     = 0,
        internal = 1,
        leaf     = 2
    };

    struct scene_object
    {
        aabb_3d bounds;
        void * user_data;
    };

    // This BVH is a binary tree. User objects are represented by leaf nodes (left/right),
    // while groups of objects are represented by internal nodes. 
    struct bvh_node
    {
        aabb_3d bounds;                    // Bounds of this node, encompassing all children
        uint64_t morton { 0 };             // The morton index value for this node
        bvh_node * parent { nullptr };     // Parent node attached to this node (nullptr if this is the root)
        bvh_node * left { nullptr };       // The 'left' child node (nullptr if this is a leaf node)
        bvh_node * right { nullptr };      // The 'right' child node (nullptr if this is a leaf node)
        scene_object * object { nullptr }; // The object attached to this node (nullptr if this an internal node)
        bvh_node_type type { bvh_node_type::root };
    };

    // Lauterbach et al: 
    // "The main disadvantage of the LBVH algorithm is that it does not build hierarchies that are optimized for 
    // performance in raytracing since it uniormly subdivides space at the median." It's also non-ideal for scenes
    // with highly non-uniform distributions, which might be improved by using https://dcgi.fel.cvut.cz/projects/emc/.

    class bvh_tree
    {
        typedef std::pair<uint64_t, scene_object*> bvh_morton_pair;

        bvh_node * root {nullptr};                  // Root scene node of the tree

        std::vector<scene_object *> objects;         // Convenience container for tree reconstruction (prevents the need of a full-traversal).
        std::vector<scene_object *> staged_objects;  // Newly added objects that are waiting to be added to the tree.
        std::vector<scene_object *> pending_updates; // Container of all dirty nodes that need to be updated / leaves that have moved or rotated.

        float morton_scale{ 0.f };
        float morton_offset{ 0.f };

        const uint64_t get_normalized_morton(const float3 & coordinate) const
        {
            assert(morton_scale != 0.f);
            const float3 transformed_coordinate = (transformed_coordinate + morton_offset) * morton_scale;
            return morton_3d(transformed_coordinate);
        }

        // @todo - can probably store an aabb containing the min/max bounds 
        // which will easier when new objects are added.
        void compute_normalized_morton_scale()
        {
            // Find the minimum and maximum extents of objects in the tree
            float min = std::numeric_limits<float>::max();
            float max = std::numeric_limits<float>::min();;
            for (const scene_object * object : objects)
            {
                const float3 center = object->bounds.center();
                min = std::min(min, std::min(center.x(), std::min(center.y(), center.z())));
                max = std::max(max, std::max(center.x(), std::max(center.y(), center.z())));
            }

            // Find the transform values needed to transform all values to the range 0.0 to 1.0
            morton_scale = 1.f / std::max(0.0001f, (max - min));
            morton_offset = (min < 0.f) ? -min : 0.f;
        }

    public:

        bvh_tree() = default;
        ~bvh_tree() { destroy(); }

        void destroy()
        {
            if (root)
            {
                destroy_recursive(root);
                objects.clear();
                staged_objects.clear();
            }
        }

        bool contains(scene_object * object, bool check_new) const 
        { 
            if (std::find(objects.begin(), objects.end(), object) != objects.end()) return true;
            else if (std::find(staged_objects.begin(), staged_objects.end(), object) != staged_objects.end()) return true;
            return false;
        }

        void add(scene_object * object)
        {
            if (object)
            {
                if (contains(object, true)) remove(object);
                staged_objects.emplace_back(object);
            }
        }

        bool remove(scene_object * object)
        {
            bool result = false;

            if (object)
            {
                bvh_node * leaf = find_parent(root, object);

                // Must remove leaf node and organize the tree
                if (leaf)
                {
                    bvh_node * parent = leaf->parent;

                    // If the parent is the root, we can simply remove the leaf.
                    if (parent->type == bvh_node_type::root)
                    {
                        if (parent->left == leaf)
                        {
                            // Remove the left child and shift the right over. 
                            delete leaf;
                            leaf = nullptr;

                            parent->left = parent->right;
                            parent->right = nullptr;
                        }
                        else
                        {
                            // This is the right child of the root. Can remove.
                            delete leaf;
                            leaf = nullptr;
                            parent->right = nullptr;
                        }

                        root->morton = root->left->morton;
                        fit_bounds_recursive(root);
                    }
                    // The parent is a non-root internal node.
                    else
                    {
                        // The parent will be removed and the remaining child will be moved to be a child of the parent's parent.
                        bvh_node * surviving_child = (parent->left == leaf) ? parent->right : parent->left;
                        bvh_node * parent_of_parent = parent->parent;

                        if (parent_of_parent->left == parent) parent_of_parent->left = surviving_child;
                        else parent_of_parent->right = surviving_child;

                        surviving_child->parent = parent_of_parent;

                        delete leaf;
                        delete parent;

                        leaf = nullptr;
                        parent = nullptr;

                        uint64_t morton = 0;

                        if (parent_of_parent->left)
                        {
                            morton = parent_of_parent->left->morton;

                            if (parent_of_parent->right)
                            {
                                morton += parent_of_parent->right->morton;
                                morton /= 2;
                            }
                        }
                        else if (parent_of_parent->right)
                        {
                            morton = parent_of_parent->right->morton;
                        }

                        parent_of_parent->morton = morton;
                        fit_bounds_recursive(parent_of_parent);
                    }

                    // Remove from object collection
                    auto findObject = std::find(objects.begin(), objects.end(), object);
                    if (findObject != objects.end()) { objects.erase(findObject); }

                    // Make sure the object isn't being queued for addition to the tree
                    for (auto iter = staged_objects.begin(); iter != staged_objects.end(); ++iter)
                    {
                        if ((*iter) == object)
                        {
                            staged_objects.erase(iter);
                            break;
                        }
                    }

                    result = true;
                }
            }

            // Possibility that we are being asked to remove an item that is still in the
            // staged object collection (added and removed prior to a build call)
            if (!result)
            {
                for (auto iter = staged_objects.begin(); iter != staged_objects.end(); ++iter)
                {
                    if ((*iter) == object)
                    {
                        result = true;
                        staged_objects.erase(iter);
                        break;
                    }
                }
            }

            return result;
        }

        void build()
        {
            rebuild();
        }

        void get_flat_node_list(std::vector<bvh_node *> & list, bvh_node * node = nullptr)
        {   
            if (node == nullptr) get_flat_node_list(list, root);
            else
            {
                list.push_back(node);
                if (node->left) get_flat_node_list(list, node->left);
                if (node->right) get_flat_node_list(list, node->right);
            }
        }

        void debug_print_tree(std::ostream & output)
        {
            std::size_t indent_level = 0;
            const auto print_recursive = [&, this](const auto & self, const bvh_node * node) -> void
            {
                auto total_indents = std::string(indent_level, '\t');

                output << total_indents.c_str();
                ++indent_level;

                output << "[node] " << (int)node->type << " / " << node << std::endl;

                if (node->left) self(self, node->left);
                if (node->right) self(self, node->right);

                --indent_level;
            };

            print_recursive(print_recursive, root);
        } 

        bool intersect(const ray & ray, std::vector<std::pair<scene_object*, float>> & results) const
        {
            results.reserve(objects.size());

            intersect_internal(root, ray, results);

            std::sort(results.begin(), results.end(), [](auto & first, auto & second) -> bool
            {
                return (first.second) < (second.second);
            });

            return results.size() ? true : false;
        }

    private: 

        void intersect_internal(bvh_node * node, const ray & ray, std::vector<std::pair<scene_object*, float>> & results) const
        {
            if (node)
            {
                float outMinT, outMaxT;
                const bool hit = intersect_ray_box(ray, node->bounds.min(), node->bounds.max(), &outMinT, &outMaxT);

                if (hit)
                {
                    if ((node->type == bvh_node_type::leaf) && node->object)
                    {
                        results.emplace_back(std::make_pair(node->object, outMinT));
                    }
                    else
                    {
                        intersect_internal(node->left, ray, results);
                        intersect_internal(node->right, ray, results);
                    }
                }
            }
        }

        // Completely rebuild the tree
        // Call on initial tree construction or when a significant number of new objects have been added.
        void rebuild()
        {
            destroy_recursive(root);

            if (staged_objects.size() > 0)
            {
                objects.reserve(static_cast<uint32_t>(objects.size()) + staged_objects.size());
                objects.insert(objects.end(), staged_objects.begin(), staged_objects.end());
                staged_objects.clear();
            }

            build_internal();
        }

        void destroy_recursive(bvh_node * node) const
        {
            if (node)
            {
                destroy_recursive(node->left);
                destroy_recursive(node->right);
                delete node;
                node = nullptr;
            }
        }

        void insert_object(scene_object * object)
        {
            if (object)
            {
                const uint64_t morton = get_normalized_morton(object->bounds.center());

                bvh_node * newLeafNode = new bvh_node();
                newLeafNode->morton = morton;
                newLeafNode->object = object;
                newLeafNode->type = bvh_node_type::leaf;
            
                // Insert into the root
                //if (objects.size() < 2)
                //{
                    if (root == nullptr)
                    {
                        root = new bvh_node();
                        root->type = bvh_node_type::root;

                        newLeafNode->parent = root;

                        if (root->left == nullptr)
                        {
                            root->left = newLeafNode;
                        }
                        else
                        {
                            if (root->left->morton < morton)
                            {
                                root->right = newLeafNode;
                            }
                            else
                            {
                                root->right = root->left;
                                root->left = newLeafNode;
                            }
                        }
                    }
                //}
                else
                {
                    // Get the nearest leaf node
                    bvh_node * nearestLeafNode = find_nearest(root, morton);
                    bvh_node * nearestParent = nearestLeafNode->parent;

                    // Insert our new leaf into the internal parent of the one we just found.
                    // The parent will already have two children. So we will need to create
                    // a new internal node as three children can not belong to a single parent.
                    bvh_node * newInternalNode = new bvh_node();
                    newInternalNode->type = bvh_node_type::internal;
                    newInternalNode->parent = nearestParent;

                    // One of the three children will remain direct descendents of the parent,
                    // the other two children will move to be descendents of the new internal node.
                    // The left-most child (smallest morton code) will remain as the direct descendent (left).
                    // The other two, will move to the new internal and place in order of their morton value.
                    if (newLeafNode->morton <= nearestParent->left->morton)
                    {
                        newInternalNode->left = nearestParent->left;
                        newInternalNode->right = nearestParent->right;

                        nearestParent->left = newLeafNode;
                    }
                    else if (newLeafNode->morton <= nearestParent->right->morton)
                    {
                        newInternalNode->left = newLeafNode;
                        newInternalNode->right = nearestParent->right;
                    }
                    else
                    {
                        newInternalNode->left = nearestParent->right;
                        newInternalNode->right = newLeafNode;
                    }

                    nearestParent->right = newInternalNode;

                    // Refit morton codes
                    newInternalNode->morton = (newInternalNode->left->morton + newInternalNode->right->morton) / 2;
                    nearestParent->morton = (nearestParent->left->morton + nearestParent->right->morton) / 2;
                }
            }
        }

        // Finds the leaf node that owns the specified object in the tree.
        bvh_node * find_parent(bvh_node * node, scene_object * object) const
        {
            bvh_node * parent = nullptr;
            if (node->type == bvh_node_type::leaf)
            {
                if (node->object == object) parent = node;
            }
            else
            {
                parent = find_parent(node->left, object);
                if (parent == nullptr) parent = find_parent(node->right, object);
            }
            return parent;
        }

        // Finds a node with the nearest morton code to the one specified.
        bvh_node * find_nearest(bvh_node * node, uint64_t const & morton) const
        {
            if (morton < node->morton) { if (node->left) return find_nearest(node->left, morton); }
            else if (morton > node->morton) { if (node->right) return find_nearest(node->right, morton); }
            return node;
        }

        void build_internal()
        {
            #ifdef POLYMER_BVH_DEBUG_SPAM
            scoped_timer t("[bvh_tree] build_internal");
            #endif

            // Generate the morton codes for each scene object and sort them.
            // This first block could be parallelized across multiple threads. 
            std::vector<bvh_morton_pair> sorted_pairs;
            {
                #ifdef POLYMER_BVH_DEBUG_SPAM
                scoped_timer t("[bvh_tree] compute and sort morton codes");
                #endif

                compute_normalized_morton_scale();

                // Create and sort codes
                for (scene_object * object : objects)
                {
                    const float3 center = object->bounds.center();
                    const uint64_t morton_code = get_normalized_morton(center);
                    sorted_pairs.push_back(std::make_pair(morton_code, object));
                }

                std::sort(sorted_pairs.begin(), sorted_pairs.end(), [](bvh_morton_pair const & a, bvh_morton_pair const & b)
                {
                    return (a.first < b.first);
                });
            }

            /* @todo - handle duplicate morton codes */

            {
                #ifdef POLYMER_BVH_DEBUG_SPAM
                scoped_timer t("[bvh_tree] make_tree_recursive(...)");
                #endif

                // Recursively build the tree top-down
                const uint32_t num_objects = static_cast<uint32_t>(objects.size());

                if (num_objects > 0)
                {
                    root = make_tree_recursive(nullptr, sorted_pairs, 0, num_objects - 1);
                    root->type = bvh_node_type::root;
                }
                else
                {
                    root = new bvh_node();
                    root->type = bvh_node_type::root;
                }
            }

            {
                // Recursively fit the bounds of each node around children
                scoped_timer t("[bvh_tree] fit_bounds_recursive(root)");
                fit_bounds_recursive(root);
            }
        }

        // Recursively generates the tree in a top-down manner beginning at the root
        bvh_node * make_tree_recursive(bvh_node * parent, std::vector<bvh_morton_pair> const & pairs, const uint32_t first, const uint32_t last) const
        {
            bvh_node * result = new bvh_node();
            result->parent = parent;

            // The split consists of one item (leaf node) 
            if (first == last)
            {
                result->type = bvh_node_type::leaf;
                result->morton = pairs[first].first;
                result->object = pairs[first].second;
            }
            else
            {
                // The split has multiple objects (internal node)
                const uint32_t split = find_split(pairs, first, last);
                result->type = bvh_node_type::internal;
                result->left = make_tree_recursive(result, pairs, first, split);
                result->right = make_tree_recursive(result, pairs, split + 1, last);
            }

            return result;
        }

        // Finds the index to split the remaining objects to fit the tree
        uint32_t find_split(const std::vector<bvh_morton_pair> & pairs, const uint32_t first, const uint32_t last) const
        {
            uint32_t result = first;

            const uint64_t firstCode = pairs[first].first;
            const uint64_t lastCode = pairs[last].first;

            // Identical morton codes: split range in the middle
            if (firstCode == lastCode)
            {
                result = (first + last) >> 1;
            }
            else
            {
                // Calculate the number of highest bits are the same for all objects
                uint32_t common_prefix = clz64(firstCode ^ lastCode);

                // Use binary search to find where the next bit differs.
                // We are looking for the highest object that shares more than
                // just the commonPrefix bits with the first one.
                uint32_t step_size = (last - first);
                uint32_t proposed_split = result;

                do
                {
                    step_size = (step_size + 1) >> 1; // Exponential decrease
                    proposed_split = result + step_size;
                    if (proposed_split < last)
                    {
                        const uint64_t split = pairs[proposed_split].first;
                        const uint32_t prefix = clz64(firstCode ^ split);
                        if (prefix > common_prefix) result = proposed_split;
     
                    }
                } while (step_size > 1);
            }

            return result;
        }

        void fit_bounds_recursive(bvh_node * node) const
        {
            if (node)
            {
                if (node->type == bvh_node_type::leaf)
                {
                    node->bounds = node->object->bounds;
                }
                else if (node->type == bvh_node_type::internal)
                {
                    fit_bounds_recursive(node->left);
                    fit_bounds_recursive(node->right);

                    auto combined_bounds = node->left->bounds.add(node->right->bounds);
                    node->bounds = combined_bounds;
                    node->morton = get_normalized_morton(combined_bounds.center());
                }
                else
                {
                    // Care must be taken with the root node (the only internal node that may have null children).
                    fit_bounds_recursive(node->left);
                    fit_bounds_recursive(node->right);

                    if (node->left)
                    {
                        node->bounds = node->left->bounds;
                        if (node->right) node->bounds.surround(node->right->bounds);
                        node->morton = get_normalized_morton(node->bounds.center());
                    }
                    else
                    {
                        // There will never be a root node with the left child null while the right child is not null.
                    }
                }
            }
        }
    };

} // end namespace polymer

#endif // end polymer_bvh_hpp
