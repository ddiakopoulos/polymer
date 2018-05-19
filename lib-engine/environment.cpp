#include "environment.hpp"
#include "system-renderer-pbr.hpp"
#include "system-collision.hpp"
#include "system-transform.hpp"
#include "system-identifier.hpp"

using namespace polymer;

entity environment::track_entity(entity e) 
{ 
    active_entities.push_back(e); return e;
}

std::vector<entity> & environment::entity_list() 
{ 
    return active_entities; 
}

void environment::clear_tracked_entities() 
{ 
    active_entities.clear(); 
}

void environment::destroy(entity e)
{
    //visit_systems(this, [e](const char * name, auto * system_pointer)
    //{
    //    if (system_pointer) system_pointer->destroy(e);
    //});
}
