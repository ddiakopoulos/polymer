#pragma once

#ifndef material_editor_hpp
#define material_editor_hpp

#include "material.hpp"
#include "renderer-pbr.hpp"
#include "asset-handle-utils.hpp"
#include "gizmo-controller.hpp"
#include "environment.hpp"
#include "editor-inspector-ui.hpp"
#include "arcball.hpp"
#include "gl-texture-view.hpp"
#include "win32.hpp"

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
    std::unique_ptr<simple_texture_view> fullscreen_surface;
    std::unique_ptr<gui::imgui_instance> auxImgui;
    std::unique_ptr<arcball_controller> arcball;

    std::unique_ptr<pbr_renderer> preview_renderer;
    std::unique_ptr<material_component> material_comp;
    std::unique_ptr<mesh_component> mesh_comp;
    renderable preview_renderable;

    perspective_camera previewCam;

    std::string stringBuffer;
    int assetSelection = -1;
    const uint32_t previewHeight = 420;

    environment & scene;
    std::shared_ptr<gizmo_controller> selector;

    entity inspected_entity{ kInvalidEntity };
    entity debug_sphere{ kInvalidEntity };

    material_editor_window(gl_context * context,
        int w, int h,
        const std::string & title,
        int samples,
        environment & scene,
        std::shared_ptr<gizmo_controller> selector,
        entity_orchestrator & orch)
        : glfw_window(context, w, h, title, samples), scene(scene), selector(selector)
    {
        glfwMakeContextCurrent(window);

        fullscreen_surface.reset(new simple_texture_view());

        // Create debug sphere asset and assign to a handle
        // These are created on the this gl context and cached as global variables in the asset table. It will be re-assigned
        // every time this window is opened so we don't need to worry about cleaning up the handle asset.
        create_handle_for_asset("debug-sphere", make_mesh_from_geometry(make_icosasphere(3)));

        renderer_settings previewSettings;
        previewSettings.renderSize = int2(w, previewHeight);
        previewSettings.msaaSamples = 8;
        previewSettings.performanceProfiling = false;
        previewSettings.useDepthPrepass = false;
        previewSettings.tonemapEnabled = false;
        previewSettings.shadowsEnabled = false;

        preview_renderer.reset(new pbr_renderer(previewSettings));

        // Create a debug entity
        debug_sphere = orch.create_entity();

        mesh_comp.reset(new mesh_component(debug_sphere));
        mesh_comp->mesh = gpu_mesh_handle("debug-sphere");

        material_comp.reset(new material_component(debug_sphere));
        material_comp->material = material_handle(material_library::kDefaultMaterialId);

        preview_renderable.e = debug_sphere;
        preview_renderable.scale = float3(1.f, 1.f, 1.f);
        preview_renderable.t = transform();
        preview_renderable.material = material_comp.get();
        preview_renderable.mesh = mesh_comp.get();

        previewCam.pose = lookat_rh(float3(0, 0.25f, 2), float3(0, 0.001f, 0));
        auxImgui.reset(new gui::imgui_instance(window, true));

        std::cout << get_current_directory() << std::endl;

        auto fontAwesomeBytes = read_file_binary("../assets/fonts/font_awesome_4.ttf");
        auxImgui->append_icon_font(fontAwesomeBytes);

        arcball.reset(new arcball_controller({ (float)w, (float)h }));

        gui::make_light_theme();
    }

    virtual void on_input(const polymer::app_input_event & e) override final
    {
        if (e.window == window) auxImgui->update_input(e);
        if (ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard) return;

        if (e.type == app_input_event::MOUSE && e.is_down())
        {
            arcball->mouse_down(e.cursor);
        }
        else if (e.type == app_input_event::CURSOR && e.drag)
        {
            arcball->mouse_drag(e.cursor);
            preview_renderable.t.orientation = safe_normalize(qmul(arcball->currentQuat, preview_renderable.t.orientation));
        }
    }

    virtual void on_window_close() override final
    {
        glfwMakeContextCurrent(window);

        // Why do we do all these resets? Well, the way the gl_handle handle system works, the
        // destructor is called on the context of the destroyer. In the case of a secondary window,
        // that would be the main thread. Instead, we need to manually clean up everything here before
        // the destructor is called. 
        fullscreen_surface.reset();
        preview_renderer.reset();
        auxImgui.reset();

        gpu_mesh_handle::destroy("debug-sphere");

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

            const std::vector<entity> selected_entities = selector->get_selection();

            // Only one object->material can be edited at once
            if (selected_entities.size() == 1)
            {
                // Produce a list of material instance names. This could also be done by
                // iterating the keys of instances in the mat library, but using asset_handles is more canonical.
                std::vector<std::string> materialNames;
                for (auto & m : asset_handle<std::shared_ptr<material_interface>>::list()) materialNames.push_back(m.name);

                // Get an entity from the selection
                entity selected_entity = selected_entities[0];

                // We can only edit scene entities with a material component
                if (auto * mc = scene.render_system->get_material_component(selected_entity))
                {
                    inspected_entity = selected_entity;
                    uint32_t mat_idx = 0;
                    for (const std::string & name : materialNames)
                    {
                        const std::string name_candidate = mc->material.name;
                        if (name_candidate == name) assetSelection = mat_idx;
                        mat_idx++;
                    }
                }
                else
                {
                    inspected_entity = kInvalidEntity;
                    assetSelection = -1;
                }
            }

            // A non-zero asset selection also means the preview mesh would have a valid material
            if (assetSelection > 0)
            {
                // Construct ad-hoc payload
                render_payload preview_payload = {};
                preview_payload.clear_color = float4(0.25f, 0.25f, 0.25f, 1.f);
                preview_payload.ibl_irradianceCubemap = texture_handle("wells-irradiance-cubemap");
                preview_payload.ibl_radianceCubemap = texture_handle("wells-radiance-cubemap");
                preview_payload.views.push_back(view_data(0, previewCam.pose, previewCam.get_projection_matrix(width / float(previewHeight))));
                preview_payload.render_set.push_back(preview_renderable);

                // Render
                preview_renderer->render_frame(preview_payload);
            }

            glUseProgram(0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, previewHeight, width, height);

            auxImgui->begin_frame();
            gui::imgui_fixed_window_begin("material-editor", { { 0, 0 },{ width, int(height - previewHeight) } });

            ImGui::Dummy({ 0, 12 });

            if (ImGui::Button(" " ICON_FA_PLUS " Create Material ", { 160, 24 })) ImGui::OpenPopup("Create Material");
            ImGui::SameLine();
            if (ImGui::Button(" " ICON_FA_FILE " Save Materials ", { 160, 24 })) scene.mat_library->export_all();

            if (ImGui::BeginPopupModal("Create Material", NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Dummy({ 0, 6 });
                gui::InputText("Name", &stringBuffer);
                ImGui::Dummy({ 0, 6 });

                // Make a list of the material types (i.e. pbr, blinn-phong, etc)
                std::vector<std::string> materialTypes;
                visit_subclasses((material_interface*)nullptr, [&materialTypes](const char * name, auto * p)
                {
                    if (name != "polymer_default_material") materialTypes.push_back(name);
                });

                static int materialTypeSelection = -1;
                gui::Combo("Type", &materialTypeSelection, materialTypes);

                ImGui::Dummy({ 0, 6 });

                if (ImGui::Button("OK", ImVec2(120, 0)))
                {
                    if (!stringBuffer.empty())
                    {
                        if (materialTypeSelection == 0) // pbr
                        {
                            auto new_material = std::make_shared<polymer_pbr_standard>();
                            scene.mat_library->create_material(stringBuffer, new_material);
                        }
                        else if (materialTypeSelection == 1) // blinn-phong
                        {
                            auto new_material = std::make_shared<polymer_blinn_phong_standard>();
                            scene.mat_library->create_material(stringBuffer, new_material);
                        }
                        else if (materialTypeSelection == 2) // wireframe
                        {
                            auto new_material = std::make_shared<polymer_wireframe_material>();
                            scene.mat_library->create_material(stringBuffer, new_material);
                        }
                    }

                    stringBuffer.clear();

                    ImGui::CloseCurrentPopup();
                }

                ImGui::SetItemDefaultFocus();
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
                ImGui::EndPopup();
            }

            // Only draw the list of materials if there's no asset selected in the editor. 
            // This is a bit of a UX hack.
            if (!inspected_entity)
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
                    return {};
                };

                // Set the material on the preview mesh
                // This is by index. Future: might be easier if materials were entities too.
                std::shared_ptr<material_interface> mat = material_handle::list()[assetSelection].get();
                const std::string material_handle_name = index_to_handle_name(assetSelection);

                material_comp->material = material_handle(material_handle_name);
                assert(!material_handle_name.empty());

                ImGui::Text("Material: %s", material_handle_name.c_str());
                ImGui::Dummy({ 0, 12 });

                // Inspect
                inspect_material(mat.get());

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
                        if (inspected_entity)
                        {
                            // Set the inspected entity to the default material
                            auto mc = scene.render_system->get_material_component(inspected_entity);
                            mc->material = material_handle(material_library::kDefaultMaterialId);
                        }

                        // Set the preview entity to the default material
                        material_comp->material = material_handle(material_library::kDefaultMaterialId);

                        // This item must have been selected, so we must force unselect it
                        assetSelection = -1;

                        scene.mat_library->remove_material(material_handle_name);
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
