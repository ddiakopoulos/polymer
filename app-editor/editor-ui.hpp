#pragma once

#ifndef imgui_utils_hpp
#define imgui_utils_hpp

#include "gl-api.hpp"
#include "uniforms.hpp"
#include "asset-handle-utils.hpp"
#include "material.hpp"
#include "gl-imgui.hpp"
#include "serialization.inl"
#include "imgui/imgui_internal.h"

///////////////////////////////////////////////
//   ImGui generators for object properties  //
///////////////////////////////////////////////

template<class... A>
inline bool build_imgui(const char * label, std::string & s, const A & ... metadata)
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
inline bool build_imgui(const char * label, bool & v, const A & ... metadata)
{ 
    if (auto * hidden = unpack<editor_hidden>(metadata...)) return false;
    return ImGui::Checkbox(label, &v); 
}

template<class... A>
inline bool build_imgui(const char * label, float & v, const A & ... metadata)
{ 
    if (auto * hidden = unpack<editor_hidden>(metadata...)) return false;
    auto * rangeData = unpack<range_metadata<float>>(metadata...);
    if (rangeData) return ImGui::SliderFloat(label, &v, rangeData->min, rangeData->max, "%.5f");
    else return ImGui::InputFloat(label, &v); 
}

template<class... A>
inline bool build_imgui(const char * label, int & v, const A & ... metadata)
{ 
    if (auto * hidden = unpack<editor_hidden>(metadata...)) return false;
    auto * rangeData = unpack<range_metadata<int>>(metadata...);
    auto * useInput = unpack<input_field>(metadata...);

    if (rangeData && !useInput) return ImGui::SliderInt(label, &v, rangeData->min, rangeData->max);
    else return ImGui::InputInt(label, &v);
}

template<class... A>
inline bool build_imgui(const char * label, int2 & v, const A & ... metadata)
{ 
    if (auto * hidden = unpack<editor_hidden>(metadata...)) return false;
    auto * intRange = unpack<range_metadata<int>>(metadata...);
    auto * useInput = unpack<input_field>(metadata...);
    if (intRange && !useInput) return ImGui::SliderInt2(label, &v[0], intRange->min, intRange->max);
    else return ImGui::InputInt2(label, &v.x); 
}

template<class... A>
inline bool build_imgui(const char * label, int3 & v, const A & ... metadata)
{ 
    if (auto * hidden = unpack<editor_hidden>(metadata...)) return false;
    return ImGui::InputInt3(label, &v.x); 
}

template<class... A>
inline bool build_imgui(const char * label, int4 & v, const A & ... metadata)
{ 
    if (auto * hidden = unpack<editor_hidden>(metadata...)) return false;
    return ImGui::InputInt4(label, &v.x); 
}

template<class... A>
inline bool build_imgui(const char * label, float2 & v, const A & ... metadata)
{ 
    if (auto * hidden = unpack<editor_hidden>(metadata...)) return false;
    return ImGui::InputFloat2(label, &v.x); 
}

template<class... A>
inline bool build_imgui(const char * label, float3 & v, const A & ... metadata)
{ 
    if (auto * hidden = unpack<editor_hidden>(metadata...)) return false;
    return ImGui::InputFloat3(label, &v.x);
}

template<class... A>
inline bool build_imgui(const char * label, float4 & v, const A & ... metadata)
{ 
    if (auto * hidden = unpack<editor_hidden>(metadata...)) return false;
    return ImGui::InputFloat4(label, &v.x); 
}

template<class T, class ... A> 
bool build_imgui(const char * label, asset_handle<T> & h, const A & ... metadata)
{
    if (auto * hidden = unpack<editor_hidden>(metadata...)) return false;

    int index;
    std::vector<std::string> items;

    for (auto & handle : asset_handle<T>::list())
    {
        if (handle.name == h.name) index = static_cast<int>(items.size());
        items.push_back(handle.name);
    }

    if (ImGui::Combo(label, &index, items))
    {
        h = items[index];
        return true;
    }
    else return false;
}

template<class T> std::enable_if_t<std::is_class<T>::value, bool>
build_imgui(const char * label, T & object)
{
    bool r = false;
    visit_fields(object, [&r](const char * name, auto & field, auto... metadata)
    {   
        r |= build_imgui(name, field, metadata...);
    });
    return r;
}

template<class T> bool 
inspect_object(const char * label, T * ptr)
{
    bool r = false;
    visit_subclasses(ptr, [&r, label](const char * name, auto * p)
    {
        if (p)
        {
            if (label) r = build_imgui((std::string(label) + " - " + name).c_str(), *p);
            else r = build_imgui(name, *p);
        }
    });
    return r;
}

/////////////////////////////////////////////////////////////////
//   Additional ImGui Utilities used in the Scene Editor only  //
/////////////////////////////////////////////////////////////////

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
        return Combo(label, currIndex, vector_getter, static_cast<void*>(&values), values.size());
    }

    bool ListBox(const char* label, int* currIndex, std::vector<std::string>& values)
    {
        if (values.empty()) { return false; }
        return ListBox(label, currIndex, vector_getter, static_cast<void*>(&values), values.size());
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
                ImGui::SetScrollHere(1.0f);
            }
            ScrollToBottom = false;

            ImGui::EndChild();
        }
    };

    class spdlog_editor_sink : public spdlog::sinks::sink
    {
        editor_app_log & console;
    public:
        spdlog_editor_sink(editor_app_log & c) : console(c) { };
        void log(const spdlog::details::log_msg & msg) override
        {
            console.Update(msg.raw.str());
        }
        void flush() { };
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
                uint32_t position = 0;

                switch (t)
                {
                case Left:   position = cursor.x - r.min().x; break;
                case Right:  position = r.max().x - cursor.x; break;
                case Top:    position = cursor.y - r.min().y; break;
                case Bottom: position = r.max().y - cursor.y; break;
                }

                *v = position;
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
