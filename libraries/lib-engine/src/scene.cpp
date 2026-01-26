#include "polymer-engine/scene.hpp"
#include "polymer-engine/serialization.hpp"
#include "polymer-engine/asset/asset-resolver.hpp"
#include "polymer-engine/object.hpp"

#include "polymer-core/util/file-io.hpp"

using namespace polymer;

void scene::copy(entity src, entity dest)
{
    log::get()->engine_log->info("[scene] copied entity {} to {}", src.as_string(), dest.as_string());
}

void scene::destroy(entity e)
{
    if (e == kInvalidEntity) return;
}

void scene::import_environment(const std::string & import_path)
{
    manual_timer t;
    t.start();

    const std::string json_txt = read_file_text(import_path);
    const json env_doc = json::parse(json_txt);

    for (auto entityIterator = env_doc.begin(); entityIterator != env_doc.end(); ++entityIterator)
    {
        const entity parsed_entity = entityIterator.key().c_str();
    }

    t.stop();
    log::get()->engine_log->info("importing {} took {}ms", import_path, t.get());
}

void scene::export_environment(const std::string & export_path) 
{
    manual_timer t;
    t.start();

    json scene;

    // for (const auto & e : entity_list())
    // {
    //     json entity; 
    //     scene[e] = entity;
    // }

    write_file_text(export_path, scene.dump(4));

    t.stop();
    log::get()->engine_log->info("exporting {} took {}ms", export_path, t.get());
}

void scene::reset(int2 default_renderer_resolution, bool create_default_entities)
{
    graph.set_scene(this);  // Ensure graph has scene pointer

    // Create renderer with the specified resolution (requires GL context to be current)
    renderer_settings render_settings;
    render_settings.renderSize = default_renderer_resolution;
    renderer.reset(new pbr_renderer(render_settings));

    // Create collision system
    the_collision_system.reset(new collision_system(graph));

    // Create a material mat_library
    mat_library.reset(new polymer::material_library());

    // Resolving assets is the last thing we should do
    resolver.reset(new polymer::asset_resolver(this, mat_library.get()));

    if (create_default_entities)
    {
        // Create procedural skybox entity
        {
            base_object skybox_obj("procedural-skybox");
            skybox_obj.add_component(transform_component());
            skybox_obj.add_component(the_procedural_skybox);
            instantiate(std::move(skybox_obj));
        }

        // Create IBL cubemap entity
        {
            base_object ibl_obj("ibl-cubemap");
            ibl_obj.add_component(transform_component());
            ibl_obj.add_component(the_cubemap);
            instantiate(std::move(ibl_obj));
        }

        // Create sun directional light entity
        {
            base_object sun_obj("sun-light");
            sun_obj.add_component(transform_component());
            directional_light_component sun_light;
            sun_light.data.direction = float3(0, -1, 0);
            sun_light.data.color = float3(1, 1, 1);
            sun_light.data.amount = 1.0f;
            sun_obj.add_component(sun_light);
            base_object & created_sun = instantiate(std::move(sun_obj));

            // Link the sun to the skybox
            for (auto & [e, obj] : graph.graph_objects)
            {
                if (auto * skybox = obj.get_component<procedural_skybox_component>())
                {
                    skybox->sun_directional_light = created_sun.get_entity();
                    break;
                }
            }
        }

        graph.refresh();
    }
}

void scene::update(float delta_time)
{
    for (auto & [entity, obj] : graph.graph_objects)
    {
        if (obj.enabled)
        {
            obj.on_update(delta_time);
        }
    }

    if (event_manager) event_manager->process();
}

base_object * scene::get_object(const entity & e)
{
    auto it = graph.graph_objects.find(e);
    return (it != graph.graph_objects.end()) ? &(it->second) : nullptr;
}

base_object & scene::instantiate(base_object && obj)
{
    entity e = obj.get_entity();
    graph.add_object(std::move(obj));
    return graph.get_object(e);
}

base_object & scene::instantiate_mesh(
    const std::string & name,
    const transform & pose,
    const float3 & scale,
    const std::string & mesh_name,
    const std::string & material_name)
{
    base_object obj(name);
    obj.add_component(transform_component(pose, scale));
    obj.add_component(mesh_component(gpu_mesh_handle(mesh_name)));
    obj.add_component(material_component(material_handle(material_name)));
    obj.add_component(geometry_component(cpu_mesh_handle(mesh_name)));
    return instantiate(std::move(obj));
}

base_object & scene::instantiate_empty(
    const std::string & name,
    const transform & pose,
    const float3 & scale)
{
    base_object obj(name);
    obj.add_component(transform_component(pose, scale));
    return instantiate(std::move(obj));
}

base_object & scene::instantiate_point_light(
    const std::string & name,
    const float3 & position,
    const float3 & color,
    float radius)
{
    base_object obj(name);
    obj.add_component(transform_component(
        transform(position), float3(1, 1, 1)));

    point_light_component light;
    light.data.position = position;
    light.data.color = color;
    light.data.radius = radius;
    obj.add_component(light);

    return instantiate(std::move(obj));
}

// Notify systems of component additions
void base_object::notify_component_added(poly_typeid tid)
{
    if (!owning_scene) return;

    // geometry_component -> collision_system
    if (tid == get_typeid<geometry_component>())
    {
        if (auto * collision = owning_scene->get_collision_system())
        {
            collision->add_collidable(e);
        }
    }

    // Future: mesh_component -> render_system, etc.
}

// Notify systems of component removals
void base_object::notify_component_removed(poly_typeid tid)
{
    if (!owning_scene) return;

    if (tid == get_typeid<geometry_component>())
    {
        if (auto * collision = owning_scene->get_collision_system())
        {
            collision->remove_collidable(e);
        }
    }
}
