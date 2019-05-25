#include "environment.hpp"

#include "system-collision.hpp"
#include "system-transform.hpp"
#include "system-identifier.hpp"
#include "system-render.hpp"
#include "asset-resolver.hpp"

#include "file_io.hpp"
#include "serialization.hpp"

using namespace polymer;

render_component polymer::assemble_render_component(environment & env, const entity e)
{
    render_component r(e);
    r.material = env.render_system->get_material_component(e);
    r.mesh = env.render_system->get_mesh_component(e);
    r.world_transform = const_cast<world_transform_component*>(env.xform_system->get_world_transform(e));
    r.local_transform = const_cast<local_transform_component*>(env.xform_system->get_local_transform(e));
    return r;
}

entity environment::track_entity(entity e) 
{ 
    log::get()->engine_log->info("[environment] created tracked entity {}", e);
    active_entities.push_back(e); return e;
}

const std::vector<entity> & environment::entity_list() 
{ 
    return active_entities; 
}

void environment::copy(entity src, entity dest)
{
    visit_systems(this, [src, dest](const char * name, auto * system_pointer)
    {
        if (system_pointer)
        {
            visit_components(src, system_pointer, [system_pointer, src, dest](const char * component_name, auto & component_ref, auto... component_metadata)
            {
                if (component_ref.get_entity() == src)
                {
                    const auto id = get_typeid<std::decay<decltype(component_ref)>::type>();
                    auto component_copy = component_ref;
                    component_copy.e = dest;
                    system_pointer->create(dest, id, &component_copy);
                }
            });
        }
    });

    log::get()->engine_log->info("[environment] copied entity {} to {}", src, dest);

}

void environment::destroy(entity e)
{
    if (e == kInvalidEntity) return;

    // Destroy everything
    if (e == kAllEntities)
    {
        for (auto active : active_entities)
        {
            visit_systems(this, [active](const char * name, auto * system_pointer)
            {
                if (system_pointer) system_pointer->destroy(active);
            });
        }
        active_entities.clear();
        log::get()->engine_log->info("[environment] destroyed all active entities");
    }
    else
    {
        // Recursively destroy entities from the transform_system, including children.
        const std::vector<entity> entities_to_destroy = xform_system->destroy_with_list(e);

        for (const auto & to_be_destroyed : entities_to_destroy)
        {
            active_entities.erase(std::find(active_entities.begin(), active_entities.end(), to_be_destroyed));

            // Destroy systems this entity is attached to (except transform_system, which we did above). 
            visit_systems(this, [to_be_destroyed](const char * name, auto * system_pointer)
            {
                if (system_pointer) 
                {
                    if (auto xform_system = dynamic_cast<transform_system*>(system_pointer)) {} // no-op
                    else system_pointer->destroy(to_be_destroyed);
                }
            });
        }
    }
}

void environment::import_environment(const std::string & import_path, entity_orchestrator & o)
{
    manual_timer t;
    t.start();

    // Leave this up to applications; user-created objects in code might be part of 
    // the non-serialized scene, etc.
    //destroy(kAllEntities);

    const std::string json_txt = read_file_text(import_path);
    const json env_doc = json::parse(json_txt);
    std::unordered_map<entity, entity> remap_table;

    // Remap
    for (auto entityIterator = env_doc.begin(); entityIterator != env_doc.end(); ++entityIterator)
    {
        const entity parsed_entity = std::atoi(entityIterator.key().c_str());
        const entity new_entity = track_entity(o.create_entity());
        remap_table[parsed_entity] = new_entity; // remap old entity to new (used for transform system)
        log::get()->import_log->info("remapping {} to {}", parsed_entity, new_entity);
    }

    for (auto entityIterator = env_doc.begin(); entityIterator != env_doc.end(); ++entityIterator)
    {
        const entity parsed_entity = std::atoi(entityIterator.key().c_str());
        const entity new_entity = remap_table[parsed_entity];
        const json & comp = entityIterator.value();

        for (auto componentIterator = comp.begin(); componentIterator != comp.end(); ++componentIterator)
        {
            if (starts_with(componentIterator.key(), "@"))
            {
                const std::string type_key = componentIterator.key();
                const std::string type_name = type_key.substr(1);

                // This is a inefficient given that components are re-parsed and re-constructed
                // for every system until a system finally creates it. We should eventually return a system
                // for a component type.
                visit_systems(this, [&](const char * system_name, auto * system_pointer)
                {
                    if (system_pointer)
                    {
                        if (create_component_on_system<identifier_component>(new_entity, type_name, system_pointer, componentIterator)) {}
                        else if (create_component_on_system<mesh_component>(new_entity, type_name, system_pointer, componentIterator)) {}
                        else if (create_component_on_system<geometry_component>(new_entity, type_name, system_pointer, componentIterator)) {}
                        else if (create_component_on_system<material_component>(new_entity, type_name, system_pointer, componentIterator)) {}
                        else if (create_component_on_system<point_light_component>(new_entity, type_name, system_pointer, componentIterator)) {}
                        else if (create_component_on_system<directional_light_component>(new_entity, type_name, system_pointer, componentIterator)) {}
                        else if (create_component_on_system<procedural_skybox_component>(new_entity, type_name, system_pointer, componentIterator)) {}
                        else if (create_component_on_system<cubemap_component>(new_entity, type_name, system_pointer, componentIterator)) {}
                        else if (type_name == get_typename<local_transform_component>())
                        {
                            local_transform_component c = componentIterator.value();
                            c.e = new_entity;

                            if (auto xform_system = dynamic_cast<transform_system*>(system_pointer))
                            {
                                if (xform_system->create(new_entity, c.local_pose, c.local_scale)) { log::get()->import_log->info("[visit_systems] created {} to {}", type_name, system_name); }
                            }
                        }
                        else throw std::runtime_error("`type_name` does not match any known component type...");
                    }
                });
            }
            else throw std::runtime_error("type key mismatch!");
        }
    } // end first pass

    // Second pass (for the transform system and parent/child relationships)
    for (auto entityIterator = env_doc.begin(); entityIterator != env_doc.end(); ++entityIterator)
    {
        const entity parsed_entity = std::atoi(entityIterator.key().c_str());
        const entity new_entity = remap_table[parsed_entity];
        const json & comp = entityIterator.value();

        for (auto componentIterator = comp.begin(); componentIterator != comp.end(); ++componentIterator)
        {
            if (starts_with(componentIterator.key(), "@"))
            {
                const std::string type_key = componentIterator.key();
                const std::string type_name = type_key.substr(1);
                const poly_typeid id = get_typeid(type_name.c_str());

                visit_systems(this, [&, new_entity](const char * system_name, auto * system_pointer)
                {
                    if (system_pointer)
                    {
                        if (type_name == get_typename<local_transform_component>())
                        {
                            // Deserialize the graph component again
                            local_transform_component c = componentIterator.value();

                            if (c.parent != kInvalidEntity)
                            {
                                // At this point, all the children entities serialized from disk refer to old ones. We update them here.
                                auto remapped_parent = remap_table[c.parent];

                                if (auto xform_system = dynamic_cast<transform_system*>(system_pointer))
                                {
                                    if (xform_system->add_child(remapped_parent, new_entity)) 
                                    {
                                        log::get()->import_log->info("[visit_systems] xform_system->add_child {} (child) to {} (parent)", new_entity, remapped_parent);
                                    }
                                    else log::get()->import_log->info("[visit_systems] failed to add_child {} (child) to {} (parent)", new_entity, remapped_parent);
                                }
                            }
                        }
                    }
                });
            }
            else throw std::runtime_error("type key mismatch!");
        }
    } // end second pass

    // Finalize the transform system by refreshing the scene graph
    xform_system->refresh();

    t.stop();
    log::get()->engine_log->info("importing {} took {}ms", import_path, t.get());
}

void environment::export_environment(const std::string & export_path) 
{
    manual_timer t;
    t.start();

    json environment;

    // foreach entity (array)
    for (const auto & e : entity_list())
    {
        json entity; 

        // foreach system (flatten into list of components)
        visit_systems(this, [&](const char * system_name, auto * system_pointer)
        {
            if (system_pointer)
            {
                // foreach component (array)
                visit_components(e, system_pointer, [&](const char * component_name, auto & component_ref, auto... component_metadata)
                {
                    const std::string type_key = get_typename<std::decay<decltype(component_ref)>::type>();
                    const std::string type_name = "@" + type_key;

                    json component; 

                    // foreach field (object)
                    visit_fields(component_ref, [&](const char * name, auto & field, auto... metadata)
                    { 
                        if (auto * no_serialize = unpack<serializer_hidden>(metadata...)) return;
                        component[name] = field;
                    });

                    entity[type_name] = component;
                });
            }
        });

        environment[std::to_string(e)] = entity;
    }

    write_file_text(export_path, environment.dump(4));

    t.stop();
    log::get()->engine_log->info("exporting {} took {}ms", export_path, t.get());
}

void environment::reset(entity_orchestrator & o, int2 default_renderer_resolution, bool create_default_entities)
{
    destroy(kAllEntities);

    // Create an event manager
    event_manager.reset(new polymer::event_manager_async());

    // Create or reset other required systems for scenes to work. We don't have any kind
    // of dependency injection system, so systems are roughly created in order
    // of their importance
    xform_system = o.create_system<polymer::transform_system>(&o);
    identifier_system = o.create_system<polymer::identifier_system>(&o);
    collision_system = o.create_system<polymer::collision_system>(&o);

    // Create or reset the renderer
    renderer_settings initialSettings;
    initialSettings.renderSize = default_renderer_resolution;
    render_system = o.create_system<polymer::render_system>(initialSettings, create_default_entities, &o);

    // @todo - less than ideal that we have to remember to add tracked entities. Systems should
    // be able to do it.
    if (create_default_entities)
    {
        track_entity(render_system->get_procedural_skybox()->get_entity());
        track_entity(render_system->get_procedural_skybox()->sun_directional_light);
        track_entity(render_system->get_cubemap()->get_entity());
    }

    // Create a material library
    mat_library.reset(new polymer::material_library("../assets/materials/")); // must include trailing slash

    // Resolving assets is the last thing we should do
    resolver.reset(new polymer::asset_resolver("../assets/", this, mat_library.get()));
}