#pragma once

#ifndef core_scene_hpp
#define core_scene_hpp

#include "geometry.hpp"

#include "gl-api.hpp"
#include "gl-mesh.hpp"
#include "gl-camera.hpp"
#include "gl-mesh.hpp"
#include "gl-procedural-sky.hpp"

#include "uniforms.hpp"
#include "material.hpp"
#include "material-library.hpp"
#include "asset-handle-utils.hpp"

#include "ecs/typeid.hpp"
#include "ecs/core-ecs.hpp"
#include "ecs/system-name.hpp"
#include "ecs/system-transform.hpp"

///////////////////////
//   Scene Objects   //
///////////////////////

using namespace polymer;

struct ViewportRaycast
{
    perspective_camera & cam; float2 viewport;
    ViewportRaycast(perspective_camera & camera, float2 viewport) : cam(camera), viewport(viewport) {}
    Ray from(float2 cursor) { return cam.get_world_ray(cursor, viewport); };
};

struct RaycastResult
{
    bool hit = false;
    float distance = std::numeric_limits<float>::max();
    float3 normal = { 0, 0, 0 };
    RaycastResult(bool h, float t, float3 n) : hit(h), distance(t), normal(n) {}
};

struct GameObject
{
    std::string id;
    virtual ~GameObject() {}
    virtual void update(const float & dt) {}
    virtual aabb_3d get_world_bounds() const = 0;
    virtual aabb_3d get_bounds() const = 0;
    virtual float3 get_scale() const = 0;
    virtual void set_scale(const float3 & s) = 0;
    virtual Pose get_pose() const = 0;
    virtual void set_pose(const Pose & p) = 0;
    virtual RaycastResult raycast(const Ray & worldRay) const = 0;
};

struct Renderable : public GameObject
{
    material_handle mat;

    bool receive_shadow{ true };
    bool cast_shadow{ true };

    material_interface * get_material() 
    {
        if (mat.assigned())
        {
            return mat.get().get();
        }

        else return nullptr; 
    }
    void set_material(asset_handle<std::shared_ptr<material_interface>> handle) { mat = handle; }

    void set_receive_shadow(const bool value) { receive_shadow = value; }
    bool get_receive_shadow() const { return receive_shadow; }

    void set_cast_shadow(const bool value) { cast_shadow = value; }
    bool get_cast_shadow() const { return cast_shadow; }

    virtual void draw() const {};
};

struct PointLight final : public Renderable
{
    bool enabled = true;
    uniforms::point_light data;

    PointLight()
    {
        receive_shadow = false;
        cast_shadow = false;
    }

    Pose get_pose() const override { return Pose(float4(0, 0, 0, 1), data.position); }
    void set_pose(const Pose & p) override { data.position = p.position; }
    aabb_3d get_bounds() const override { return aabb_3d(float3(-0.5f), float3(0.5f)); }
    float3 get_scale() const override { return float3(1, 1, 1); }
    void set_scale(const float3 & s) override { /* no-op */ }

    void draw() const override
    {

    }

    aabb_3d get_world_bounds() const override
    {
        const aabb_3d local = get_bounds();
        return{ get_pose().transform_coord(local.min()), get_pose().transform_coord(local.max()) };
    }

    RaycastResult raycast(const Ray & worldRay) const override
    {
        auto localRay = get_pose().inverse() * worldRay;
        float outT = 0.0f;
        float3 outNormal = { 0, 0, 0 };
        bool hit = intersect_ray_sphere(localRay, Sphere(data.position, 1.0f), &outT, &outNormal);
        return{ hit, outT, outNormal };
    }
};

struct DirectionalLight final : public Renderable
{
    bool enabled = true;
    uniforms::directional_light data;

    DirectionalLight()
    {
        receive_shadow = false;
        cast_shadow = false;
    }

    Pose get_pose() const override
    {
        auto directionQuat = make_quat_from_to({ 0, 1, 0 }, data.direction);
        return Pose(directionQuat);
    }

    void set_pose(const Pose & p) override
    {
        data.direction = qydir(p.orientation);
    }

    aabb_3d get_bounds() const override { return aabb_3d(float3(-0.5f), float3(0.5f)); }
    float3 get_scale() const override { return float3(1, 1, 1); }
    void set_scale(const float3 & s) override { /* no-op */ }

    void draw() const override
    {

    }

    aabb_3d get_world_bounds() const override
    {
        const aabb_3d local = get_bounds();
        return{ get_pose().transform_coord(local.min()), get_pose().transform_coord(local.max()) };
    }

    RaycastResult raycast(const Ray & worldRay) const override
    {
        return{ false, -FLT_MAX,{ 0,0,0 } };
    }
};

struct StaticMesh final : public Renderable
{
    Pose pose;
    float3 scale{ 1, 1, 1 };
    aabb_3d bounds;

    gpu_mesh_handle mesh;
    cpu_mesh_handle geom;

    StaticMesh() { }

    Pose get_pose() const override { return pose; }
    void set_pose(const Pose & p) override { pose = p; }
    aabb_3d get_bounds() const override { return bounds; }
    float3 get_scale() const override { return scale; }
    void set_scale(const float3 & s) override { scale = s; }

    void draw() const override
    {
        mesh.get().draw_elements();
    }

    void update(const float & dt) override { }

    aabb_3d get_world_bounds() const override
    {
        const aabb_3d local = get_bounds();
        const float3 scale = get_scale();
        return{ pose.transform_coord(local.min()) * scale, pose.transform_coord(local.max()) * scale };
    }

    RaycastResult raycast(const Ray & worldRay) const override
    {
        auto localRay = pose.inverse() * worldRay;
        localRay.origin /= scale;
        localRay.direction /= scale;
        float outT = 0.0f;
        float3 outNormal = { 0, 0, 0 };
        bool hit = intersect_ray_mesh(localRay, geom.get(), &outT, &outNormal);
        return{ hit, outT, outNormal };
    }

    void set_mesh_render_mode(GLenum renderMode)
    {
        if (renderMode != GL_TRIANGLE_STRIP) mesh.get().set_non_indexed(renderMode);
    }

};

//////////////////////////
//   Scene Definition   //
//////////////////////////

namespace polymer
{
    struct material_library;

    // Mesh Component (GPU-side GlMesh)
    struct mesh_component : public base_component
    {
        mesh_component() {};
        mesh_component(entity e) : base_component(e) {}
    };
    POLYMER_SETUP_TYPEID(mesh_component);

    // Geometry (CPU-side runtime_mesh)
    struct geometry_component : public base_component
    {
        geometry_component() {};
        geometry_component(entity e) : base_component(e) {}
    };
    POLYMER_SETUP_TYPEID(geometry_component);

    // Point Light
    struct point_light_component : public base_component
    {
        point_light_component() {};
        point_light_component(entity e) : base_component(e) {}
    };
    POLYMER_SETUP_TYPEID(point_light_component);

    // Directional Light
    struct directional_light_component : public base_component
    {
        directional_light_component() {};
        directional_light_component(entity e) : base_component(e) {}
    };
    POLYMER_SETUP_TYPEID(directional_light_component);

    class render_system final : public base_system
    {
    public:
        render_system(entity_orchestrator * orch) : base_system(orch)
        {
            register_system_for_type(this, hash(get_typename<mesh_component>()));
            register_system_for_type(this, hash(get_typename<point_light_component>()));
            register_system_for_type(this, hash(get_typename<directional_light_component>()));
        }
    };
    POLYMER_SETUP_TYPEID(render_system);

    class collision_system final : public base_system
    {
    public:
        collision_system(entity_orchestrator * orch) : base_system(orch)
        {
            register_system_for_type(this, hash(get_typename<geometry_component>()));
        }
    };
    POLYMER_SETUP_TYPEID(collision_system);

}

struct poly_scene
{
    std::unique_ptr<polymer::material_library> mat_library;
    std::unique_ptr<polymer::render_system> render_system;
    std::unique_ptr<polymer::collision_system> collision_system;
    std::unique_ptr<polymer::transform_system> xform_system;
    std::unique_ptr<polymer::name_system> name_system;
    std::shared_ptr<polymer::gl_procedural_sky> skybox;
};

#endif // end core_scene_hpp