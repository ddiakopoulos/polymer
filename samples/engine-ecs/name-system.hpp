#pragma once

#include "polymer-typeid.hpp"
#include "polymer-base-ecs.hpp"

////////////////////////
//   name/id system   //
////////////////////////

class name_system final : public base_system
{

    std::unordered_map<entity, std::string> entity_to_name_;
    std::unordered_map<entity, poly_hash_value> entity_to_hash_;
    std::unordered_map<poly_hash_value, entity> hash_to_entity_;

public:

    name_system(entity_orchestrator * orch) : base_system(orch)
    {
        register_system_for_type(this, hash_fnv1a("std::string"));
    }

    ~name_system() override { }

    virtual bool create(entity e, poly_typeid hash, void * data) override final
    {
        if (hash_fnv1a("std::string") != hash) { return false; }
        if (!get_name(e).empty()) throw std::runtime_error("duplicate names are not permitted");
        const std::string name = *static_cast<std::string *>(data);
        return set_name(e, name);
    }

    bool create(entity e, const std::string & name)
    {
        if (!get_name(e).empty()) throw std::runtime_error("duplicate names are not permitted");
        return set_name(e, name);
    }

    // Disassociates any name from the entity
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

    // Finds the name associated with the entity. Returns empty string if no name is found.
    std::string get_name(entity entity) const
    {
        const auto iter = entity_to_name_.find(entity);
        return iter != entity_to_name_.end() ? iter->second : "";
    }

    bool set_name(entity entity, const std::string & name)
    {
        if (entity == kInvalidEntity) { return false; }

        const std::string existing_name = get_name(entity);
        if (existing_name == name) { return false; } // No need to proceed if current name and desired name are identical

        // Ensure a different entity with the same name does not already exist. This
        // may happen if an entity with the name had not been properly deleted or
        // the same entity had been created multiple times.
        if (find_entity(name) != kInvalidEntity) return false; // fail silently
  
        const auto hash = hash_fnv1a(name.c_str());

        hash_to_entity_.erase(hash_fnv1a(existing_name.c_str()));
        hash_to_entity_[hash] = entity;

        entity_to_name_[entity] = name;
        entity_to_hash_[entity] = hash;

        return true;
    }

    entity find_entity(const std::string & name) const
    {
        const auto hash = hash_fnv1a(name.c_str());
        const auto iter = hash_to_entity_.find(hash);
        return iter != hash_to_entity_.end() ? iter->second : kInvalidEntity;
    }
};

POLYMER_SETUP_TYPEID(name_system);