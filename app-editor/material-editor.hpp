#pragma once

#ifndef material_editor_hpp
#define material_editor_hpp

#include "material.hpp"
#include "fwd_renderer.hpp"
#include "asset-handle-utils.hpp"
#include "scene.hpp"
#include "editor-ui.hpp"
#include "arcball.hpp"
#include "gl-texture-view.hpp"

template<typename AssetHandleType>
bool draw_listbox(const std::string & label, ImGuiTextFilter & filter, int & selection)
{
    bool r = false;
    std::vector<std::string> assets;
    for (auto & m : AssetHandleType::list()) assets.push_back(m.name);

    ImGui::Text(label.c_str());

    ImGui::PushItemWidth(-1);
    if (ImGui::ListBoxHeader("##assets"))
    {
        for (int n = 0; n < assets.size(); ++n)
        {
            const std::string name = assets[n];
            if (!filter.PassFilter(name.c_str())) continue;
            if (ImGui::Selectable(name.c_str(), n == selection))
            {
                selection = n;
                r = true; // made a new selection
            }
        }

        ImGui::ListBoxFooter();
    }
    ImGui::PopItemWidth();
    return r;
}

struct material_editor_window final : public glfw_window
{
    std::unique_ptr<fullscreen_texture> fullscreen_surface;
    std::unique_ptr<forward_renderer> preview_renderer;
    std::unique_ptr<gui::imgui_instance> auxImgui;
    std::unique_ptr<StaticMesh> previewMesh;
    std::unique_ptr<arcball_controller> arcball;

    perspective_camera previewCam;
    render_payload previewSceneData;

    std::string stringBuffer;
    int assetSelection = -1;
    const uint32_t previewHeight = 420;
    material_library & lib;
    selection_controller<GameObject> & selector;
    StaticMesh * inspectedObject{ nullptr };

    material_editor_window(gl_context * context, int w, int h, const std::string title, int samples, polymer::material_library & lib, selection_controller<GameObject> & selector)
        : glfw_window(context, w, h, title, samples), lib(lib), selector(selector)
    {
        glfwMakeContextCurrent(window);

        fullscreen_surface.reset(new fullscreen_texture());

        // These are created on the this gl context and cached as global variables in the asset table. It will be re-assigned
        // every time this window is opened so we don't need to worry about cleaning it up.
        auto cap = make_icosasphere(3);
        create_handle_for_asset("preview-cube", make_mesh_from_geometry(cap));
        create_handle_for_asset("preview-cube", std::move(cap));

        previewMesh.reset(new StaticMesh());
        previewMesh->mesh = "preview-cube";
        previewMesh->geom = "preview-cube";
        previewMesh->pose.position = float3(0, 0, 0);

        renderer_settings previewSettings;
        previewSettings.renderSize = int2(w, previewHeight);
        previewSettings.msaaSamples = 8;
        previewSettings.performanceProfiling = false;
        previewSettings.useDepthPrepass = false;
        previewSettings.tonemapEnabled = false;
        previewSettings.shadowsEnabled = false;

        preview_renderer.reset(new forward_renderer(previewSettings));

        previewCam.pose = look_at_pose_rh(float3(0, 0.25f, 2), previewMesh->pose.position);
        auxImgui.reset(new gui::imgui_instance(window, true));

        auto fontAwesomeBytes = read_file_binary("../assets/fonts/font_awesome_4.ttf");
        auxImgui->append_icon_font(fontAwesomeBytes);

        arcball.reset(new arcball_controller({ (float)w, (float)h }));

        gui::make_light_theme();
    }

    virtual void on_input(const polymer::InputEvent & e) override final
    {
        if (e.window == window) auxImgui->update_input(e);
        if (ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard) return;

        if (e.type == InputEvent::MOUSE && e.is_down())
        {
            arcball->mouse_down(e.cursor);
        }
        else if (e.type == InputEvent::CURSOR && e.drag)
        {
            arcball->mouse_drag(e.cursor);
            previewMesh->pose.orientation = safe_normalize(qmul(arcball->currentQuat, previewMesh->pose.orientation));
        }
    }

    virtual void on_window_close() override final
    {
        glfwMakeContextCurrent(window);

        // Why do we do all these resets? Well, the way the GlObject handle system works, the
        // destructor is called on the context of the destroyer. In the case of a secondary window,
        // that would be the main thread. Instead, we need to manually clean up everything here before
        // the destructor is called. 
        fullscreen_surface.reset();
        preview_renderer.reset();
        auxImgui.reset();
        previewMesh.reset();

        asset_handle<GlMesh>::destroy("preview-cube");
        asset_handle<Geometry>::destroy("preview-cube");

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

            auto entitySelection = selector.get_selection();

            // Only one object->material can be edited at once
            if (entitySelection.size() == 1)
            {
                // Produce a list of material instance names. This could also be done by
                // iterating the keys of instances in the material library, but using
                // asset_handles is more canonical.
                std::vector<std::string> materialNames;
                for (auto & m : asset_handle<std::shared_ptr<material_interface>>::list()) materialNames.push_back(m.name);

                // Get the object from the selection
                GameObject * selected_object = entitySelection[0];

                // Can it have a material?
                if (auto * obj_as_mesh = dynamic_cast<StaticMesh *>(selected_object))
                {
                    inspectedObject = obj_as_mesh;
                    uint32_t mat_idx = 0;
                    for (std::string & name : materialNames)
                    {
                        if (obj_as_mesh->mat.name == name)
                        {
                            assetSelection = mat_idx;
                        }
                        mat_idx++;
                    }
                }
                else inspectedObject = nullptr;
            }
            else
            {
                inspectedObject = nullptr;
            }

            // A non-zero asset selection also means the preview mesh would have a valid material
            if (assetSelection > 0)
            {
                // Single-viewport camera
                previewSceneData = {};
                previewSceneData.clear_color = float4(0, 0, 0, 0);
                previewSceneData.renderSet.push_back(previewMesh.get());
                previewSceneData.ibl_irradianceCubemap = "wells-irradiance-cubemap"; // these handles could be cached
                previewSceneData.ibl_radianceCubemap = "wells-radiance-cubemap";
                previewSceneData.views.push_back(view_data(0, previewCam.pose, previewCam.get_projection_matrix(width / float(previewHeight))));
                preview_renderer->render_frame(previewSceneData);
            }

            glUseProgram(0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, previewHeight, width, height);

            auxImgui->begin_frame();
            gui::imgui_fixed_window_begin("material-editor", { { 0, 0 },{ width, int(height - previewHeight) } });

            ImGui::Dummy({ 0, 12 });
            ImGui::Text("Library: %s", lib.library_path.c_str());
            ImGui::Dummy({ 0, 12 });

            if (ImGui::Button(" " ICON_FA_PLUS " Create Material ", { 160, 24 })) ImGui::OpenPopup("Create Material");

            if (ImGui::BeginPopupModal("Create Material", NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Dummy({ 0, 6 });
                gui::InputText("Name", &stringBuffer);
                ImGui::Dummy({ 0, 6 });

                // Make a list of all the material types. (i.e. PBR, etc)
                std::vector<std::string> materialTypes;
                visit_subclasses((material_interface*)nullptr, [&materialTypes](const char * name, auto * p)
                {
                    materialTypes.push_back(name);
                });

                static int materialTypeSelection = -1;
                gui::Combo("Type", &materialTypeSelection, materialTypes);

                ImGui::Dummy({ 0, 6 });

                if (ImGui::Button("OK", ImVec2(120, 0)))
                {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::SetItemDefaultFocus();
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
                ImGui::EndPopup();
            }

            // Only draw the list of materials if there's no asset selected in the editor. 
            // This is a bit of a UX hack.
            if (!inspectedObject)
            {
                ImGui::Dummy({ 0, 12 });
                ImGuiTextFilter textFilter;
                textFilter.Draw(" " ICON_FA_SEARCH "  ");
                ImGui::Dummy({ 0, 12 });

                // Draw the listbox of materials
                draw_listbox<material_handle>("Materials", textFilter, assetSelection);
                ImGui::Dummy({ 0, 12 });
                ImGui::Separator();
            }

            if (assetSelection >= 0)
            {
                auto index_to_handle_name = [](const uint32_t index) -> std::string
                {
                    uint32_t idx = 0;
                    for (auto & m : asset_handle<std::shared_ptr<material_interface>>::list())
                    {
                        if (index == idx) return m.name;
                        idx++;
                    }
                };

                // This is by index
                auto mat = asset_handle<std::shared_ptr<material_interface>>::list()[assetSelection].get();
                const std::string material_handle_name = index_to_handle_name(assetSelection);
                previewMesh->mat = material_handle_name;

                ImGui::Text("Material: %s", material_handle_name.c_str());
                ImGui::Dummy({ 0, 12 });

                // Inspect
                inspect_object(nullptr, mat.get());

                ImGui::Dummy({ 0, 12 });

                // Shouldn't be able to delete the default material from the UI
                if (material_handle_name != material_library::kDefaultMaterialId)
                {
                    if (ImGui::Button(" " ICON_FA_TRASH " Delete Material "))
                    {
                        ImGui::OpenPopup("Delete Material");
                    }
                }
                ImGui::Dummy({ 0, 12 });

                if (ImGui::BeginPopupModal("Delete Material", NULL, ImGuiWindowFlags_AlwaysAutoResize))
                {
                    ImGui::Text("Are you sure you want \nto delete %s?", material_handle_name.c_str());

                    if (ImGui::Button("OK", ImVec2(120, 0)))
                    {
                        if (inspectedObject) inspectedObject->set_material(material_library::kDefaultMaterialId);
                        lib.remove_material(material_handle_name);
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::SetItemDefaultFocus();
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
                    ImGui::EndPopup();
                }

            }

            ImGui::Dummy({ 0, 12 });

            gui::imgui_fixed_window_end();
            auxImgui->end_frame();

            if (assetSelection > 0)
            {
                glViewport(0, 0, width, previewHeight);
                fullscreen_surface->draw(preview_renderer->get_color_texture(0));
            }

            glFlush();

            glfwSwapBuffers(window);
        }
    }

    ~material_editor_window()
    {
        if (window)
        {
            glfwDestroyWindow(window);
            window = nullptr;
        }
    }
};

#endif // end material_editor_hpp
