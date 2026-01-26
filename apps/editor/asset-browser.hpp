#pragma once

#ifndef asset_browser_hpp
#define asset_browser_hpp

#include "polymer-engine/material.hpp"
#include "polymer-engine/renderer/renderer-pbr.hpp"
#include "polymer-engine/asset/asset-handle-utils.hpp"
#include "gizmo-controller.hpp"
#include "polymer-engine/scene.hpp"
#include "editor-inspector-ui.hpp"
#include "polymer-core/tools/arcball.hpp"
#include "polymer-gfx-gl/gl-texture-view.hpp"
#include "polymer-core/util/util-win32.hpp"
#include "polymer-engine/asset/asset-import.hpp"

namespace
{
    // Helper to format timestamp as a readable string
    std::string format_timestamp(uint64_t timestamp_ns)
    {
        if (timestamp_ns == 0) return "N/A";
        // Convert nanoseconds to seconds for display
        double seconds = timestamp_ns / 1e9;
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%.2f s", seconds);
        return buffer;
    }

    // Template function to draw asset table for a specific handle type
    template<typename HandleType>
    void draw_asset_table(const char * section_name, ImGuiTextFilter & filter)
    {
        auto assets = HandleType::list();

        // Count visible assets for the header
        int visible_count = 0;
        for (const auto & asset : assets)
        {
            if (filter.PassFilter(asset.name.c_str())) visible_count++;
        }

        char header[128];
        snprintf(header, sizeof(header), "%s (%d)", section_name, visible_count);

        if (ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (assets.empty())
            {
                ImGui::TextDisabled("  No assets loaded");
            }
            else
            {
                ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6, 4));

                if (ImGui::BeginTable(section_name, 3,
                    ImGuiTableFlags_Borders |
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_Resizable |
                    ImGuiTableFlags_SizingStretchProp))
                {
                    // Setup columns
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_None, 0.6f);
                    ImGui::TableSetupColumn("Timestamp", ImGuiTableColumnFlags_None, 0.2f);
                    ImGui::TableSetupColumn("Assigned", ImGuiTableColumnFlags_None, 0.2f);
                    ImGui::TableHeadersRow();

                    for (const auto & asset : assets)
                    {
                        if (!filter.PassFilter(asset.name.c_str())) continue;

                        ImGui::TableNextRow();

                        // Name column
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted(asset.name.c_str());

                        // Timestamp column
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(format_timestamp(asset.get_timestamp()).c_str());

                        // Assigned column
                        ImGui::TableSetColumnIndex(2);
                        if (asset.assigned())
                        {
                            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Yes");
                        }
                        else
                        {
                            ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.2f, 1.0f), "No");
                        }
                    }

                    ImGui::EndTable();
                }

                ImGui::PopStyleVar();
            }

            ImGui::Dummy(ImVec2(0, 8));
        }
    }
}

struct asset_browser_window final : public glfw_window
{
    std::unique_ptr<gui::imgui_instance> auxImgui;
    ImGuiTextFilter assetFilter;

    asset_browser_window(gl_context * context,
        int w, int h,
        const std::string & title,
        int samples) : glfw_window(context, w, h, title, samples)
    {
        glfwMakeContextCurrent(window);

        auxImgui.reset(new gui::imgui_instance(window, true));
        gui::make_light_theme();
    }

    virtual void on_input(const polymer::app_input_event & e) override final
    {
        if (e.window == window) auxImgui->update_input(e);
        if (ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard) return;
    }

    virtual void on_drop(std::vector<std::string> names) override final
    {
        for (auto path : names)
        {
            // ...
        }
    }

    virtual void on_window_close() override final
    {
        glfwMakeContextCurrent(window);
        auxImgui.reset();

        if (window)
        {
            glfwDestroyWindow(window);
            window = nullptr;
        }
    }

    void run()
    {
        if (!window) return;
        if (!glfwWindowShouldClose(window))
        {
            glfwMakeContextCurrent(window);

            int width, height;
            glfwGetWindowSize(window, &width, &height);

            glEnable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);

            glViewport(0, 0, width, height);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            auxImgui->begin_frame();
            gui::imgui_fixed_window_begin("asset-browser", { { 0, 0 },{ width, height } });

            ImGui::Text("Asset Browser");
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, 8));

            // Search filter
            assetFilter.Draw("Filter Assets", -1.0f);
            ImGui::Dummy(ImVec2(0, 8));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, 8));

            // Draw asset tables for each type
            draw_asset_table<texture_handle>("Textures", assetFilter);
            draw_asset_table<cubemap_handle>("Cubemaps", assetFilter);
            draw_asset_table<gpu_mesh_handle>("GPU Meshes", assetFilter);
            draw_asset_table<cpu_mesh_handle>("CPU Meshes", assetFilter);
            draw_asset_table<material_handle>("Materials", assetFilter);
            draw_asset_table<shader_handle>("Shaders", assetFilter);

            gui::imgui_fixed_window_end();
            auxImgui->end_frame();

            glFlush();

            glfwSwapBuffers(window);
        }
    }

    ~asset_browser_window()
    {
        if (window)
        {
            glfwDestroyWindow(window);
            window = nullptr;
        }
    }
};

#endif // end asset_browser_hpp
