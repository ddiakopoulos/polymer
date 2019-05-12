#include "environment.hpp"

#include "system-collision.hpp"
#include "system-transform.hpp"
#include "system-identifier.hpp"
#include "system-render.hpp"

#include "file_io.hpp"
#include "serialization.hpp"

using namespace polymer;

render_component polymer::assemble_render_component(environment & env, const entity e)
{
    render_component r{ e };
    r.material = env.render_system->get_material_component(e);
    r.mesh = env.render_system->get_mesh_component(e);
    r.world_transform = env.xform_system->get_world_transform(e);
    r.local_transform = env.xform_system->get_local_transform(e);
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
                    system_pointer->create(dest, id, &component_ref);
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
        log::get()->engine_log->info("[environment] destroyed all entities");
    }
    else
    {
        active_entities.erase(std::find(active_entities.begin(), active_entities.end(), e));

        // Destroy a single entity
        visit_systems(this, [e](const char * name, auto * system_pointer)
        {
            if (system_pointer) system_pointer->destroy(e);
        });

        log::get()->engine_log->info("[environment] destroyed single entity {}", e);
    }
}

void environment::import_environment(const std::string & import_path, entity_orchestrator & o)
{
    manual_timer t;
    t.start();

    destroy(kAllEntities);

    const json env_doc = json::parse(read_file_text(import_path));
    std::unordered_map<entity, entity> remap_table;

    // Remap
    for (auto entityIterator = env_doc.begin(); entityIterator != env_doc.end(); ++entityIterator)
    {
        const entity parsed_entity = std::atoi(entityIterator.key().c_str());
        const entity new_entity = track_entity(o.create_entity());
        remap_table[parsed_entity] = new_entity; // remap old entity to new (used for transform system)
        std::cout << "Remapping: " << parsed_entity << " to " << new_entity << std::endl;
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
                const poly_typeid id = get_typeid(type_name.c_str());

                // This is a inefficient given that components are re-parsed and re-constructed
                // for every system until a system finally creates it. We should eventually return a system
                // for a component type.
                visit_systems(this, [&](const char * system_name, auto * system_pointer)
                {
                    if (system_pointer)
                    {
                        if (type_name == get_typename<identifier_component>())
                        {
                            identifier_component c = componentIterator.value();
                            c.e = new_entity;
                            if (system_pointer->create(new_entity, id, &c)) std::cout << "Created " << type_name << " on " << system_name << std::endl;
                        }
                        else if (type_name == get_typename<mesh_component>())
                        {
                            mesh_component c = componentIterator.value();
                            c.e = new_entity;
                            if (system_pointer->create(new_entity, id, &c)) std::cout << "Created " << type_name << " on " << system_name << std::endl;
                        }
                        else if (type_name == get_typename<material_component>())
                        {
                            material_component c = componentIterator.value();
                            c.e = new_entity;
                            if (system_pointer->create(new_entity, id, &c)) std::cout << "Created " << type_name << " on " << system_name << std::endl;
                        }
                        else if (type_name == get_typename<geometry_component>())
                        {
                            geometry_component c = componentIterator.value();
                            c.e = new_entity;
                            if (system_pointer->create(new_entity, id, &c)) std::cout << "Created " << type_name << " on " << system_name << std::endl;
                        }
                        else if (type_name == get_typename<point_light_component>())
                        {
                            point_light_component c = componentIterator.value();
                            c.e = new_entity;
                            if (system_pointer->create(new_entity, id, &c)) std::cout << "Created " << type_name << " on " << system_name << std::endl;
                        }
                        else if (type_name == get_typename<directional_light_component>())
                        {
                            directional_light_component c = componentIterator.value();
                            c.e = new_entity;
                            if (system_pointer->create(new_entity, id, &c)) std::cout << "Created " << type_name << " on " << system_name << std::endl;
                        }
                        else if (type_name == get_typename<skybox_component>())
                        {
                            skybox_component c = componentIterator.value();
                            c.e = new_entity;
                            if (system_pointer->create(new_entity, id, &c)) std::cout << "Created " << type_name << " on " << system_name << std::endl;
                        }
                        else if (type_name == get_typename<local_transform_component>())
                        {
                            // Create a new graph component
                            local_transform_component c = componentIterator.value();

                            // Assign it's id to the one we created for this import operation (instead of the one in the file)
                            c.e = new_entity;

                            // At this point, all the children entities serialized from disk refer to old ones. We update them here.
                            if (c.parent != kInvalidEntity) c.parent = remap_table[c.parent];
                            for (auto & child : c.children) child = remap_table[child];

                            if (auto xform_system = dynamic_cast<transform_system*>(system_pointer))
                            {
                                if (xform_system->create(new_entity, c.local_pose, c.local_scale, c.parent, c.children))
                                {
                                    std::cout << "Created " << type_name << " on " << system_name << std::endl;
                                }
                            }
                        }
                        else
                        {
                            throw std::runtime_error("component type name mismatch. did you forget to add a new component type here?");
                        }
   
                    }
                });
            }
            else throw std::runtime_error("type key mismatch!");
        }
    }

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