#pragma once

#ifndef polymer_ui_actions_hpp
#define polymer_ui_actions_hpp

#include "absl/any.hpp"
#include "util.hpp"
#include "property.hpp"
#include <deque>

namespace polymer
{
    ///////////////////////////////
    //   action + undo_manager   //
    ///////////////////////////////

    template <typename T, typename... Args>
    std::unique_ptr<T> make_action(Args &&... args)
    {
        std::unique_ptr<T> u_ptr(new T(std::forward<Args>(args)...));
        return u_ptr;
    }

    struct action
    {
        std::string description;
        uint64_t timestamp;
        virtual ~action()      = default;
        virtual void undo()    = 0;
        virtual void redo()    = 0;
        virtual void execute() = 0;
    };

    class undo_manager final
    {
        typedef std::deque<std::unique_ptr<action>> stack_t;

        stack_t undo_actions;
        stack_t redo_actions;
        uint32_t max_stack_size {64};

        void execute_internal(stack_t & pop_from, stack_t & push_to, bool is_redo) const
        {
            if (pop_from.empty()) return;

            auto action = std::move(pop_from.front()); // move out 
            pop_from.pop_front();

            if (is_redo) action->redo();
            else action->undo();

            // Make room for new <redo> actions
            if (is_redo && push_to.size() == max_stack_size) { push_to.pop_back(); }

            push_to.push_front(std::move(action));
        }

    public:

        undo_manager() = default;
        ~undo_manager() = default;

        void undo()
        {
            execute_internal(undo_actions, redo_actions, false);
        }

        void redo()
        {
            execute_internal(redo_actions, undo_actions, true);
        }

        void execute(std::unique_ptr<action> && the_action)
        {
            the_action->execute();

            // Make room for new <undo> actions
            if (undo_actions.size() == max_stack_size)
            {
                undo_actions.pop_back();
            }

            undo_actions.push_front(std::move(the_action));
        }

        void set_max_stack_size(const uint32_t new_max_size) { max_stack_size = new_max_size; }
        uint32_t get_max_stack_size() const { return max_stack_size; }

        bool can_redo() const { return (redo_actions.size() >= 1); }
        bool can_undo() const { return (undo_actions.size() >= 1); }

        void clear() { undo_actions.clear(); redo_actions.clear(); }

        std::vector<std::string> undo_stack_descriptions() const
        {
            std::vector<std::string> descriptions;
            for (auto & a : undo_actions) descriptions.emplace_back(a->description);
            return descriptions;
        }
    };

    //////////////////////////////
    //   action_edit_property   //
    //////////////////////////////

    class action_edit_property : public action
    {
        property_action_interface & prop;
        polymer::any value_new, value_old;

    public:

        action_edit_property(property_action_interface & prop, polymer::any new_value) 
            : value_new(new_value), prop(prop)
        {
            timestamp = system_time_ns();
            value_old = prop.get_value(); 
        }

        virtual void undo() override final { prop.set_value(value_old); }
        virtual void redo() override final { prop.set_value(value_new); }
        virtual void execute() override final { prop.set_value(value_new); }
    };

    class action_create_entity : public action
    {
    public:
        action_create_entity() { timestamp = system_time_ns();}
        virtual void undo() override final {}
        virtual void redo() override final {}
        virtual void execute() override final {}
    };

    class action_delete_entity : public action
    {
    public:
        action_delete_entity() { timestamp = system_time_ns();}
        virtual void undo() override final {}
        virtual void redo() override final {}
        virtual void execute() override final {}
    };

    class action_clone_entity : public action
    {
    public:
        action_clone_entity() { timestamp = system_time_ns();}
        virtual void undo() override final {}
        virtual void redo() override final {}
        virtual void execute() override final {}
    };

    class action_select_entity : public action
    {
    public:
        action_select_entity() { timestamp = system_time_ns();}
        virtual void undo() override final {}
        virtual void redo() override final {}
        virtual void execute() override final {}
    };

    class action_deselect_entity : public action
    {
    public:
        action_deselect_entity() { timestamp = system_time_ns();}
        virtual void undo() override final {}
        virtual void redo() override final {}
        virtual void execute() override final {}
    };

    class action_transform_entity : public action
    {
    public:
        action_transform_entity() { timestamp = system_time_ns();}
        virtual void undo() override final {}
        virtual void redo() override final {}
        virtual void execute() override final {}
    };

} // end namespace polymer

#endif // end polymer_ui_actions_hpp
