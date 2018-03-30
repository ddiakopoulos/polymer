#pragma once

#ifndef imgui_utils_hpp
#define imgui_utils_hpp

#include "gl-api.hpp"
#include "uniforms.hpp"
#include "assets.hpp"
#include "material.hpp"
#include "gl-imgui.hpp"
#include "imgui/imgui_internal.h"

/////////////////////////////////////
//   ImGui Scene Editor Utilities  //
/////////////////////////////////////

namespace ImGui
{

    struct ImGuiAppLog
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

    class LogWindowSink : public spdlog::sinks::sink
    {
        ImGuiAppLog & console;
    public:
        LogWindowSink(ImGuiAppLog & c) : console(c) { };
        void log(const spdlog::details::log_msg & msg) override
        {
            console.Update(msg.raw.str());
        }
        void flush() { };
    };

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

    typedef std::pair<Bounds2D, Bounds2D> SplitRegion;

    SplitRegion Split(const Bounds2D & r, int * v, SplitType t)
    {
        ImGuiWindow * window = ImGui::GetCurrentWindowRead();

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
            case Left:   
            {
                result.first._min.x = (result.second._max.x = r.min().x + *v) + 8; 
                break;
            }
            case Right:  
            {
                result.first._max.x = (result.second._min.x = r.max().x - *v) - 8; 
                //std::cout << "Region Min: " << result.first._max.x << ", " << "17" << std::endl;
                //std::cout << "Region Max: " << result.first._max.x - 16 << ", " << result.first._max.y << std::endl;
                window->DrawList->AddRectFilled({ result.first._max.x, 17 }, { result.first._max.x + 8 , result.first._max.y }, ImGui::GetColorU32((ImGuiCol)40));
                break;
            }
            case Top:   
            {
                result.first._min.y = (result.second._max.y = r.min().y + *v) + 8; 
                break;
            }
            case Bottom: 
            {
                result.first._max.y = (result.second._min.y = r.max().y - *v) - 8; 
                break;
            }
        }

        const ImU32 hoverColor = ImGui::ColorConvertFloat4ToU32(GImGui->Style.Colors[ImGuiCol_Button]);
        const ImU32 hoverColorActive = ImGui::ColorConvertFloat4ToU32(GImGui->Style.Colors[ImGuiCol_ButtonHovered]);
        //std::cout << "Region Min: " << ImGui::GetWindowContentRegionMin().x << ", " << ImGui::GetWindowContentRegionMin().y << std::endl;
        //std::cout << "Region Max: " << ImGui::GetWindowContentRegionMax().x << ", " << ImGui::GetWindowContentRegionMax().y << std::endl;
        //std::cout << "Size: " << ImGui::GetWindowPos().x << ", " << ImGui::GetWindowPos().y << std::endl;

        //std::cout << *v << std::endl;

        // in screen space
        //window->DrawList->AddRectFilled({ result.first._max.x, 0 }, { result.first._max.x + 8 , result.first._max.y }, ImGui::GetColorU32((ImGuiCol)40));

        //std::cout << "Region Min: " << result.first._max.x << ", " << "0" << std::endl;
        //std::cout << "Region Max: " << result.first._max.x + 8 << ", " << result.first._max.y << std::endl;

        //std::cout << "-------\n";
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0))
        {
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
    };

}

// Variadic unpacking of the metadata in the style of sgorsten. The tricky bit is the use of SFINAE with `enable_if_t.
// The compiler will keep bailing out until requested type matches the type in the parameter pack, by either of the two other templates below 
// This is loosely inspired by `findArg` found here (https://github.com/WyattTechnology/Wyatt-STM/blob/master/wstm/find_arg.h) (BSD-3),
// but using pointers instead of default-constructed objects.
template<class T, class A, class... O> 
std::enable_if_t<!std::is_same<T, A>::value, const T *> unpack(const A & first, const O & ... others)
{
    // Recursively resolve piece of metadata until `others...` exhausted
    return unpack<T>(others...);
}

// Resolves the metadata when provided with a parameter pack. In the case of a single piece of metadata, this is the target.
template<class T, class... O> 
const T * unpack(const T & meta, const O & ... others) { return &meta; }

// Base template to that is resolved when there's no metadata
template<class T> 
const T * unpack() { return nullptr; }

template<class T> struct range_metadata { T min, max; };
template<class T> struct degree_metadata { T min, max; };
struct editor_hidden { };

inline bool Edit(const char * label, std::string & s)
{
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
inline bool Edit(const char * label, bool & v, const A & ... metadata)
{ 
    return ImGui::Checkbox(label, &v); 
}

template<class... A>
inline bool Edit(const char * label, float & v, const A & ... metadata)
{ 
    auto * rangeData = unpack<range_metadata<float>>(metadata...);
    if (rangeData) return ImGui::SliderFloat(label, &v, rangeData->min, rangeData->max, "%.5f");
    else return ImGui::InputFloat(label, &v); 
}

template<class... A>
inline bool Edit(const char * label, int & v, const A & ... metadata)
{ 
    auto * rangeData = unpack<range_metadata<int>>(metadata...);
    if (rangeData) return ImGui::SliderInt(label, &v, rangeData->min, rangeData->max);
    else return ImGui::InputInt(label, &v);
}

template<class... A>
inline bool Edit(const char * label, int2 & v, const A & ... metadata)
{ 
    auto * intRange = unpack<range_metadata<int>>(metadata...);
    if (intRange) return ImGui::SliderInt2(label, &v[0], intRange->min, intRange->max);
    else return ImGui::InputInt2(label, &v.x); 
}

template<class... A>
inline bool Edit(const char * label, int3 & v, const A & ... metadata)
{ 
    return ImGui::InputInt3(label, &v.x); 
}

template<class... A>
inline bool Edit(const char * label, int4 & v, const A & ... metadata)
{ 
    return ImGui::InputInt4(label, &v.x); 
}

template<class... A>
inline bool Edit(const char * label, float2 & v, const A & ... metadata)
{ 
    return ImGui::InputFloat2(label, &v.x); 
}

template<class... A>
inline bool Edit(const char * label, float3 & v, const A & ... metadata)
{ 
    return ImGui::InputFloat3(label, &v.x);
}

template<class... A>
inline bool Edit(const char * label, float4 & v, const A & ... metadata)
{ 
    return ImGui::InputFloat4(label, &v.x); 
}

template<class T, class ... A> 
bool Edit(const char * label, AssetHandle<T> & h, const A & ... metadata)
{
    int index;
    std::vector<std::string> items;

    for (auto & handle : AssetHandle<T>::list())
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
Edit(const char * label, T & object)
{
    bool r = false;
    visit_fields(object, [&r](const char * name, auto & field, auto... metadata)
    {   
        auto * hidden = unpack<editor_hidden>(metadata...);
        if (hidden)
        {
            return false;
        }
        r |= Edit(name, field, metadata...);
    });
    return r;
}

template<class T> bool 
InspectGameObjectPolymorphic(const char * label, T * ptr)
{
    bool r = false;
    visit_subclasses(ptr, [&r, label](const char * name, auto * p)
    {
        if (p)
        {
            if (label) r = Edit((std::string(label) + " - " + name).c_str(), *p);
            else r = Edit(name, *p);
        }
    });
    return r;
}

#endif // end imgui_utils_hpp
