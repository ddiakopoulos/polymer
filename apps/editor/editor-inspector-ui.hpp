#pragma once

#ifndef imgui_utils_hpp
#define imgui_utils_hpp

// IMPORTANT: imgui_internal.h must be included BEFORE polymer headers
// to avoid 'log' symbol ambiguity between global log() and polymer::log
#include "imgui/imgui_internal.h"

#include "polymer-engine/scene.hpp"
#include "polymer-engine/object.hpp"
#include "polymer-gfx-gl/gl-api.hpp"
#include "polymer-app-base/wrappers/gl-imgui.hpp"
#include "polymer-engine/material.hpp"
#include "polymer-engine/renderer/renderer-uniforms.hpp"
#include "polymer-engine/asset/asset-handle-utils.hpp"
#include "polymer-engine/serialization.hpp"

using namespace polymer;

///////////////////////////////////////////////
//   imgui generators for object properties  //
///////////////////////////////////////////////

struct imgui_ui_context
{

};

template<class... A>
inline bool build_imgui(imgui_ui_context & ctx, const char * label, std::string & s, const A & ... metadata)
{
    if (auto * hidden = unpack<editor_hidden>(metadata...)) return false;
    char buffer[2048];
    assert(s.size() + 1 < sizeof(buffer));
    std::memcpy(buffer, s.data(), std::min(s.size() + 1, sizeof(buffer)));
    if (ImGui::InputText(label, buffer, sizeof(buffer)))
    {
        s = buffer;
        return true;
    }
    else return false;
}

template<class... A>
inline bool build_imgui(imgui_ui_context & ctx, const char * label, bool & v, const A & ... metadata)
{ 
    if (auto * hidden = unpack<editor_hidden>(metadata...)) return false;
    return ImGui::Checkbox(label, &v); 
}

template<class... A>
inline bool build_imgui(imgui_ui_context & ctx, const char * label, float & v, const A & ... metadata)
{ 
    if (auto * hidden = unpack<editor_hidden>(metadata...)) return false;
    auto * rangeData = unpack<range_metadata<float>>(metadata...);
    if (rangeData) return ImGui::SliderFloat(label, &v, rangeData->min, rangeData->max, "%.5f");
    else return ImGui::InputFloat(label, &v); 
}

template<class... A>
inline bool build_imgui(imgui_ui_context & ctx, const char * label, int & v, const A & ... metadata)
{ 
    if (auto * hidden = unpack<editor_hidden>(metadata...)) return false;
    auto * rangeData = unpack<range_metadata<int>>(metadata...);
    auto * useInput = unpack<input_field>(metadata...);

    if (rangeData && !useInput) return ImGui::SliderInt(label, &v, rangeData->min, rangeData->max);
    else return ImGui::InputInt(label, &v, 1);
}

template<class... A>
inline bool build_imgui(imgui_ui_context & ctx, const char * label, uint32_t & v, const A & ... metadata)
{
    if (auto * hidden = unpack<editor_hidden>(metadata...)) return false;
    auto * rangeData = unpack<range_metadata<int>>(metadata...);
    auto * useInput = unpack<input_field>(metadata...);

    if (rangeData && !useInput) return ImGui::SliderInt(label, reinterpret_cast<int*>(&v), rangeData->min, rangeData->max);
    else return ImGui::InputInt(label, reinterpret_cast<int*>(&v), 1);
}

template<class... A>
inline bool build_imgui(imgui_ui_context & ctx, const char * label, int2 & v, const A & ... metadata)
{ 
    if (auto * hidden = unpack<editor_hidden>(metadata...)) return false;
    auto * intRange = unpack<range_metadata<int>>(metadata...);
    auto * useInput = unpack<input_field>(metadata...);
    if (intRange && !useInput) return ImGui::SliderInt2(label, &v[0], intRange->min, intRange->max);
    else return ImGui::InputInt2(label, &v.x); 
}

template<class... A>
inline bool build_imgui(imgui_ui_context & ctx, const char * label, int3 & v, const A & ... metadata)
{ 
    if (auto * hidden = unpack<editor_hidden>(metadata...)) return false;
    return ImGui::InputInt3(label, &v.x); 
}

template<class... A>
inline bool build_imgui(imgui_ui_context & ctx, const char * label, int4 & v, const A & ... metadata)
{ 
    if (auto * hidden = unpack<editor_hidden>(metadata...)) return false;
    return ImGui::InputInt4(label, &v.x); 
}

template<class... A> 
inline bool build_imgui(imgui_ui_context & ctx, const char * label, float2 & v, const A & ... metadata)
{ 
    if (auto * hidden = unpack<editor_hidden>(metadata...)) return false;
    return ImGui::InputFloat2(label, &v.x); 
}

template<class... A>
inline bool build_imgui(imgui_ui_context & ctx, const char * label, float3 & v, const A & ... metadata)
{ 
    if (auto * hidden = unpack<editor_hidden>(metadata...)) return false;
    return ImGui::InputFloat3(label, &v.x);
}

template<class... A>
inline bool build_imgui(imgui_ui_context & ctx, const char * label, float4 & v, const A & ... metadata)
{ 
    if (auto * hidden = unpack<editor_hidden>(metadata...)) return false;
    return ImGui::InputFloat4(label, &v.x); 
}

template<class... A>
inline bool build_imgui(imgui_ui_context & ctx, const char * label, quatf & v, const A & ... metadata)
{
    if (auto * hidden = unpack<editor_hidden>(metadata...)) return false;
    return ImGui::InputFloat4(label, &v.x);
}

template<class... A> 
inline bool build_imgui(imgui_ui_context & ctx, const char * label, entity & e, const A & ... metadata)
{
    if (auto * hidden = unpack<editor_hidden>(metadata...)) return false;
    return ImGui::InputInt(label, (int *) &e);
}

template<class... A> 
inline bool build_imgui(imgui_ui_context & ctx, const char * label, std::vector<entity> & e, const A & ... metadata)
{
    if (auto * hidden = unpack<editor_hidden>(metadata...)) return false;
    // todo - how to edit this? 
    return false;
}

template<class T, class ... A> 
inline bool build_imgui(imgui_ui_context & ctx, const char * label, asset_handle<T> & h, const A & ... metadata)
{
    if (auto * hidden = unpack<editor_hidden>(metadata...)) return false;

    int index{ -1 };
    std::vector<std::string> items;

    // List all handles for this type
    for (auto & handle : asset_handle<T>::list())
    {
        // Pre-select index from list if handle matches name
        if (handle.name == h.name)
        {
            index = static_cast<int>(items.size());
        }
        items.push_back(handle.name);
    }

    if (ImGui::Combo(label, &index, items))
    {
        // if we've selected none
        if (index == items.size())
        {
            h = {}; // clear / zero assign the asset handle
        }
        else
        {
            h = items[index]; // Selected an existing asset handle
        }

        return true;
    }
    else return false;
}

template<class T> inline std::enable_if_t<std::is_class<T>::value, bool>
build_imgui(imgui_ui_context & ctx, const char * label, T & object)
{
    bool r = false;
    visit_fields(object, [&r, &ctx](const char * name, auto & field, auto... metadata)
    {   
        r |= build_imgui(ctx, name, field, metadata...);
    });
    return r;
}

// Inspect entity components using the new base_object pattern
inline bool inspect_entity_new(imgui_ui_context & ctx, base_object & obj)
{
    bool r = false;

    // Display object name
    ImGui::Text("Name: %s", obj.name.c_str());
    ImGui::Separator();

    // Transform component (always present)
    if (auto * xform = obj.get_component<transform_component>())
    {
        if (ImGui::TreeNode("transform_component"))
        {
            r |= build_imgui(ctx, "local_pose.position", xform->local_pose.position);
            r |= build_imgui(ctx, "local_pose.orientation", xform->local_pose.orientation);
            r |= build_imgui(ctx, "local_scale", xform->local_scale);
            ImGui::TreePop();
        }
    }

    // Mesh component
    if (auto * mesh = obj.get_component<mesh_component>())
    {
        if (ImGui::TreeNode("mesh_component"))
        {
            r |= build_imgui(ctx, "mesh", mesh->mesh);
            ImGui::TreePop();
        }
    }

    // Material component
    if (auto * mat = obj.get_component<material_component>())
    {
        if (ImGui::TreeNode("material_component"))
        {
            r |= build_imgui(ctx, "material", mat->material);
            r |= build_imgui(ctx, "receive_shadow", mat->receive_shadow);
            r |= build_imgui(ctx, "cast_shadow", mat->cast_shadow);
            ImGui::TreePop();
        }
    }

    // Geometry component
    if (auto * geom = obj.get_component<geometry_component>())
    {
        if (ImGui::TreeNode("geometry_component"))
        {
            r |= build_imgui(ctx, "geom", geom->geom);
            r |= build_imgui(ctx, "is_static", geom->is_static);
            ImGui::TreePop();
        }
    }

    // Point light component
    if (auto * pt_light = obj.get_component<point_light_component>())
    {
        if (ImGui::TreeNode("point_light_component"))
        {
            r |= build_imgui(ctx, "enabled", pt_light->enabled);
            r |= build_imgui(ctx, "position", pt_light->data.position);
            r |= build_imgui(ctx, "color", pt_light->data.color);
            r |= build_imgui(ctx, "radius", pt_light->data.radius);
            ImGui::TreePop();
        }
    }

    // Directional light component
    if (auto * dir_light = obj.get_component<directional_light_component>())
    {
        if (ImGui::TreeNode("directional_light_component"))
        {
            r |= build_imgui(ctx, "enabled", dir_light->enabled);
            r |= build_imgui(ctx, "direction", dir_light->data.direction);
            r |= build_imgui(ctx, "color", dir_light->data.color);
            r |= build_imgui(ctx, "amount", dir_light->data.amount);
            ImGui::TreePop();
        }
    }

    // IBL component
    if (auto * ibl = obj.get_component<ibl_component>())
    {
        if (ImGui::TreeNode("ibl_component"))
        {
            r |= build_imgui(ctx, "ibl_irradianceCubemap", ibl->ibl_irradianceCubemap);
            r |= build_imgui(ctx, "ibl_radianceCubemap", ibl->ibl_radianceCubemap);
            ImGui::TreePop();
        }
    }

    // Procedural skybox component
    if (auto * skybox = obj.get_component<procedural_skybox_component>())
    {
        if (ImGui::TreeNode("procedural_skybox_component"))
        {
            r |= build_imgui(ctx, "sun_directional_light", skybox->sun_directional_light);
            ImGui::TreePop();
        }
    }

    return r;
}

// Legacy function - kept for compatibility but deprecated
inline bool inspect_entity(imgui_ui_context & ctx, const char * label, entity e, scene & env)
{
    base_object & obj = env.get_graph().get_object(e);
    return inspect_entity_new(ctx, obj);
}

inline bool inspect_material(imgui_ui_context & ctx, base_material * material)
{
    bool r = false;

    visit_subclasses(material, [&r, &ctx](const char * name, auto * material_pointer)
    {
        if (material_pointer)
        {
            r |= build_imgui(ctx, name, *material_pointer);
        }
    });

    return r;
}

inline void copy_uniform_variant(uniform_override_t & overrides, const std::string & uniform_name, const polymer::uniform_variant_t & base_value)
{
    if (auto * val = nonstd::get_if<polymer::property<int>>(&base_value))
        overrides.table[uniform_name] = polymer::property<int>(static_cast<int>(*val));
    else if (auto * val = nonstd::get_if<polymer::property<float>>(&base_value))
        overrides.table[uniform_name] = polymer::property<float>(static_cast<float>(*val));
    else if (auto * val = nonstd::get_if<polymer::property<float2>>(&base_value))
        overrides.table[uniform_name] = polymer::property<float2>(static_cast<float2>(*val));
    else if (auto * val = nonstd::get_if<polymer::property<float3>>(&base_value))
        overrides.table[uniform_name] = polymer::property<float3>(static_cast<float3>(*val));
    else if (auto * val = nonstd::get_if<polymer::property<float4>>(&base_value))
        overrides.table[uniform_name] = polymer::property<float4>(static_cast<float4>(*val));
}

inline bool build_override_checkbox(const char * label, uniform_override_t & overrides, const std::string & uniform_name, const polymer::uniform_variant_t & base_value)
{
    bool is_overridden = overrides.table.find(uniform_name) != overrides.table.end();
    bool was_overridden = is_overridden;

    ImGui::PushID(uniform_name.c_str());

    if (ImGui::Checkbox("##override", &is_overridden))
    {
        if (is_overridden && !was_overridden)
        {
            copy_uniform_variant(overrides, uniform_name, base_value);
        }
        else if (!is_overridden && was_overridden)
        {
            overrides.table.erase(uniform_name);
        }
    }

    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(is_overridden ? "Click to revert to base material" : "Click to override this property");
    }

    ImGui::PopID();
    return is_overridden != was_overridden;
}

template<typename T>
inline bool build_override_field(imgui_ui_context & ctx, const char * label, uniform_override_t & overrides, const std::string & uniform_name, T & base_value)
{
    bool r = false;
    bool is_overridden = overrides.table.find(uniform_name) != overrides.table.end();

    if (is_overridden)
    {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.3f, 0.5f, 0.3f, 1.0f));
    }

    if (is_overridden)
    {
        polymer::uniform_variant_t & override_variant = overrides.table[uniform_name];
        if (auto * val = nonstd::get_if<polymer::property<T>>(&override_variant))
        {
            r |= build_imgui(ctx, label, val->raw());
        }
    }
    else
    {
        ImGui::BeginDisabled(true);
        T temp_value = base_value;
        build_imgui(ctx, label, temp_value);
        ImGui::EndDisabled();
    }

    if (is_overridden)
    {
        ImGui::PopStyleColor();
    }

    return r;
}

inline bool inspect_material_overrides(imgui_ui_context & ctx, base_material * material, uniform_override_t & overrides)
{
    bool r = false;

    polymer_pbr_standard * pbr = dynamic_cast<polymer_pbr_standard *>(material);
    if (pbr)
    {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), ICON_FA_PENCIL " OVERRIDE MODE");
        ImGui::SameLine();
        ImGui::TextDisabled("(editing instance, not base material)");
        ImGui::Dummy({0, 8});

        if (ImGui::Button(" " ICON_FA_UNDO " Clear All Overrides "))
        {
            overrides.table.clear();
            r = true;
        }
        ImGui::Dummy({0, 8});
        ImGui::Separator();
        ImGui::Dummy({0, 8});

        for (auto & [uniform_name, base_variant] : pbr->uniform_table)
        {
            ImGui::PushID(uniform_name.c_str());

            r |= build_override_checkbox(uniform_name.c_str(), overrides, uniform_name, base_variant);
            ImGui::SameLine();

            if (auto * val = nonstd::get_if<polymer::property<int>>(&base_variant))
            {
                r |= build_override_field<int>(ctx, uniform_name.c_str(), overrides, uniform_name, val->raw());
            }
            else if (auto * val = nonstd::get_if<polymer::property<float>>(&base_variant))
            {
                r |= build_override_field<float>(ctx, uniform_name.c_str(), overrides, uniform_name, val->raw());
            }
            else if (auto * val = nonstd::get_if<polymer::property<float2>>(&base_variant))
            {
                r |= build_override_field<float2>(ctx, uniform_name.c_str(), overrides, uniform_name, val->raw());
            }
            else if (auto * val = nonstd::get_if<polymer::property<float3>>(&base_variant))
            {
                r |= build_override_field<float3>(ctx, uniform_name.c_str(), overrides, uniform_name, val->raw());
            }
            else if (auto * val = nonstd::get_if<polymer::property<float4>>(&base_variant))
            {
                r |= build_override_field<float4>(ctx, uniform_name.c_str(), overrides, uniform_name, val->raw());
            }

            ImGui::PopID();
        }

        ImGui::Dummy({0, 8});
        ImGui::Separator();
        ImGui::Dummy({0, 8});
        ImGui::Text("Texture Handles (shared with base):");
        ImGui::Dummy({0, 4});

        ImGui::BeginDisabled(true);
        build_imgui(ctx, "albedo_handle", pbr->albedo);
        build_imgui(ctx, "normal_handle", pbr->normal);
        build_imgui(ctx, "metallic_handle", pbr->metallic);
        build_imgui(ctx, "roughness_handle", pbr->roughness);
        build_imgui(ctx, "emissive_handle", pbr->emissive);
        build_imgui(ctx, "height_handle", pbr->height);
        build_imgui(ctx, "occlusion_handle", pbr->occlusion);
        ImGui::EndDisabled();
    }
    else if (polymer_pbr_bubble * bubble = dynamic_cast<polymer_pbr_bubble *>(material))
    {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), ICON_FA_PENCIL " OVERRIDE MODE");
        ImGui::SameLine();
        ImGui::TextDisabled("(editing instance, not base material)");
        ImGui::Dummy({0, 8});

        if (ImGui::Button(" " ICON_FA_UNDO " Clear All Overrides "))
        {
            overrides.table.clear();
            r = true;
        }
        ImGui::Dummy({0, 8});
        ImGui::Separator();
        ImGui::Dummy({0, 8});

        for (auto & [uniform_name, base_variant] : bubble->uniform_table)
        {
            ImGui::PushID(uniform_name.c_str());

            r |= build_override_checkbox(uniform_name.c_str(), overrides, uniform_name, base_variant);
            ImGui::SameLine();

            if (auto * val = nonstd::get_if<polymer::property<int>>(&base_variant))
            {
                r |= build_override_field<int>(ctx, uniform_name.c_str(), overrides, uniform_name, val->raw());
            }
            else if (auto * val = nonstd::get_if<polymer::property<float>>(&base_variant))
            {
                r |= build_override_field<float>(ctx, uniform_name.c_str(), overrides, uniform_name, val->raw());
            }
            else if (auto * val = nonstd::get_if<polymer::property<float2>>(&base_variant))
            {
                r |= build_override_field<float2>(ctx, uniform_name.c_str(), overrides, uniform_name, val->raw());
            }
            else if (auto * val = nonstd::get_if<polymer::property<float3>>(&base_variant))
            {
                r |= build_override_field<float3>(ctx, uniform_name.c_str(), overrides, uniform_name, val->raw());
            }
            else if (auto * val = nonstd::get_if<polymer::property<float4>>(&base_variant))
            {
                r |= build_override_field<float4>(ctx, uniform_name.c_str(), overrides, uniform_name, val->raw());
            }

            ImGui::PopID();
        }

        ImGui::Dummy({0, 8});
        ImGui::Separator();
        ImGui::Dummy({0, 8});
        ImGui::Text("Texture Handles (shared with base):");
        ImGui::Dummy({0, 4});

        ImGui::BeginDisabled(true);
        build_imgui(ctx, "normal_handle", bubble->normal);
        build_imgui(ctx, "thickness_handle", bubble->thickness);
        ImGui::EndDisabled();
    }
    else
    {
        ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.2f, 1.0f), "Override mode only supported for PBR materials");
        ImGui::Dummy({0, 8});
        r |= inspect_material(ctx, material);
    }

    return r;
}

// Additional imgui Utilities used in Polymer's scene editor only

namespace ImGui
{
    static auto vector_getter = [](void* vec, int idx, const char** out_text)
    {
        auto & vector = *static_cast<std::vector<std::string>*>(vec);
        if (idx < 0 || idx >= static_cast<int>(vector.size())) { return false; }
        *out_text = vector.at(idx).c_str();
        return true;
    };

    bool Combo(const char* label, int* currIndex, std::vector<std::string> & values)
    {
        if (values.empty()) { return false; }
        return Combo(label, currIndex, vector_getter, static_cast<void*>(&values), static_cast<int>(values.size()));
    }

    bool ListBox(const char* label, int* currIndex, std::vector<std::string>& values)
    {
        if (values.empty()) { return false; }
        return ListBox(label, currIndex, vector_getter, static_cast<void*>(&values), static_cast<int>(values.size()));
    }

    struct editor_app_log
    {
        std::vector<std::string> buffer;
        ImGuiTextFilter Filter;

        bool ScrollToBottom = true;

        void Clear() { buffer.clear(); }

        void Update(const std::string & message)
        {
            buffer.push_back(message);
            ScrollToBottom = true;
        }

        void Draw(const char * title)
        {
            if (ImGui::Button("Clear")) Clear();
            ImGui::SameLine();

            bool copy = ImGui::Button("Copy");

            ImGui::SameLine();

            Filter.Draw("Filter", -100.0f);
            ImGui::Separator();

            ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

            if (copy) ImGui::LogToClipboard();

            if (Filter.IsActive())
            {
                for (auto & s : buffer)
                {
                    if (Filter.PassFilter(s.c_str()))
                    {
                        ImGui::TextUnformatted(s.c_str());
                    }
                }
            }
            else
            {
                for (auto & s : buffer)
                {
                    ImGui::TextUnformatted(s.c_str());
                }
            }

            if (ScrollToBottom)
            {
                ImGui::SetScrollHereY(1.0f);
            }
            ScrollToBottom = false;

            ImGui::EndChild();
        }
    };

    class spdlog_editor_sink : public spdlog::sinks::base_sink<std::mutex>
    {
        editor_app_log & console;
    public:
        spdlog_editor_sink(editor_app_log & c) : console(c) { };
    protected:
        void sink_it_(const spdlog::details::log_msg & msg) override
        {
            spdlog::memory_buf_t formatted;
            spdlog::sinks::base_sink<std::mutex>::formatter_->format(msg, formatted);
            console.Update(fmt::to_string(formatted));
        }
        void flush_() override { };
    };

    enum SplitType : uint32_t
    {
        Left,
        Right,
        Top,
        Bottom
    };

    typedef std::pair<aabb_2d, aabb_2d> SplitRegion;

    SplitRegion Split(const aabb_2d & r, int * v, SplitType t)
    {
        ImGuiWindow * window = ImGui::GetCurrentWindowRead();

        //window->DrawList->AddRectFilled({ -100, -100 }, { 100, 100 }, ImGui::GetColorU32((ImGuiCol)40));

        const ImGuiID id = window->GetID(v);
        const auto & io = ImGui::GetIO();

        float2 cursor = float2(io.MousePos);

        if (GImGui->ActiveId == id)
        {
            // Get the current mouse position relative to the desired axis
            if (io.MouseDown[0])
            {
                float position = 0.f;

                switch (t)
                {
                case Left:   position = cursor.x - r.min().x; break;
                case Right:  position = r.max().x - cursor.x; break;
                case Top:    position = cursor.y - r.min().y; break;
                case Bottom: position = r.max().y - cursor.y; break;
                }

                *v = static_cast<uint32_t>(position);
            }
            else ImGui::SetActiveID(0, nullptr);
        }

        SplitRegion result = { r, r };

        // Define the interactable split region
        switch (t)
        {
        case Left:   result.first._min.x = (result.second._max.x = r.min().x + *v) + 8; break;
        case Right:  result.first._max.x = (result.second._min.x = r.max().x - *v) - 8; break;
        case Top:    result.first._min.y = (result.second._max.y = r.min().y + *v) + 8; break;
        case Bottom: result.first._max.y = (result.second._min.y = r.max().y - *v) - 8; break;
        }

        if (r.contains(cursor) && !result.first.contains(cursor) && !result.second.contains(cursor))
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
            if (io.MouseClicked[0])
            {
                ImGui::SetActiveID(id, window);
            }
        }
        return result;
    }

} // end namespace ImGui

#endif // end imgui_utils_hpp
