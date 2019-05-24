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

    class bvh_tree
    {
        typedef std::pair<uint64_t, scene_object*> MortonPair;

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

        //virtual void getAllVisibleObjects(Math::Frustum const& frustum, std::vector<SceneObject*>& objects) const override;
        //virtual void getIntersections(Math::Ray const& ray, std::vector<std::pair<SceneObject*, float>>& objects) const override;
        //virtual void getIntersections(Math::BoundsSphere const& bounds, std::vector<SceneObject*>& objects) const override;
        //virtual void getIntersections(Math::BoundsAABB const& bounds, std::vector<SceneObject*>& objects) const override;
        //virtual void getIntersections(Math::BoundsOBB const& bounds, std::vector<SceneObject*>& objects) const override;
        //virtual void setDirty(UUID const& uuid) override;

    };

} // end namespace polymer

#endif // end polymer_bvh_hpp