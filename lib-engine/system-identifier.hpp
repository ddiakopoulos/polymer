#pragma once

#ifndef polymer_system_identifier_hpp
#define polymer_system_identifier_hpp

#include "ecs/typeid.hpp"
#include "ecs/core-ecs.hpp"
#include "scene.hpp"

//////////////////////////////////
//   name/id/tag/layer system   // 
//////////////////////////////////

namespace polymer
{

    class identifier_system final : public base_system
    {
        std::unordered_map<entity, identifier_component> entity_to_name_;
        std::unordered_map<entity, poly_hash_value> entity_to_hash_;
        std::unordered_map<poly_hash_value, entity> hash_to_entity_;
    
        std::unordered_map<entity, std::vector<std::string>> entity_to_tag;

        template<class F> friend void visit_components(entity e, identifier_system * system, F f);

    public:

        identifier_system(entity_system_manager * esm) : base_system(esm)
        {
            register_system_for_type(this, get_typeid<identifier_component>());
        }

        ~identifier_system() override {}

        virtual bool create(entity e, poly_typeid type, void * data) override final
        {
            if (type != get_typeid<identifier_component>()) { return false; }
            std::string new_name = static_cast<identifier_component *>(data)->id;
            const entity existing_entity = find_entity(new_name);
            if (existing_entity != kInvalidEntity) set_name(e, "clone of " + new_name);
            return set_name(e, new_name);
        }

        virtual bool create(entity e, poly_typeid hash, void * data, void *& out_data) override final
        {
            out_data = nullptr;
            return create(e, hash, data);
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
            return iter != entity_to_name_.end() ? iter->second.id : "";
        }

        bool set_name(entity entity, const std::string & name)
        {
            if (entity == kInvalidEntity) { return false; }

            const std::string existing_name = get_name(entity);
            if (existing_name == name) { return false; } // No need to proceed if current name and desired name are identical

            // Ensure a different entity with the same name does not already exist. This
            // may happen if an entity with the name had not been properly deleted or
            // the same entity had been created multiple times.
            if (find_entity(name) != kInvalidEntity)
            {
                log::get()->engine_log->info("[identifier system] an entity by the name {} already exists...", name);
                return false;
            }

            const auto h = hash(name.c_str());

            hash_to_entity_.erase(hash(existing_name.c_str()));
            hash_to_entity_[h] = entity;

            entity_to_name_[entity] = identifier_component(entity, name);
            entity_to_hash_[entity] = h;

            return true;
        }

        entity find_entity(const std::string & name) const
        {
            const auto h = hash(name.c_str());
            const auto iter = hash_to_entity_.find(h);
            return iter != hash_to_entity_.end() ? iter->second : kInvalidEntity;
        }

        void assign_tag(const entity e, const std::string & tag)
        {
            auto & tags = entity_to_tag[e];
            tags.push_back(tag);
        }

        std::vector<std::string> get_tags(const entity e)
        {
            return entity_to_tag[e];
        }
    };

    POLYMER_SETUP_TYPEID(identifier_system);

    // pass-through visit_fields for a string since the id system has no component type
    template<class F> void visit_fields(std::string & str, F f) 
    { 
        f("id", str); 
    }

    template<class F> void visit_components(entity e, identifier_system * system, F f)
    {
        auto iter = system->entity_to_name_.find(e);
        if (iter != system->entity_to_name_.end()) f("identifier component", iter->second);
    }

} // end namespace polymer

#endif // end polymer_system_identifier_hpp
