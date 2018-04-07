#pragma once

#ifndef core_scene_hpp
#define core_scene_hpp

#include "gl-api.hpp"
#include "gl-mesh.hpp"
#include "gl-camera.hpp"

#include "uniforms.hpp"
#include "asset-defs.hpp"
#include "material.hpp"
#include "geometry.hpp"
#include "gl-mesh.hpp"

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
    virtual Bounds3D get_world_bounds() const = 0;
    virtual Bounds3D get_bounds() const = 0;
    virtual float3 get_scale() const = 0;
    virtual void set_scale(const float3 & s) = 0;
    virtual Pose get_pose() const = 0;
    virtual void set_pose(const Pose & p) = 0;
    virtual RaycastResult raycast(const Ray & worldRay) const = 0;
};

struct Renderable : public GameObject
{
    MaterialHandle mat;

    bool receive_shadow{ true };
    bool cast_shadow{ true };

    Material * get_material() 
    {
        if (mat.assigned())
        {
            return mat.get().get();
        }

        else return nullptr; 
    }
    void set_material(asset_handle<std::shared_ptr<Material>> handle) { mat = handle; }

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
    Bounds3D get_bounds() const override { return Bounds3D(float3(-0.5f), float3(0.5f)); }
    float3 get_scale() const override { return float3(1, 1, 1); }
    void set_scale(const float3 & s) override { /* no-op */ }

    void draw() const override
    {

    }

    Bounds3D get_world_bounds() const override
    {
        const Bounds3D local = get_bounds();
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

    Bounds3D get_bounds() const override { return Bounds3D(float3(-0.5f), float3(0.5f)); }
    float3 get_scale() const override { return float3(1, 1, 1); }
    void set_scale(const float3 & s) override { /* no-op */ }

    void draw() const override
    {

    }

    Bounds3D get_world_bounds() const override
    {
        const Bounds3D local = get_bounds();
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
    Bounds3D bounds;

    GlMeshHandle mesh{ "" };
    GeometryHandle geom{ "" };

    StaticMesh() { }

    Pose get_pose() const override { return pose; }
    void set_pose(const Pose & p) override { pose = p; }
    Bounds3D get_bounds() const override { return bounds; }
    float3 get_scale() const override { return scale; }
    void set_scale(const float3 & s) override { scale = s; }

    void draw() const override
    {
        mesh.get().draw_elements();
    }

    void update(const float & dt) override { }

    Bounds3D get_world_bounds() const override
    {
        const Bounds3D local = get_bounds();
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

struct PhysicsMesh final : public Renderable
{
    PhysicsMesh() { }
    Pose get_pose() const override { return Pose(); }
    void set_pose(const Pose & p) override { }
    float3 get_scale() const override { return float3(); }
    void set_scale(const float3 & s) override { }
    Bounds3D get_bounds() const override { return Bounds3D(); }
    Bounds3D get_world_bounds() const override { return Bounds3D(); }
    RaycastResult raycast(const Ray & worldRay) const override { return{ false, -FLT_MAX,{ 0,0,0 } }; }
};

struct VRController final : public Renderable
{
    VRController() { }
    Pose get_pose() const override { return Pose(); }
    void set_pose(const Pose & p) override { }
    float3 get_scale() const override { return float3(); }
    void set_scale(const float3 & s) override { }
    Bounds3D get_bounds() const override { return Bounds3D(); }
    Bounds3D get_world_bounds() const override { return Bounds3D(); }
    RaycastResult raycast(const Ray & worldRay) const override { return{ false, -FLT_MAX,{ 0,0,0 } }; }
};

struct TeleportDestination final : public Renderable
{
    TeleportDestination() { }
    Pose get_pose() const override { return Pose(); }
    void set_pose(const Pose & p) override { }
    float3 get_scale() const override { return float3(); }
    void set_scale(const float3 & s) override { }
    Bounds3D get_bounds() const override { return Bounds3D(); }
    Bounds3D get_world_bounds() const override { return Bounds3D(); }
    RaycastResult raycast(const Ray & worldRay) const override { return{ false, -FLT_MAX,{ 0,0,0 } }; }
};

struct Reticle final : public Renderable
{
    Reticle() { }
    Pose get_pose() const override { return Pose(); }
    void set_pose(const Pose & p) override { }
    float3 get_scale() const override { return float3(); }
    void set_scale(const float3 & s) override { }
    Bounds3D get_bounds() const override { return Bounds3D(); }
    Bounds3D get_world_bounds() const override { return Bounds3D(); }
    RaycastResult raycast(const Ray & worldRay) const override { return{ false, -FLT_MAX,{ 0,0,0 } }; }
};

struct TexturedQuad final : public Renderable
{
    TexturedQuad() { }
    Pose get_pose() const override { return Pose(); }
    void set_pose(const Pose & p) override { }
    float3 get_scale() const override { return float3(); }
    void set_scale(const float3 & s) override { }
    Bounds3D get_bounds() const override { return Bounds3D(); }
    Bounds3D get_world_bounds() const override { return Bounds3D(); }
    RaycastResult raycast(const Ray & worldRay) const override { return{ false, -FLT_MAX,{ 0,0,0 } }; }
};

struct WorldAnchor final : public GameObject
{
    WorldAnchor() { }
    Pose get_pose() const override { return Pose(); }
    void set_pose(const Pose & p) override { }
    float3 get_scale() const override { return float3(); }
    void set_scale(const float3 & s) override { }
    Bounds3D get_bounds() const override { return Bounds3D(); }
    Bounds3D get_world_bounds() const override { return Bounds3D(); }
    RaycastResult raycast(const Ray & worldRay) const override { return{ false, -FLT_MAX,{ 0,0,0 } }; }
};

struct AudioEmitter final : public GameObject
{
    AudioEmitter() { }
    Pose get_pose() const override { return Pose(); }
    void set_pose(const Pose & p) override { }
    float3 get_scale() const override { return float3(); }
    void set_scale(const float3 & s) override { }
    Bounds3D get_bounds() const override { return Bounds3D(); }
    Bounds3D get_world_bounds() const override { return Bounds3D(); }
    RaycastResult raycast(const Ray & worldRay) const override { return{ false, -FLT_MAX,{ 0,0,0 } }; }
};

//////////////////////////
//   Scene Definition   //
//////////////////////////

struct Scene
{
    std::shared_ptr<ProceduralSky> skybox;
    std::vector<std::shared_ptr<GameObject>> objects;
    std::map<std::string, std::shared_ptr<Material>> materialInstances;
};

#endif // end core_scene_hpp