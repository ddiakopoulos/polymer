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

    // Track parent-child relationships to establish after all entities created
    struct parent_child_link
    {
        entity child_entity;
        entity parent_entity;
    };
    std::vector<parent_child_link> parent_child_links;

    for (auto entityIterator = env_doc.begin(); entityIterator != env_doc.end(); ++entityIterator)
    {
        const std::string entity_key = entityIterator.key();
        const json & entity_json = entityIterator.value();

        // Create entity with the parsed key as entity ID (must be valid GUID format)
        entity parsed_entity = entity(entity_key);
        base_object obj(parsed_entity);

        // Parse identifier_component for name
        if (entity_json.contains("@identifier_component"))
        {
            const json & id_json = entity_json["@identifier_component"];
            if (id_json.contains("id")) obj.name = id_json["id"].get<std::string>();
        }

        // Parse local_transform_component
        if (entity_json.contains("@local_transform_component"))
        {
            const json & xform_json = entity_json["@local_transform_component"];

            transform_component xform_c;

            if (xform_json.contains("local_pose")) xform_c.local_pose = xform_json["local_pose"].get<transform>();
            if (xform_json.contains("local_scale")) xform_c.local_scale = xform_json["local_scale"].get<float3>();

            obj.add_component(xform_c);

            // Track parent relationship for later (parent should be GUID string or empty)
            if (xform_json.contains("parent"))
            {
                const json & parent_val = xform_json["parent"];
                entity parent_ent = kInvalidEntity;

                if (parent_val.is_string())
                {
                    std::string parent_str = parent_val.get<std::string>();
                    if (!parent_str.empty()) parent_ent = entity(parent_str);
                }

                if (parent_ent != kInvalidEntity) parent_child_links.push_back({parsed_entity, parent_ent});
            }
        }

        // Parse mesh_component
        if (entity_json.contains("@mesh_component"))
        {
            mesh_component mesh_c = entity_json["@mesh_component"].get<mesh_component>();
            obj.add_component(mesh_c);
        }

        // Parse material_component
        if (entity_json.contains("@material_component"))
        {
            material_component mat_c = entity_json["@material_component"].get<material_component>();
            obj.add_component(mat_c);
        }

        // Parse geometry_component
        if (entity_json.contains("@geometry_component"))
        {
            geometry_component geom_c = entity_json["@geometry_component"].get<geometry_component>();
            obj.add_component(geom_c);
        }

        // Parse directional_light_component
        if (entity_json.contains("@directional_light_component"))
        {
            directional_light_component light_c = entity_json["@directional_light_component"].get<directional_light_component>();
            obj.add_component(light_c);
        }

        // Parse point_light_component
        if (entity_json.contains("@point_light_component"))
        {
            point_light_component light_c = entity_json["@point_light_component"].get<point_light_component>();
            obj.add_component(light_c);
        }

        // Parse procedural_skybox_component
        if (entity_json.contains("@procedural_skybox_component"))
        {
            procedural_skybox_component skybox_c = entity_json["@procedural_skybox_component"].get<procedural_skybox_component>();
            obj.add_component(skybox_c);
        }

        // Parse cubemap_component (ibl_component)
        if (entity_json.contains("@cubemap_component"))
        {
            ibl_component ibl_c = entity_json["@cubemap_component"].get<ibl_component>();
            obj.add_component(ibl_c);
        }

        instantiate(std::move(obj));
    }

    // Establish parent-child relationships
    for (const auto & link : parent_child_links)
    {
        if (graph.graph_objects.find(link.parent_entity) != graph.graph_objects.end())
        {
            graph.add_child(link.parent_entity, link.child_entity);
        }
    }

    graph.refresh();

    t.stop();
    log::get()->engine_log->info("importing {} took {}ms", import_path, t.get());
}

void scene::export_environment(const std::string & export_path)
{
    manual_timer t;
    t.start();

    json scene_json;

    for (const auto & [e, obj] : graph.graph_objects)
    {
        if (!obj.serializable) continue;

        json entity_json;

        // Serialize identifier (name)
        if (!obj.name.empty())
        {
            entity_json["@identifier_component"] = json{{"id", obj.name}};
        }

        // Serialize transform
        if (auto * xform = const_cast<base_object &>(obj).get_component<transform_component>())
        {
            json xform_json;
            xform_json["local_pose"] = xform->local_pose;
            xform_json["local_scale"] = xform->local_scale;

            // Get parent from scene graph
            entity parent = graph.get_parent(e);
            if (parent != kInvalidEntity)
            {
                xform_json["parent"] = parent.as_string();
            }
            else
            {
                xform_json["parent"] = "";
            }

            entity_json["@local_transform_component"] = xform_json;
        }

        // Serialize mesh_component
        if (auto * mesh = const_cast<base_object &>(obj).get_component<mesh_component>())
        {
            entity_json["@mesh_component"] = *mesh;
        }

        // Serialize material_component
        if (auto * mat = const_cast<base_object &>(obj).get_component<material_component>())
        {
            entity_json["@material_component"] = *mat;
        }

        // Serialize geometry_component
        if (auto * geom = const_cast<base_object &>(obj).get_component<geometry_component>())
        {
            entity_json["@geometry_component"] = *geom;
        }

        // Serialize directional_light_component
        if (auto * light = const_cast<base_object &>(obj).get_component<directional_light_component>())
        {
            entity_json["@directional_light_component"] = *light;
        }

        // Serialize point_light_component
        if (auto * light = const_cast<base_object &>(obj).get_component<point_light_component>())
        {
            entity_json["@point_light_component"] = *light;
        }

        // Serialize procedural_skybox_component
        if (auto * skybox = const_cast<base_object &>(obj).get_component<procedural_skybox_component>())
        {
            entity_json["@procedural_skybox_component"] = *skybox;
        }

        // Serialize ibl_component (cubemap)
        if (auto * ibl = const_cast<base_object &>(obj).get_component<ibl_component>())
        {
            entity_json["@cubemap_component"] = *ibl;
        }

        scene_json[e.as_string()] = entity_json;
    }

    write_file_text(export_path, scene_json.dump(4));

    t.stop();
    log::get()->engine_log->info("exporting {} took {}ms", export_path, t.get());
}

void scene::reset(int2 default_renderer_resolution, bool create_default_entities)
{
    // Clear existing scene graph objects first
    graph.clear();

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
