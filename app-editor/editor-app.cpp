#include "lib-polymer.hpp"
#include "system-util.hpp"
#include "editor-app.hpp"
#include "editor-inspector-ui.hpp"
#include "serialization.inl"
#include "logging.hpp"
#include "win32.hpp"
#include "model-io.hpp"
#include "renderer-util.hpp"

using namespace polymer;

// The Polymer editor has a number of "intrinsic" mesh assets that are loaded from disk at runtime. These primarily
// add to the number of objects that can be quickly prototyped with, along with the usual set of procedural mesh functions
// included with Polymer.
void load_editor_intrinsic_assets(path root)
{
    scoped_timer t("load_editor_intrinsic_assets");
    for (auto & entry : recursive_directory_iterator(root))
    {
        const size_t root_len = root.string().length(), ext_len = entry.path().extension().string().length();
        auto path = entry.path().string(), name = path.substr(root_len + 1, path.size() - root_len - ext_len - 1);
        for (auto & chr : path) if (chr == '\\') chr = '/';

        if (entry.path().extension().string() == ".mesh")
        {
            auto geo_import = import_mesh_binary(path);
            create_handle_for_asset(std::string("poly-" + get_filename_without_extension(path)).c_str(), make_mesh_from_geometry(geo_import));
            create_handle_for_asset(std::string("poly-" + get_filename_without_extension(path)).c_str(), std::move(geo_import));
        }
    }
};

scene_editor_app::scene_editor_app() : polymer_app(1920, 1080, "Polymer Editor")
{
    working_dir_on_launch = get_current_directory();

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    log::get()->replace_sink(std::make_shared<ImGui::spdlog_editor_sink>(log));

    auto droidSansTTFBytes = read_file_binary("../assets/fonts/droid_sans.ttf");

    igm.reset(new gui::imgui_instance(window));
    gui::make_light_theme();
    igm->add_font(droidSansTTFBytes);

    cam.look_at({ 0, 9.5f, -6.0f }, { 0, 0.1f, 0 });
    cam.farclip = 256;
    flycam.set_camera(&cam);

    load_editor_intrinsic_assets("../assets/models/runtime/");
    load_required_renderer_assets("../assets", shaderMonitor);

    fullscreen_surface.reset(new simple_texture_view());

    renderer_settings initialSettings;
    initialSettings.renderSize = int2(width, height);
    scene.collision_system = orchestrator.create_system<collision_system>(&orchestrator);
    scene.xform_system = orchestrator.create_system<transform_system>(&orchestrator);
    scene.identifier_system = orchestrator.create_system<identifier_system>(&orchestrator);
    scene.render_system = orchestrator.create_system<render_system>(initialSettings, &orchestrator);

    gizmo.reset(new gizmo_controller(scene.xform_system));

    // Only need to set the skybox on the |render_payload| once (unless we clear the payload)
    the_render_payload.skybox = scene.render_system->get_skybox();
    the_render_payload.sunlight = scene.render_system->get_implicit_sunlight();

    // fixme to be resolved
    auto radianceBinary = read_file_binary("../assets/textures/envmaps/wells_radiance.dds");
    auto irradianceBinary = read_file_binary("../assets/textures/envmaps/wells_irradiance.dds");
    gli::texture_cube radianceHandle(gli::load_dds((char *)radianceBinary.data(), radianceBinary.size()));
    gli::texture_cube irradianceHandle(gli::load_dds((char *)irradianceBinary.data(), irradianceBinary.size()));
    create_handle_for_asset("wells-radiance-cubemap", load_cubemap(radianceHandle));
    create_handle_for_asset("wells-irradiance-cubemap", load_cubemap(irradianceHandle));

    the_render_payload.ibl_irradianceCubemap = texture_handle("wells-irradiance-cubemap");
    the_render_payload.ibl_radianceCubemap = texture_handle("wells-radiance-cubemap");

    scene.mat_library.reset(new polymer::material_library("../assets/materials.json"));

    // Resolve asset_handles to resources on disk
    resolver.reset(new asset_resolver());
    resolver->resolve("../assets/", &scene, scene.mat_library.get());
}

void scene_editor_app::on_drop(std::vector<std::string> filepaths)
{
    for (auto & path : filepaths)
    {
        std::transform(path.begin(), path.end(), path.begin(), ::tolower);
        const std::string fileExtension = get_extension(path);

        if (fileExtension == "png" || fileExtension == "tga" || fileExtension == "jpg")
        {
            create_handle_for_asset(get_filename_without_extension(path).c_str(), load_image(path, false));
            continue;
        }

        auto importedModel = import_model(path); // fbx and obj

        for (auto & m : importedModel)
        {
            auto & mesh = m.second;
            rescale_geometry(mesh, 1.f);

            if (mesh.normals.size() == 0) compute_normals(mesh);
            if (mesh.tangents.size() == 0) compute_tangents(mesh);
                
            const std::string filename = get_filename_without_extension(path);
            const std::string outputBasePath = "../assets/models/runtime/";
            const std::string outputFile = outputBasePath + filename + "-" + m.first + "-" + ".mesh";

            export_mesh_binary(outputFile, mesh, false);

            auto importedMesh = import_mesh_binary(outputFile);

            create_handle_for_asset(std::string(get_filename_without_extension(path) + "-" + m.first).c_str(), make_mesh_from_geometry(importedMesh));
            create_handle_for_asset(std::string(get_filename_without_extension(path) + "-" + m.first).c_str(), std::move(importedMesh));
        }
    }
}

void scene_editor_app::on_window_resize(int2 size) 
{ 
    // Iconification/minimization triggers an on_window_resize event with a zero size
    if (size.x > 0 && size.y > 0)
    {
        renderer_settings settings;
        settings.renderSize = size;
        scene.render_system->reconfigure(settings);
        scene.render_system->get_skybox()->onParametersChanged(); // reconfigure directional light
    }
}

void scene_editor_app::on_input(const app_input_event & event)
{
    igm->update_input(event);
    gizmo->on_input(event);

    if (ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard)
    {
        flycam.reset();
        gizmo->reset_input();
        return;
    }

    // flycam only works when a mod key isn't held down
    if (event.mods == 0) flycam.handle_input(event);

    {
        if (event.type == app_input_event::KEY)
        {
            // De-select all objects
            if (event.value[0] == GLFW_KEY_ESCAPE && event.action == GLFW_RELEASE)
            {
                gizmo->clear();
            }

            // Focus on currently selected object
            if (event.value[0] == GLFW_KEY_F && event.action == GLFW_RELEASE)
            {
                if (gizmo->get_selection().size() == 0) return;
                if (entity theSelection = gizmo->get_selection()[0])
                {
                    const transform selectedObjectPose = scene.xform_system->get_world_transform(theSelection)->world_pose;
                    const float3 focusOffset = selectedObjectPose.position + float3(0.f, 0.5f, 4.f);
                    cam.look_at(focusOffset, selectedObjectPose.position);
                    flycam.update_yaw_pitch();
                }
            }

            // Togle drawing ImGUI
            if (event.value[0] == GLFW_KEY_TAB && event.action == GLFW_RELEASE)
            {
                show_imgui = !show_imgui;
            }

            if (event.value[0] == GLFW_KEY_SPACE && event.action == GLFW_RELEASE)
            {

            }
        }

        // Raycast for editor/gizmo selection on mouse up
        if (event.type == app_input_event::MOUSE && event.action == GLFW_RELEASE && event.value[0] == GLFW_MOUSE_BUTTON_LEFT)
        {
            int width, height;
            glfwGetWindowSize(window, &width, &height);

            const ray r = cam.get_world_ray(event.cursor, float2(static_cast<float>(width), static_cast<float>(height)));

            if (length(r.direction) > 0 && !gizmo->active())
            {
                std::vector<entity> selectedObjects;
                entity_hit_result result = scene.collision_system->raycast(r);
                if (result.e != kInvalidEntity) selectedObjects.push_back(result.e);

                // New object was selected
                if (selectedObjects.size() > 0)
                {
                    // Multi-selection
                    if (event.mods & GLFW_MOD_CONTROL)
                    {
                        auto existingSelection = gizmo->get_selection();
                        for (auto s : selectedObjects)
                        {
                            if (!gizmo->selected(s)) existingSelection.push_back(s);
                        }
                        gizmo->set_selection(existingSelection);
                    }
                    // Single Selection
                    else
                    {
                        gizmo->set_selection(selectedObjects);
                    }
                }
            }

        }
    }
}

void scene_editor_app::open_material_editor()
{
    if (!material_editor)
    {
       material_editor.reset(new material_editor_window(get_shared_gl_context(), 500, 1200, "", 1, scene, gizmo, orchestrator));
    }
    else if (!material_editor->get_window())
    {
        // Workaround since there's no convenient way to reset the material_editor when it's been closed
        material_editor.reset(new material_editor_window(get_shared_gl_context(), 500, 1200, "", 1, scene, gizmo, orchestrator));
    }

    glfwMakeContextCurrent(window);
}

void scene_editor_app::on_update(const app_update_event & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    set_working_directory(working_dir_on_launch);

    editorProfiler.begin("on_update");
    flycam.update(e.timestep_ms);
    shaderMonitor.handle_recompile();
    gizmo->on_update(cam, float2(static_cast<float>(width), static_cast<float>(height)));
    editorProfiler.end("on_update");
}

void scene_editor_app::on_draw()
{
    glfwMakeContextCurrent(window);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const float4x4 projectionMatrix = cam.get_projection_matrix(float(width) / float(height));
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);

    {
        editorProfiler.begin("gather-scene");

        // Clear out transient scene payload data
        the_render_payload.views.clear();
        the_render_payload.render_set.clear();
        the_render_payload.point_lights.clear();
        the_render_payload.sunlight = nullptr;

        // Does the entity have a material? If so, we can render it. 
        for (const auto e : scene.entity_list())
        {
            if (material_component * mat_c = scene.render_system->get_material_component(e))
            {
                auto mesh_c = scene.render_system->get_mesh_component(e);
                if (!mesh_c) continue; // for the case that we just created a material component and haven't set a mesh yet

                auto xform_c = scene.xform_system->get_world_transform(e);
                assert(xform_c != nullptr); // we did something bad if this entity doesn't also have a world transform component

                auto scale_c = scene.xform_system->get_local_transform(e);
                assert(scale_c != nullptr); 

                renderable r;
                r.e = e;
                r.material = mat_c;
                r.mesh = mesh_c;
                r.t = xform_c->world_pose;
                r.scale = scale_c->local_scale;
                the_render_payload.render_set.push_back(r);
            }
        }

        // Gather directional lights
        for (const auto e : scene.entity_list())
        {
            if (auto dir_light_c = scene.render_system->get_directional_light_component(e)) the_render_payload.sunlight = dir_light_c;
        }

        // Gather point lights
        for (const auto e : scene.entity_list())
        {
            if (auto pt_light_c = scene.render_system->get_point_light_component(e)) the_render_payload.point_lights.push_back(pt_light_c);
        }

        // Add single-viewport camera
        the_render_payload.views.push_back(view_data(0, cam.pose, projectionMatrix));

        editorProfiler.end("gather-scene");

        // Submit scene to the scene renderer
        editorProfiler.begin("submit-scene");
        scene.render_system->get_renderer()->render_frame(the_render_payload);
        editorProfiler.end("submit-scene");

        // Draw to screen framebuffer
        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        fullscreen_surface->draw(scene.render_system->get_renderer()->get_color_texture(0));

        if (show_grid)
        {
            grid.draw(viewProjectionMatrix, Identity4x4, float4(1, 1, 1, 0.25f));
        }
        gl_check_error(__FILE__, __LINE__);
    }

    // Draw selected objects as wireframe by directly
    editorProfiler.begin("wireframe-rendering");
    {
        glDisable(GL_DEPTH_TEST);

        gl_shader & program = wireframeHandle.get()->get_variant()->shader;

        program.bind();
        program.uniform("u_eyePos", cam.get_eye_point());
        program.uniform("u_viewProjMatrix", viewProjectionMatrix);
        for (const entity e : gizmo->get_selection())
        {
            const transform p = scene.xform_system->get_world_transform(e)->world_pose;
            const float3 scale = scene.xform_system->get_local_transform(e)->local_scale;
            const float4x4 modelMatrix = mul(p.matrix(), make_scaling_matrix(scale));
            program.uniform("u_modelMatrix", modelMatrix);
            if (auto mesh = scene.render_system->get_mesh_component(e))
            {
                mesh->draw();
            }
        }
        program.unbind();

        glEnable(GL_DEPTH_TEST);
    }
    editorProfiler.end("wireframe-rendering");

    editorProfiler.begin("imgui-menu");
    igm->begin_frame();

    gui::imgui_menu_stack menu(*this, ImGui::GetIO().KeysDown);
    menu.app_menu_begin();
    {
        menu.begin("File");
        bool mod_enabled = !gizmo->active();
        if (menu.item("Open Scene", GLFW_MOD_CONTROL, GLFW_KEY_O, mod_enabled))
        {
            const auto import_path = windows_file_dialog("polymer scene", "json", true);
            if (!import_path.empty())
            {
                scene.destroy(kAllEntities);
                gizmo->clear();
                the_render_payload.render_set.clear();
                scene.import_environment(import_path, orchestrator);
                glfwSetWindowTitle(window, import_path.c_str());
            }
        }

        if (menu.item("Save Scene", GLFW_MOD_CONTROL, GLFW_KEY_S, mod_enabled))
        {
            const auto export_path = windows_file_dialog("polymer scene", "json", false);
            if (!export_path.empty())
            {
                gizmo->clear();
                the_render_payload.render_set.clear();
                scene.export_environment(export_path);
                glfwSetWindowTitle(window, export_path.c_str());
            }
        }

        if (menu.item("New Scene", GLFW_MOD_CONTROL, GLFW_KEY_N, mod_enabled))
        {
            gizmo->clear();
            scene.destroy(kAllEntities);
            the_render_payload.render_set.clear();
            glfwSetWindowTitle(window, "unsaved new scene");
        }

        if (menu.item("Take Screenshot", GLFW_MOD_CONTROL, GLFW_KEY_EQUAL, mod_enabled))
        {
            request_screenshot("scene-editor");
        }

        if (menu.item("Exit", GLFW_MOD_ALT, GLFW_KEY_F4)) exit();
        menu.end();

        menu.begin("Edit");
        if (menu.item("Clone", GLFW_MOD_CONTROL, GLFW_KEY_D)) 
        {
            const auto selection_list = gizmo->get_selection();
            if (!selection_list.empty() && selection_list[0] != kInvalidEntity)
            {
                const entity the_copy = scene.track_entity(orchestrator.create_entity());
                scene.copy(selection_list[0], the_copy);
                std::vector<entity> new_selection_list = { the_copy };
                gizmo->set_selection(new_selection_list);
            }
        }
        if (menu.item("Delete", 0, GLFW_KEY_DELETE)) 
        {
            const auto selection_list = gizmo->get_selection();
            if (!selection_list.empty() && selection_list[0] != kInvalidEntity) scene.destroy(selection_list[0]);
            gizmo->clear();
        }
        if (menu.item("Select All", GLFW_MOD_CONTROL, GLFW_KEY_A)) 
        {
            gizmo->set_selection(scene.entity_list());
        }
        menu.end();

        menu.begin("Create");
        if (menu.item("entity")) 
        {
            std::vector<entity> list = { scene.track_entity(orchestrator.create_entity()) };
            scene.xform_system->create(list[0], transform());
            scene.identifier_system->create(list[0], "new entity (" + std::to_string(list[0]) + ")");
            gizmo->set_selection(list); // Newly spawned objects are selected by default
        }
        menu.end();

        menu.begin("Windows");
        if (menu.item("Material Editor", GLFW_MOD_CONTROL, GLFW_KEY_M))
        {
            should_open_material_window = true;
        }
        menu.end();
    }

    menu.app_menu_end();

    editorProfiler.end("imgui-menu");

    editorProfiler.begin("imgui-editor");
    if (show_imgui)
    {
        static int horizSplit = 380;
        static int rightSplit1 = (height / 2) - 17;

        // Define a split region between the whole window and the right panel
        auto rightRegion = ImGui::Split({ { 0.f ,17.f },{ (float)width, (float)height } }, &horizSplit, ImGui::SplitType::Right);
        auto split2 = ImGui::Split(rightRegion.second, &rightSplit1, ImGui::SplitType::Top);

        ui_rect topRightPane = { { int2(split2.second.min()) }, { int2(split2.second.max()) } };  // top half
        ui_rect bottomRightPane = { { int2(split2.first.min()) },{ int2(split2.first.max()) } };  // bottom half

        gui::imgui_fixed_window_begin("Inspector", topRightPane);

        if (gizmo->get_selection().size() >= 1)
        {
            ImGui::Dummy({ 0, 8 });
            if (ImGui::Button(" Add Component ", { 260, 20 })) ImGui::OpenPopup("Create Component");
            ImGui::Dummy({ 0, 8 });

            gizmo->refresh(); // selector only stores data, not pointers, so we need to recalc new xform.
            inspect_entity(nullptr, gizmo->get_selection()[0], scene);

            if (ImGui::BeginPopupModal("Create Component", NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                const entity selection = gizmo->get_selection()[0];

                ImGui::Dummy({ 0, 6 });

                std::vector<std::string> component_names;
                enumerate_components([&](const char * name, poly_typeid type) { component_names.push_back(name); });

                static int componentTypeSelection = -1;
                gui::Combo("Component", &componentTypeSelection, component_names);

                ImGui::Dummy({ 0, 6 });

                if (ImGui::Button("OK", ImVec2(120, 0)))
                {
                    if (componentTypeSelection == -1)ImGui::CloseCurrentPopup();

                    const std::string type_name = component_names[componentTypeSelection];

                    visit_systems(&scene, [&](const char * system_name, auto * system_pointer)
                    {
                        if (system_pointer)
                        {
                            if      (type_name == get_typename<identifier_component>()) system_pointer->create(selection, get_typeid<identifier_component>(), &identifier_component(selection));
                            else if (type_name == get_typename<scene_graph_component>()) system_pointer->create(selection, get_typeid<scene_graph_component>(), &scene_graph_component(selection));
                            else if (type_name == get_typename<mesh_component>()) system_pointer->create(selection, get_typeid<mesh_component>(), &mesh_component(selection));
                            else if (type_name == get_typename<material_component>()) system_pointer->create(selection, get_typeid<material_component>(), &material_component(selection));
                            else if (type_name == get_typename<geometry_component>()) system_pointer->create(selection, get_typeid<geometry_component>(), &geometry_component(selection));
                            else if (type_name == get_typename<point_light_component>()) system_pointer->create(selection, get_typeid<point_light_component>(), &point_light_component(selection));
                            else if (type_name == get_typename<directional_light_component>()) system_pointer->create(selection, get_typeid<directional_light_component>(), &directional_light_component(selection));
                        }
                    });

                    ImGui::CloseCurrentPopup();
                }

                ImGui::SetItemDefaultFocus();
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
                ImGui::EndPopup();
            }

        }
        gui::imgui_fixed_window_end();

        gui::imgui_fixed_window_begin("Scene Entities", bottomRightPane);
        for (auto entity : scene.entity_list())
        {
            ImGui::PushID(static_cast<int>(entity));

            bool selected = gizmo->selected(entity);
            std::string name = scene.identifier_system->get_name(entity);
            name = name.empty() ? "entity": name;

            if (ImGui::Selectable(name.c_str(), &selected))
            {
                if (!ImGui::GetIO().KeyCtrl) gizmo->clear();
                gizmo->update_selection(entity);
            }
            ImGui::PopID();
        }
        gui::imgui_fixed_window_end();

        // Define a split region between the whole window and the left panel
        static int leftSplit = 380;
        static int leftSplit1 = (height / 2);
        auto leftRegionSplit = ImGui::Split({ { 0.f,17.f },{ (float)width, (float)height } }, &leftSplit, ImGui::SplitType::Left);
        auto lsplit2 = ImGui::Split(leftRegionSplit.second, &leftSplit1, ImGui::SplitType::Top);
        ui_rect topLeftPane = { { int2(lsplit2.second.min()) },{ int2(lsplit2.second.max()) } };
        ui_rect bottomLeftPane = { { int2(lsplit2.first.min()) },{ int2(lsplit2.first.max()) } };

        gui::imgui_fixed_window_begin("Renderer", topLeftPane);
        {
            ImGui::Dummy({ 0, 10 });

            ImGui::Checkbox("Show Floor Grid", &show_grid);
            ImGui::Dummy({ 0, 10 });

            if (ImGui::TreeNode("Core"))
            {
                renderer_settings lastSettings = scene.render_system->get_renderer()->settings;

                if (build_imgui("renderer", *scene.render_system->get_renderer()))
                {
                    scene.render_system->get_renderer()->gpuProfiler.set_enabled(scene.render_system->get_renderer()->settings.performanceProfiling);
                    scene.render_system->get_renderer()->cpuProfiler.set_enabled(scene.render_system->get_renderer()->settings.performanceProfiling);
                }

                ImGui::TreePop();
            }

            ImGui::Dummy({ 0, 10 });

            if (ImGui::TreeNode("Procedural Sky") && the_render_payload.skybox)
            {
                //build_imgui("skybox", *the_render_payload.skybox);
                ImGui::TreePop();
            }

            ImGui::Dummy({ 0, 10 });

            if (auto * shadows = scene.render_system->get_renderer()->get_shadow_pass())
            {
                if (ImGui::TreeNode("Cascaded Shadow Mapping"))
                {
                    build_imgui("shadows", *shadows);
                    ImGui::TreePop();
                }
            }

            ImGui::Dummy({ 0, 10 });

            if (scene.render_system->get_renderer()->settings.performanceProfiling)
            {
                for (auto & t : scene.render_system->get_renderer()->gpuProfiler.get_data()) ImGui::Text("[Renderer GPU] %s %f ms", t.first.c_str(), t.second);
                for (auto & t : scene.render_system->get_renderer()->cpuProfiler.get_data()) ImGui::Text("[Renderer CPU] %s %f ms", t.first.c_str(), t.second);
            }

            ImGui::Dummy({ 0, 10 });

            for (auto & t : editorProfiler.get_data()) ImGui::Text("[Editor] %s %f ms", t.first.c_str(), t.second);
        }
        gui::imgui_fixed_window_end();

        gui::imgui_fixed_window_begin("Application Log", bottomLeftPane);
        {
            log.Draw("-");
        }
        gui::imgui_fixed_window_end();
    }

    igm->end_frame();
    editorProfiler.end("imgui-editor");

    {
        editorProfiler.begin("gizmo_on_draw");
        glClear(GL_DEPTH_BUFFER_BIT);
        gizmo->on_draw();
        editorProfiler.end("gizmo_on_draw");
    }

    gl_check_error(__FILE__, __LINE__);

    glFlush();

    // `should_open_material_window` flag required because opening a new window directly 
    // from an ImGui instance trashes some piece of state somewhere
    if (should_open_material_window)
    {
        should_open_material_window = false;
        open_material_editor();
    }

    if (material_editor && material_editor->get_window()) material_editor->run();
    glfwSwapBuffers(window);
}

IMPLEMENT_MAIN(int argc, char * argv[])
{
    try
    {
        scene_editor_app app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        std::cerr << "Application Fatal: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
