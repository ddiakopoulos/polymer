#pragma once

#include "polymer-typeid.hpp"

////////////////////////
//   name/id system   //
////////////////////////

/*
struct name_system final : public system
{
    name_system(entity_manager * f) : system(f)
    {
        register_system_for_type(this, hash_fnv1a("NameDefinition"));
    }

    ~name_system() override { }

    bool create(entity e, component_hash component_name_hash) override final
    {
        return false;
    }

    // Associates |entity| with a name. Removes any existing name associated with this entity.
    bool create(entity entity, poly_hash_value type, void * data) override final
    {
        if (type != const_hash_fnv1a("NameDefinition")) { return false; }
        return true;
        // todo - set name based on data
    }

    // Disassociates any name from |entity|.
    void destroy(entity entity) override final
    {
        auto iter = entity_to_hash_.find(entity);
        if (iter != entity_to_hash_.end())
        {
            hash_to_entity_.erase(iter->second);
            entity_to_hash_.erase(iter);
        }
        entity_to_name_.erase(entity);
    }

    // Finds the name associated with |entity|. Returns empty string if no name is found.
    std::string get_name(entity entity) const
    {
        const auto iter = entity_to_name_.find(entity);
        return iter != entity_to_name_.end() ? iter->second : "";
    }

    void set_name(entity entity, const std::string & name)
    {
        if (entity == kInvalidEntity) { return; }

        const std::string existing_name = get_name(entity);
        if (existing_name == name) { return; } // No need to proceed if current name and desired name are identical

                                               // Ensure a different entity with the same name does not already exist. This
                                               // may happen if an entity with the name had not been properly deleted or
                                               // the same entity had been created multiple times.
        if (find_entity(name) != kInvalidEntity)
        {
            std::cout << "entity " << name << " already exists..." << std::endl;
            return;
        }

        const auto hash = hash_fnv1a(name.c_str());

        hash_to_entity_.erase(hash_fnv1a(existing_name.c_str()));
        hash_to_entity_[hash] = entity;

        entity_to_name_[entity] = name;
        entity_to_hash_[entity] = hash;
    }

    entity find_entity(const std::string & name) const
    {
        const auto hash = hash_fnv1a(name.c_str());
        const auto iter = hash_to_entity_.find(hash);
        return iter != hash_to_entity_.end() ? iter->second : kInvalidEntity;
    }

    std::unordered_map<entity, std::string> entity_to_name_;
    std::unordered_map<entity, poly_hash_value> entity_to_hash_;
    std::unordered_map<poly_hash_value, entity> hash_to_entity_;
};

POLYMER_SETUP_TYPEID(name_system);

*/
