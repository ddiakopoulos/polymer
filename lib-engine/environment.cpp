#include "environment.hpp"
#include "system-renderer-pbr.hpp"
#include "system-collision.hpp"
#include "system-transform.hpp"
#include "system-identifier.hpp"
#include "file_io.hpp"
#include "serialization.inl"

using namespace polymer;

entity environment::track_entity(entity e) 
{ 
    log::get()->assetLog->info("[environment] created tracked entity {}", e);
    active_entities.push_back(e); return e;
}

const std::vector<entity> & environment::entity_list() 
{ 
    return active_entities; 
}

void environment::copy(entity src, entity dest)
{
    visit_systems(this, [src](const char * name, auto * system_pointer)
    {
        if (system_pointer)
        {
            visit_components(src, system_pointer, [&](const char * component_name, auto & component_ref, auto... component_metadata)
            {
                // system_pointer->create(...)
            });
        }
    });

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
    }
    else
    {
        active_entities.erase(std::find(active_entities.begin(), active_entities.end(), e));

        // Destroy a single entity
        visit_systems(this, [e](const char * name, auto * system_pointer)
        {
            if (system_pointer) system_pointer->destroy(e);
        });
        
    }
}

void environment::import_environment(const std::string & import_path)
{
}

void environment::export_environment(const std::string & export_path) 
{
    auto json_str = cereal::serialize_to_json(*this);
    write_file_text(export_path, json_str);
}
