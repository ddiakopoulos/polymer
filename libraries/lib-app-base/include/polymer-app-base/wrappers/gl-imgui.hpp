#pragma once

#ifndef polymer_imgui_hpp
#define polymer_imgui_hpp

#include <memory>
#include <map>
#include <string>

#include "IconsFontAwesome4.h"

#include "polymer-core/math/math-common.hpp"
#include "polymer-core/util/util.hpp"

#include "polymer-gfx-gl/gl-api.hpp"

#if defined(POLYMER_PLATFORM_WINDOWS)
#pragma warning(push)
#pragma warning(disable : 4244)
#endif

// Implicit casts for linalg types
#define IM_VEC2_CLASS_EXTRA                                               \
ImVec2(const polymer::float2 & f) { x = f.x; y = f.y; }                   \
operator polymer::float2() const { return polymer::float2(x,y); }         \
ImVec2(const polymer::int2 & f) { x = f.x; y = f.y; }                     \
operator polymer::int2() const { return polymer::int2(x,y); }             
                                                                          
#define IM_VEC4_CLASS_EXTRA                                               \
ImVec4(const polymer::float4 & f) { x = f.x; y = f.y; z = f.z; w = f.w; } \
operator polymer::float4() const { return polymer::float4(x,y,z,w); }

#include "imgui/imgui.h"

using namespace polymer;

struct ui_rect
{
    int2 min, max;
    bool contains(const int2 & p) const { return all(gequal(p, min)) && all(less(p, max)); }
};

struct gl_texture_2d;
namespace polymer
{
    class polymer_app;
    struct app_input_event;
}

struct GLFWwindow;
namespace gui
{

    struct imgui_data
    {
        GLFWwindow   * window = nullptr;
        ImGuiContext * context = nullptr;
        double       Time = 0.0f;
        bool         MousePressed[3] = { false, false, false };
        float        MouseWheel = 0.0f;
        int          ShaderHandle = 0, VertHandle = 0, FragHandle = 0;
        int          AttribLocationTex = 0, AttribLocationProjMtx = 0;
        int          AttribLocationPosition = 0, AttribLocationUV = 0, AttribLocationColor = 0;
        unsigned int VboHandle = 0, VaoHandle = 0, ElementsHandle = 0;
        uint32_t     FontTexture = 0;
    };

    class imgui_instance
    {
        bool create_fonts_texture();
        bool create_render_objects();
        void destroy_render_objects();
        imgui_data data;
    public:
        imgui_instance(GLFWwindow * win, bool use_default_font = false);
        ~imgui_instance();
        ImFont * add_font(const std::vector<uint8_t> & font);
        ImFont * append_icon_font(const std::vector<uint8_t> & font);
        void update_input(const polymer::app_input_event & e);
        void begin_frame(const uint32_t width = 0, const uint32_t height = 0);
        void end_frame();
    };

    class imgui_surface
    {
    private:
        gl_framebuffer renderFramebuffer;
        gl_texture_2d renderTexture;
        uint2 framebufferSize;
    protected:
        std::unique_ptr<gui::imgui_instance> imgui;
    public:
        imgui_surface(const uint2 size, GLFWwindow * window);
        uint2 get_size() const;
        gui::imgui_instance * get_instance();
        uint32_t get_render_texture() const;
        void begin_frame();
        void end_frame();
    };

    /////////////////////
    //   ImGui Theme   //
    /////////////////////

    inline void make_light_theme()
    {
        ImGuiStyle & s = ImGui::GetStyle();

        s.WindowMinSize = ImVec2(160, 20);
        s.FramePadding = ImVec2(4, 2);
        s.ItemSpacing = ImVec2(4, 2);
        s.ItemInnerSpacing = ImVec2(4, 2);

        s.Alpha = 1.0f;
        s.WindowRounding = 0.0f;
        s.FrameRounding = 0.0f;
        s.IndentSpacing = 4.0f;
        s.ColumnsMinSpacing = 50.0f;
        s.GrabMinSize = 14.0f;
        s.GrabRounding = 4.0f;
        s.ScrollbarSize = 16.0f;
        s.ScrollbarRounding = 2.0f;

        s.Colors[ImGuiCol_Text] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
        s.Colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);

        s.Colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
        s.Colors[ImGuiCol_ChildBg] =  ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
        s.Colors[ImGuiCol_PopupBg] =  ImVec4(0.92f, 0.92f, 0.92f, 1.00f);

        s.Colors[ImGuiCol_Border] = ImVec4(0.00f, 0.00f, 0.00f, 0.39f);
        s.Colors[ImGuiCol_BorderShadow] = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);

        s.Colors[ImGuiCol_FrameBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        s.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.90f, 0.11f, 0.05f, 0.40f);
        s.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.90f, 0.11f, 0.05f, 0.90f);
        s.Colors[ImGuiCol_TitleBg] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
        s.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 1.00f, 1.00f, 0.51f);
        s.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
        s.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
        s.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.25f, 0.25f, 0.25f, 0.4f);
        s.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.69f, 0.69f, 0.69f, 0.80f);
        s.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.49f, 0.49f, 0.49f, 0.80f);
        s.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
        s.Colors[ImGuiCol_CheckMark] = ImVec4(0.90f, 0.11f, 0.05f, 1.00f);
        s.Colors[ImGuiCol_SliderGrab] = ImVec4(0.90f, 0.11f, 0.05f, 0.78f);
        s.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.90f, 0.11f, 0.05f, 1.00f);
        s.Colors[ImGuiCol_Button] = ImVec4(0.90f, 0.11f, 0.05f, 0.40f);
        s.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.90f, 0.11f, 0.05f, 1.00f);
        s.Colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.25f, 1.00f);
        s.Colors[ImGuiCol_Header] = ImVec4(0.90f, 0.11f, 0.05f, 0.8f);
        s.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.90f, 0.11f, 0.05f, 0.80f);
        s.Colors[ImGuiCol_HeaderActive] = ImVec4(0.90f, 0.11f, 0.05f, 1.00f);
        s.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
        s.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.90f, 0.11f, 0.05f, 0.67f);
        s.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.90f, 0.11f, 0.05f, 0.95f);
        s.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.90f, 0.11f, 0.05f, 0.9f);
    }

    inline void make_dark_red_theme()
    {
        ImVec4* colors = ImGui::GetStyle().Colors;
        colors[ImGuiCol_Text]                   = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
        colors[ImGuiCol_WindowBg]               = ImVec4(0.00f, 0.00f, 0.00f, 0.94f);
        colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
        colors[ImGuiCol_Border]                 = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
        colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.54f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.37f, 0.14f, 0.14f, 0.67f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.39f, 0.20f, 0.20f, 0.67f);
        colors[ImGuiCol_TitleBg]                = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(0.48f, 0.16f, 0.16f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.48f, 0.16f, 0.16f, 1.00f);
        colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
        colors[ImGuiCol_CheckMark]              = ImVec4(0.56f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_SliderGrab]             = ImVec4(1.00f, 0.19f, 0.19f, 0.40f);
        colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.89f, 0.00f, 0.19f, 1.00f);
        colors[ImGuiCol_Button]                 = ImVec4(1.00f, 0.19f, 0.19f, 0.40f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(0.80f, 0.17f, 0.00f, 1.00f);
        colors[ImGuiCol_ButtonActive]           = ImVec4(0.89f, 0.00f, 0.19f, 1.00f);
        colors[ImGuiCol_Header]                 = ImVec4(0.33f, 0.35f, 0.36f, 0.53f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(0.76f, 0.28f, 0.44f, 0.67f);
        colors[ImGuiCol_HeaderActive]           = ImVec4(0.47f, 0.47f, 0.47f, 0.67f);
        colors[ImGuiCol_Separator]              = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);
        colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);
        colors[ImGuiCol_SeparatorActive]        = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);
        colors[ImGuiCol_ResizeGrip]             = ImVec4(1.00f, 1.00f, 1.00f, 0.85f);
        colors[ImGuiCol_ResizeGripHovered]      = ImVec4(1.00f, 1.00f, 1.00f, 0.60f);
        colors[ImGuiCol_ResizeGripActive]       = ImVec4(1.00f, 1.00f, 1.00f, 0.90f);
        colors[ImGuiCol_Tab]                    = ImVec4(0.07f, 0.07f, 0.07f, 0.51f);
        colors[ImGuiCol_TabHovered]             = ImVec4(0.86f, 0.23f, 0.43f, 0.67f);
        colors[ImGuiCol_TabActive]              = ImVec4(0.19f, 0.19f, 0.19f, 0.57f);
        colors[ImGuiCol_TabUnfocused]           = ImVec4(0.05f, 0.05f, 0.05f, 0.90f);
        colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.13f, 0.13f, 0.13f, 0.74f);
        colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
        colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
        colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
        colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
        colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.07f);
        colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
        colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
    }

    inline void make_dark_gold_theme()
    {
        ImGuiStyle* style = &ImGui::GetStyle();
        ImVec4* colors = style->Colors;

        colors[ImGuiCol_Text]                   = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(0.44f, 0.44f, 0.44f, 1.00f);
        colors[ImGuiCol_WindowBg]               = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
        colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
        colors[ImGuiCol_Border]                 = ImVec4(0.51f, 0.36f, 0.15f, 1.00f);
        colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg]                = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.51f, 0.36f, 0.15f, 1.00f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.78f, 0.55f, 0.21f, 1.00f);
        colors[ImGuiCol_TitleBg]                = ImVec4(0.51f, 0.36f, 0.15f, 1.00f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
        colors[ImGuiCol_MenuBarBg]              = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.06f, 0.06f, 0.06f, 0.53f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.21f, 0.21f, 0.21f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.47f, 0.47f, 0.47f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.81f, 0.83f, 0.81f, 1.00f);
        colors[ImGuiCol_CheckMark]              = ImVec4(0.78f, 0.55f, 0.21f, 1.00f);
        colors[ImGuiCol_SliderGrab]             = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
        colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
        colors[ImGuiCol_Button]                 = ImVec4(0.51f, 0.36f, 0.15f, 1.00f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
        colors[ImGuiCol_ButtonActive]           = ImVec4(0.78f, 0.55f, 0.21f, 1.00f);
        colors[ImGuiCol_Header]                 = ImVec4(0.51f, 0.36f, 0.15f, 1.00f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
        colors[ImGuiCol_HeaderActive]           = ImVec4(0.93f, 0.65f, 0.14f, 1.00f);
        colors[ImGuiCol_Separator]              = ImVec4(0.21f, 0.21f, 0.21f, 1.00f);
        colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
        colors[ImGuiCol_SeparatorActive]        = ImVec4(0.78f, 0.55f, 0.21f, 1.00f);
        colors[ImGuiCol_ResizeGrip]             = ImVec4(0.21f, 0.21f, 0.21f, 1.00f);
        colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
        colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.78f, 0.55f, 0.21f, 1.00f);
        colors[ImGuiCol_Tab]                    = ImVec4(0.51f, 0.36f, 0.15f, 1.00f);
        colors[ImGuiCol_TabHovered]             = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
        colors[ImGuiCol_TabActive]              = ImVec4(0.78f, 0.55f, 0.21f, 1.00f);
        colors[ImGuiCol_TabUnfocused]           = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
        colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);
        colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
        colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
        colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

        style->FramePadding = ImVec2(4, 2);
        style->ItemSpacing = ImVec2(10, 2);
        style->IndentSpacing = 12;
        style->ScrollbarSize = 10;

        style->WindowRounding = 4;
        style->FrameRounding = 4;
        style->PopupRounding = 4;
        style->ScrollbarRounding = 6;
        style->GrabRounding = 4;
        style->TabRounding = 4;

        style->WindowTitleAlign = ImVec2(1.0f, 0.5f);
        style->WindowMenuButtonPosition = ImGuiDir_Right;

        style->DisplaySafeAreaPadding = ImVec2(4, 4);
    }
    
    //////////////////////////////
    //   Helper Functionality   //
    //////////////////////////////

    void Texture(const int & texture, const char * label, const ImVec2 & size, const ImVec2 & uv0 = ImVec2(0,0), const ImVec2& uv1 = ImVec2(1,1), const ImVec4& tint_col = ImVec4(1,1,1,1), const ImVec4& border_col = ImVec4(0,0,0,0));
    bool ImageButton(const int & texture, const ImVec2 & size, const ImVec2 & uv0 = ImVec2(0,1),  const ImVec2& uv1 = ImVec2(1,0), int frame_padding = -1, const ImVec4& bg_col = ImVec4(0,0,0,1), const ImVec4& tint_col = ImVec4(1,1,1,1));
    bool ListBox(const char* label, int* current_item, const std::vector<std::string>& items, int height_in_items = -1);
    bool InputText(const char* label, std::string* buf, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void* user_data = NULL);
    bool InputTextMultiline(const char* label, std::string* buf, const ImVec2& size = ImVec2(0,0), ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void* user_data = NULL);
    bool Combo(const char* label, int* current_item, const std::vector<std::string>& items, int height_in_items = -1);

    class imgui_menu_stack
    {
        bool * keys;
        int current_mods;
        std::vector<bool> open;
    public:
        imgui_menu_stack(const polymer_app & app, bool * keys);
        void app_menu_begin();
        void begin(const char * label, bool enabled = true);
        bool item(const char * label, int mods = 0, int key = 0, bool enabled = true);
        void end();
        void app_menu_end();
    };

    inline void imgui_fixed_window_begin(const char * name, const ui_rect & r)
    {
        ImGui::SetNextWindowPos(r.min);
        ImGui::SetNextWindowSize(r.max - r.min);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0, 0));
        bool result = ImGui::Begin(name, NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextColored({0.78f, 0.55f, 0.21f, 1 }, name);
        ImGui::Separator();
        assert(result);
    }

    inline void imgui_fixed_window_end()
    {
        ImGui::End();
        ImGui::PopStyleVar(2);
    }

}

#pragma warning(pop)

#endif // polymer_imgui_hpp