#include "lib-polymer.hpp"
#include "system-util.hpp"
#include "editor-app.hpp"
#include "editor-inspector-ui.hpp"
#include "serialization.hpp"
#include "logging.hpp"
#include "util-win32.hpp"
#include "model-io.hpp"
#include "renderer-util.hpp"
#include "renderer-debug.hpp"

using namespace polymer;

scene_editor_app::scene_editor_app() : polymer_app(1920, 1080, "Polymer Editor")
{
    working_dir_on_launch = get_current_directory();

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    log::get()->set_engine_logger(std::make_shared<ImGui::spdlog_editor_sink>(editor_console_log));

    auto droidSansTTFBytes = read_file_binary("../assets/fonts/droid_sans.ttf");

    igm.reset(new gui::imgui_instance(window));
    gui::make_light_theme();
    igm->add_font(droidSansTTFBytes);

    cam.look_at({ 0, 5.f, -5.f }, { 0, 3.5f, 0 });
    cam.farclip = 24;
    flycam.set_camera(&cam);

    load_required_renderer_assets("../assets", shaderMonitor);

    shaderMonitor.watch("wireframe",
       "../assets/shaders/wireframe_vert.glsl",
       "../assets/shaders/wireframe_frag.glsl",
       "../assets/shaders/wireframe_geom.glsl",
       "../assets/shaders/renderer");

    fullscreen_surface.reset(new simple_texture_view());

    the_scene.reset(the_entity_system_manager, { width, height }, true);

    global_debug_mesh_manager::get()->initialize_resources(&the_entity_system_manager, &the_scene);

    gizmo.reset(new gizmo_controller(&the_scene));
}

void scene_editor_app::on_drop(std::vector<std::string> filepaths)
{
    for (const auto & path : filepaths)
    {
        const std::string ext = get_extension(path);
        if (ext == "json") import_scene(path);
        else import_asset_runtime(path, the_scene, the_entity_system_manager);
    }
}

void scene_editor_app::on_window_resize(int2 size) 
{ 
    // Iconification/minimization triggers an on_window_resize event with a zero size
    if (size.x > 0 && size.y > 0)
    {
        renderer_settings settings;
        settings.renderSize = size;
        the_scene.render_system->reconfigure(settings);
        // scene.render_system->get_procedural_skybox_component()->onParametersChanged(); // @tofix - reconfigure directional light
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
                    const transform selectedObjectPose = the_scene.xform_system->get_world_transform(theSelection)->world_pose;
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

            if (event.value[0] == GLFW_KEY_SPACE && event.action == GLFW_RELEASE) {}

            // XZ plane nudging
            if (event.action == GLFW_RELEASE)
            {
                auto nudge = [this](const float3 amount)
                {
                    if (gizmo->get_selection().size() == 0) return;
                    if (const entity first_selection = gizmo->get_selection()[0])
                    {
                        auto transform = the_scene.xform_system->get_local_transform(first_selection)->local_pose;
                        transform.position += amount;
                        the_scene.xform_system->set_local_transform(first_selection, transform);
                    }
                };

                if (event.value[0] == GLFW_KEY_UP)    nudge({  0.25f, 0.f,  0.f });
                if (event.value[0] == GLFW_KEY_DOWN)  nudge({ -0.25f, 0.f,  0.f });
                if (event.value[0] == GLFW_KEY_LEFT)  nudge({  0.f,   0.f,  0.25f });
                if (event.value[0] == GLFW_KEY_RIGHT) nudge({  0.f,   0.f, -0.25f });
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
                entity_hit_result result = the_scene.collision_system->raycast(r);
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

            if (gizmo->moved())
            {
                the_scene.collision_system->queue_acceleration_rebuild();
            }
        }
    }
}

void scene_editor_app::import_scene(const std::string & path)
{
    if (!path.empty())
    {
        gizmo->clear();
        renderer_payload.reset();

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        the_scene.reset(the_entity_system_manager, { width, height }, false); // don't create implicit objects if importing

        the_scene.import_environment(path, the_entity_system_manager);
       
        // Resolve polymer-local assets
        the_scene.resolver->add_search_path("../assets/");

        // Resolve project assets
        const auto parent_dir = parent_directory_from_filepath(path);
        log::get()->engine_log->info("resolving local `{}` directory.", parent_dir);
        the_scene.resolver->add_search_path(parent_dir);

        the_scene.resolver->resolve();

        glfwSetWindowTitle(window, path.c_str());
    }
    else
    {
        throw std::invalid_argument("path was empty...?");
    }

    global_debug_mesh_manager::get()->initialize_resources(&the_entity_system_manager, &the_scene);
}

void scene_editor_app::open_material_editor()
{
    material_editor.reset(new material_editor_window(get_shared_gl_context(), 500, 1200, "", 1, the_scene, gizmo, the_entity_system_manager));
    glfwMakeContextCurrent(window);
}

void scene_editor_app::open_asset_browser()
{
    asset_browser.reset(new asset_browser_window(get_shared_gl_context(), 800, 400, "assets", 1));
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

void scene_editor_app::draw_entity_scenegraph(const entity e)
{
    if (e == kInvalidEntity || !the_scene.xform_system->has_transform(e))
    {
        throw std::invalid_argument("this entity is invalid or someone deleted its transform (bad)");
    }

    bool open = false;

    ImGui::PushID(static_cast<int>(e));

    // Has a transform system entry
    if (auto * xform = the_scene.xform_system->get_local_transform(e))
    {
        // Check if this has children
        if (xform->children.size())
        {
            // Increase spacing to differentiate leaves from expanded contents.
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ImGui::GetFontSize());
            ImGui::SetNextTreeNodeOpen(true, ImGuiCond_FirstUseEver);
            open = ImGui::TreeNode("");
            if (!open) ImGui::PopStyleVar();
            ImGui::SameLine();
        }
    }

    const bool selected = gizmo->selected(e);
    std::string name = the_scene.identifier_system->get_name(e);
    std::string entity_as_string = "[" + std::to_string(e) + "] ";
    name = name.empty() ? entity_as_string + "<unnamed entity>" : entity_as_string + name;

    if (ImGui::Selectable(name.c_str(), selected))
    {
        if (!ImGui::GetIO().KeyCtrl) gizmo->clear();
        gizmo->update_selection(e);
    }

    if (open)
    {
        // Has a transform system entry
        if (auto * xform = the_scene.xform_system->get_local_transform(e))
        {
            for (auto & c : xform->children)
            {
                draw_entity_scenegraph(c);
            }
            ImGui::PopStyleVar();
            ImGui::Unindent(ImGui::GetFontSize());
            ImGui::TreePop();
        }
    }

    ImGui::PopID();
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

    global_debug_mesh_manager::get()->upload();
    global_debug_mesh_manager::get()->clear();

    const float4x4 projectionMatrix = cam.get_projection_matrix(float(width) / float(height));
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = (projectionMatrix * viewMatrix);

    {
        editorProfiler.begin("gather-scene");

        // Clear out transient scene payload data
        renderer_payload.reset();

        // Does the entity have a material? If so, we can render it. 
        for (const auto e : the_scene.entity_list())
        {
            if (material_component * mat_c = the_scene.render_system->get_material_component(e))
            {
                auto mesh_c = the_scene.render_system->get_mesh_component(e);
                if (!mesh_c) continue; // for the case that we just created a material component and haven't set a mesh yet

                auto xform_c = the_scene.xform_system->get_world_transform(e);
                assert(xform_c != nullptr); // we did something bad if this entity doesn't also have a world transform component

                auto scale_c = the_scene.xform_system->get_local_transform(e);
                assert(scale_c != nullptr); 

                render_component r = assemble_render_component(the_scene, e);
                renderer_payload.render_components.push_back(r);
            }
        }

        // Only one IBL
        for (auto & e : the_scene.entity_list())
        {
            if (auto * cubemap = the_scene.render_system->get_cubemap_component(e))
            {
                renderer_payload.ibl_cubemap = cubemap;
            }
        }

        // Only one skybox
        for (auto & e : the_scene.entity_list())
        {
            if (auto * proc_skybox = the_scene.render_system->get_procedural_skybox_component(e))
            {
                renderer_payload.procedural_skybox = proc_skybox;
                if (auto sunlight = the_scene.render_system->get_directional_light_component(proc_skybox->sun_directional_light))
                {
                    renderer_payload.sunlight = sunlight;
                }
            }
        }

        // Gather point lights
        for (const auto e : the_scene.entity_list())
        {
            if (auto pt_light_c = the_scene.render_system->get_point_light_component(e))
            {
                renderer_payload.point_lights.push_back(pt_light_c);
            }
        }

        const auto debug_renderer_entity = assemble_render_component(the_scene, global_debug_mesh_manager::get()->get_entity());
        if (debug_renderer_entity.get_entity() != kInvalidEntity) renderer_payload.render_components.push_back(debug_renderer_entity);

        // Add single-viewport camera
        renderer_payload.views.push_back(view_data(0, cam.pose, projectionMatrix));

        editorProfiler.end("gather-scene");

        // Submit scene to the scene renderer
        editorProfiler.begin("submit-scene");
        the_scene.render_system->get_renderer()->render_frame(renderer_payload);
        editorProfiler.end("submit-scene");

        // Draw to screen framebuffer
        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        fullscreen_surface->draw(the_scene.render_system->get_renderer()->get_color_texture(0));

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
            const transform p = the_scene.xform_system->get_world_transform(e)->world_pose;
            const float3 scale = the_scene.xform_system->get_local_transform(e)->local_scale;
            const float4x4 modelMatrix = (p.matrix() * make_scaling_matrix(scale));
            program.uniform("u_modelMatrix", modelMatrix);
            if (auto mesh = the_scene.render_system->get_mesh_component(e))
            {
                mesh->draw();
            }
        }
        program.unbind();

        glEnable(GL_DEPTH_TEST);
    }
    editorProfiler.end("wireframe-rendering");

    // Render the gizmo behind imgui
    {
        editorProfiler.begin("gizmo_on_draw");
        glClear(GL_DEPTH_BUFFER_BIT);
        gizmo->on_draw(32.f); // set the gizmo to a fixed pixel size
        editorProfiler.end("gizmo_on_draw");
    }

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
            set_working_directory(working_dir_on_launch); // required because the dialog resets the cwd
            import_scene(import_path);
            currently_open_scene = import_path;
        }

        if (menu.item("Save Scene", GLFW_MOD_CONTROL, GLFW_KEY_S, mod_enabled))
        {
            if (currently_open_scene == "New Scene")
            {
                const auto export_path = windows_file_dialog("polymer scene", "json", false);
                set_working_directory(working_dir_on_launch); // required because the dialog resets the cwd
                if (!export_path.empty())
                {
                    //gizmo->clear();
                    renderer_payload.render_components.clear();
                    the_scene.export_environment(export_path);
                    glfwSetWindowTitle(window, export_path.c_str());

                    auto scene_name = get_filename_without_extension(export_path);
                    currently_open_scene = export_path;
                }
            }
            else
            {
                if (file_exists(currently_open_scene)) // ensure that path via save-as or open is valid
                {
                    the_scene.export_environment(currently_open_scene);
                }
            }
        }

        if (menu.item("New Scene", GLFW_MOD_CONTROL, GLFW_KEY_N, mod_enabled))
        {
            gizmo->clear();

            the_scene.reset(the_entity_system_manager, { width, height }, true);

            renderer_payload.render_components.clear();
            glfwSetWindowTitle(window, "New Scene");
            currently_open_scene = "New Scene";
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
                const entity the_copy = the_scene.track_entity(the_entity_system_manager.create_entity());
                the_scene.copy(selection_list[0], the_copy);

                // offset cloned object by 0.1 units
                auto old_local_xform = the_scene.xform_system->get_local_transform(the_copy);
                transform t = old_local_xform->local_pose;
                t.position += 0.1f;

                the_scene.xform_system->set_local_transform(the_copy, t);

                std::vector<entity> new_selection_list = { the_copy };
                gizmo->set_selection(new_selection_list);
            }
        }
        if (menu.item("Delete", 0, GLFW_KEY_DELETE)) 
        {
            const auto selection_list = gizmo->get_selection();
            if (!selection_list.empty() && selection_list[0] != kInvalidEntity) 
            {
                the_scene.destroy(selection_list[0]);
            }
            gizmo->clear();
        }
        if (menu.item("Select All", GLFW_MOD_CONTROL, GLFW_KEY_A)) 
        {
            gizmo->set_selection(the_scene.entity_list());
        }
        menu.end();

        menu.begin("Create");
        if (menu.item("basic entity")) 
        {
            std::vector<entity> list = { the_scene.track_entity(the_entity_system_manager.create_entity()) };
            the_scene.xform_system->create(list[0], transform());
            the_scene.identifier_system->create(list[0], "new entity (" + std::to_string(list[0]) + ")");
            gizmo->set_selection(list); // Newly spawned objects are selected by default
        }
        if (menu.item("renderable entity"))
        {
            const std::string name = "new renderable entity";
            const entity e = make_standard_scene_object(&the_entity_system_manager, &the_scene,
                name, {}, float3(1.f), material_handle(material_library::kDefaultMaterialId), "cube-uniform", "cube-uniform");
            std::vector<entity> list = { e };
            gizmo->set_selection(list);
        }
        menu.end();

        menu.begin("Windows");
        if (menu.item("Material Editor", GLFW_MOD_CONTROL, GLFW_KEY_M))
        {
            should_open_material_window = true;
        }
        else if (menu.item("Asset Browser", GLFW_MOD_CONTROL, GLFW_KEY_B))
        {
            should_open_asset_browser = true;
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
            inspect_entity(im_ui_ctx, nullptr, gizmo->get_selection()[0], the_scene);

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

                    visit_systems(&the_scene, [&](const char * system_name, auto * system_pointer)
                    {
                        if (system_pointer)
                        {
                            if (type_name == get_typename<mesh_component>()) system_pointer->create(selection, get_typeid<mesh_component>(), &mesh_component(selection));
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
        
        const auto entity_list = the_scene.entity_list();
        std::vector<entity> root_list;
        for (auto & e : entity_list)
        {
            // Does the entity have a transform?
            if (auto * t = the_scene.xform_system->get_local_transform(e))
            {
                // If it has a valid parent, it's a child, so we skip it
                if (t->parent != kInvalidEntity) continue;
                root_list.push_back(e);
            }
            else
            {
                // We also list out entities with no transform
                root_list.push_back(e);
            }
        }

        // Now walk the root list
        uint32_t stack_open = 0;
        for (size_t i = 0; i < root_list.size(); ++i)
        {
            draw_entity_scenegraph(root_list[i]);
        }

        gui::imgui_fixed_window_end();

        // Define a split region between the whole window and the left panel
        static int leftSplit = 380;
        static int leftSplit1 = (height / 2);
        auto leftRegionSplit = ImGui::Split({ { 0.f,17.f },{ (float)width, (float)height } }, &leftSplit, ImGui::SplitType::Left);
        auto lsplit2 = ImGui::Split(leftRegionSplit.second, &leftSplit1, ImGui::SplitType::Top);
        ui_rect topLeftPane = { { int2(lsplit2.second.min()) },{ int2(lsplit2.second.max()) } };
        ui_rect bottomLeftPane = { { int2(lsplit2.first.min()) },{ int2(lsplit2.first.max()) } };

        gui::imgui_fixed_window_begin("Settings", topLeftPane);
        {
            ImGui::Dummy({ 0, 10 });

            if (ImGui::TreeNode("Rendering"))
            {
                ImGui::Checkbox("Show Floor Grid", &show_grid);

                ImGui::SliderFloat("Far Clip", &cam.farclip, 2.f, 256.f);

                renderer_settings lastSettings = the_scene.render_system->get_renderer()->settings;
                if (build_imgui(im_ui_ctx, "Renderer", *the_scene.render_system->get_renderer()))
                {
                    the_scene.render_system->get_renderer()->gpuProfiler.set_enabled(the_scene.render_system->get_renderer()->settings.performanceProfiling);
                    the_scene.render_system->get_renderer()->cpuProfiler.set_enabled(the_scene.render_system->get_renderer()->settings.performanceProfiling);
                }

                ImGui::Dummy({ 0, 10 });

                ImGui::Dummy({ 0, 10 });

                if (auto * shadows = the_scene.render_system->get_renderer()->get_shadow_pass())
                {
                    if (ImGui::TreeNode("Shadow Mapping"))
                    {
                        build_imgui(im_ui_ctx, "shadows", *shadows);
                        ImGui::TreePop();
                    }
                }

                ImGui::TreePop();
            }

            ImGui::Dummy({ 0, 10 });

            if (the_scene.render_system->get_renderer()->settings.performanceProfiling)
            {
                for (auto & t : the_scene.render_system->get_renderer()->gpuProfiler.get_data()) ImGui::Text("[Renderer GPU] %s %f ms", t.first.c_str(), t.second);
                for (auto & t : the_scene.render_system->get_renderer()->cpuProfiler.get_data()) ImGui::Text("[Renderer CPU] %s %f ms", t.first.c_str(), t.second);
            }

            ImGui::Dummy({ 0, 10 });

            for (auto & t : editorProfiler.get_data()) ImGui::Text("[Editor] %s %f ms", t.first.c_str(), t.second);
        }
        gui::imgui_fixed_window_end();

        gui::imgui_fixed_window_begin("Application Log", bottomLeftPane);
        {
            editor_console_log.Draw("-");
        }
        gui::imgui_fixed_window_end();
    }

    igm->end_frame();
    editorProfiler.end("imgui-editor");

    gl_check_error(__FILE__, __LINE__);

    glFlush();

    // `should_open_material_window` flag required because opening a new window directly 
    // from an ImGui instance trashes some piece of state somewhere
    if (should_open_material_window)
    {
        should_open_material_window = false;
        open_material_editor();
    }

    if (should_open_asset_browser)
    {
        should_open_asset_browser = false;
        open_asset_browser();
    }

    if (material_editor && material_editor->get_window())
    {
        material_editor->run();
        glfwMakeContextCurrent(window);
    }

    if (asset_browser && asset_browser->get_window())
    {
        asset_browser->run();
        glfwMakeContextCurrent(window);
    }

    gl_check_error(__FILE__, __LINE__);

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
