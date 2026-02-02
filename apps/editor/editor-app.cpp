#include "polymer-engine/renderer/renderer-debug.hpp"
#include "polymer-engine/renderer/renderer-util.hpp"
#include "editor-app.hpp"

inline void delete_selected_entity(gizmo_controller * gizmo, scene & scene)
{
    const auto selection_list = gizmo->get_selection();
    if (!selection_list.empty() && selection_list[0] != kInvalidEntity)
    {
        scene.destroy(selection_list[0]);
    }
    gizmo->clear();
}

scene_editor_app::scene_editor_app() : polymer_app(1920, 1080, "Polymer Scene Editor")
{
    working_dir_on_launch = get_current_directory();

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    polymer::log::get()->set_engine_logger(std::make_shared<ImGui::spdlog_editor_sink>(editor_console_log));

    auto asset_base = global_asset_dir::get()->get_asset_dir();

    igm.reset(new gui::imgui_instance(window));
    gui::make_light_theme();
    igm->add_font(read_file_binary(asset_base + "/fonts/droid_sans.ttf"));

    cam.look_at({ 0, 5.f, -5.f }, { 0, 3.5f, 0 });
    cam.farclip = 32.f;
    flycam.set_camera(&cam);

    load_required_renderer_assets(asset_base, shaderMonitor);

    shaderMonitor.watch("wireframe",
       asset_base + "/shaders/wireframe_vert.glsl",
       asset_base + "/shaders/wireframe_frag.glsl",
       asset_base + "/shaders/wireframe_geom.glsl",
       asset_base + "/shaders/renderer");

    fullscreen_surface.reset(new simple_texture_view());

    the_scene.reset({ width, height }, true);

    // Load all materials from the assets directory at startup
    the_scene.resolver->add_search_path(asset_base);
    the_scene.resolver->resolve();

    polymer::global_debug_mesh_manager::get()->initialize_resources(&the_scene);

    gizmo.reset(new gizmo_controller(&the_scene));

    /*
    // glTF test imports - load test models at different positions
    {
        // Test 1: Triangle (basic indexed geometry)
        auto triangle_entities = import_asset_runtime(asset_base + "/gltf/Triangle/glTF/Triangle.gltf", the_scene);
        for (entity e : triangle_entities)
        {
            if (auto * xform = the_scene.get_graph().get_object(e).get_component<transform_component>())
            {
                xform->local_pose.position = float3(-3.f, 0.f, 0.f);
            }
        }

        // Test 2: SciFiHelmet (textured PBR model)
        auto helmet_entities = import_asset_runtime(asset_base + "/gltf/SciFiHelmet/glTF/SciFiHelmet.gltf", the_scene);
        for (entity e : helmet_entities)
        {
            if (auto * xform = the_scene.get_graph().get_object(e).get_component<transform_component>())
            {
                xform->local_pose.position = float3(0.f, 1.f, 0.f);
            }
        }

        // Test 3: RiggedSimple (skinned mesh with skeleton)
        auto rigged_entities = import_asset_runtime(asset_base + "/gltf/RiggedSimple/glTF/RiggedSimple.gltf", the_scene);
        for (entity e : rigged_entities)
        {
            if (auto * xform = the_scene.get_graph().get_object(e).get_component<transform_component>())
            {
                xform->local_pose.position = float3(3.f, 0.f, 0.f);
            }
        }

        // Test 4: AnimatedCube (animation tracks)
        auto animated_entities = import_asset_runtime(asset_base + "/gltf/AnimatedCube/glTF/AnimatedCube.gltf", the_scene);
        for (entity e : animated_entities)
        {
            if (auto * xform = the_scene.get_graph().get_object(e).get_component<transform_component>())
            {
                xform->local_pose.position = float3(6.f, 0.5f, 0.f);
            }
        }
        */

        the_scene.get_graph().refresh();
    }
}

void scene_editor_app::on_drop(std::vector<std::string> filepaths)
{
    for (const auto & path : filepaths)
    {
        const std::string ext = get_extension(path);
        if (ext == "json") import_scene(path);
        else import_asset_runtime(path, the_scene);
    }
}

void scene_editor_app::on_window_resize(int2 size)
{
    // Iconification/minimization triggers an on_window_resize event with a zero size
    // Note: Window resize reconfiguration is not currently supported
    (void)size;
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
                entity theSelection = gizmo->get_selection()[0];
                if (theSelection != kInvalidEntity)
                {
                    base_object & obj = the_scene.get_graph().get_object(theSelection);
                    const transform selectedObjectPose = obj.get_component<transform_component>()->get_world_transform();
                    const float3 focusOffset = selectedObjectPose.position + float3(0.f, 0.5f, 4.f);
                    cam.look_at(focusOffset, selectedObjectPose.position);
                    flycam.update_yaw_pitch();
                }
            }

            if (event.value[0] == GLFW_KEY_TAB && event.action == GLFW_RELEASE)
            {
                show_imgui = !show_imgui;
            }

            if (event.value[0] == GLFW_KEY_SPACE && event.action == GLFW_RELEASE) 
            {
            
            }

            if (event.value[0] == GLFW_KEY_DELETE && event.action == GLFW_RELEASE)
            {
                delete_selected_entity(gizmo.get(), the_scene);
            }

            // XZ plane nudging
            if (event.action == GLFW_RELEASE)
            {
                auto nudge = [this](const float3 amount)
                {
                    if (gizmo->get_selection().size() == 0) return;
                    const entity first_selection = gizmo->get_selection()[0];
                    if (first_selection != kInvalidEntity)
                    {
                        base_object & obj = the_scene.get_graph().get_object(first_selection);
                        auto * xform = obj.get_component<transform_component>();
                        xform->local_pose.position += amount;
                        the_scene.get_graph().refresh();
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
                entity_hit_result result = the_scene.get_collision_system()->raycast(r);
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
                else if (!(event.mods & GLFW_MOD_CONTROL))
                {
                    // Clicked on empty space without Ctrl - deselect all
                    gizmo->clear();
                }
            }

            if (gizmo->moved())
            {
                the_scene.get_collision_system()->queue_acceleration_rebuild();
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
        the_scene.reset({ width, height }, false); // don't create implicit objects if importing

        the_scene.import_environment(path);

        auto asset_base = global_asset_dir::get()->get_asset_dir();

        // Resolve polymer-local assets
        the_scene.resolver->add_search_path(asset_base);

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

    polymer::global_debug_mesh_manager::get()->initialize_resources(&the_scene);
}

void scene_editor_app::open_material_editor()
{
    material_editor.reset(new material_editor_window(get_shared_gl_context(), 600, 1200, "", 1, the_scene, gizmo));
    glfwMakeContextCurrent(window);
}

void scene_editor_app::open_asset_browser()
{
    asset_browser.reset(new asset_browser_window(get_shared_gl_context(), 800, 400, "asset browser", 1));
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
    if (e == kInvalidEntity)
    {
        throw std::invalid_argument("this entity is invalid");
    }

    base_object & obj = the_scene.get_graph().get_object(e);
    const std::vector<entity> children = the_scene.get_graph().get_children(e);

    bool open = false;

    ImGui::PushID(e.as_string().c_str());

    // Check if this has children
    if (children.size())
    {
        // Increase spacing to differentiate leaves from expanded contents.
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ImGui::GetFontSize());
        ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
        open = ImGui::TreeNode("");
        if (!open) ImGui::PopStyleVar();
        ImGui::SameLine();
    }

    const bool selected = gizmo->selected(e);
    std::string name = obj.name;
    std::string entity_as_string = "[" + e.as_string().substr(0, 8) + "] ";
    name = name.empty() ? entity_as_string + "<unnamed entity>" : entity_as_string + name;

    if (ImGui::Selectable(name.c_str(), selected))
    {
        if (!ImGui::GetIO().KeyCtrl) gizmo->clear();
        gizmo->update_selection(e);
    }

    if (open)
    {
        for (auto & c : children)
        {
            draw_entity_scenegraph(c);
        }
        ImGui::PopStyleVar();
        ImGui::Unindent(ImGui::GetFontSize());
        ImGui::TreePop();
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

    polymer::global_debug_mesh_manager::get()->upload();
    polymer::global_debug_mesh_manager::get()->clear();

    const float4x4 projectionMatrix = cam.get_projection_matrix(float(width) / float(height));
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = (projectionMatrix * viewMatrix);

    {
        editorProfiler.begin("gather-scene");

        // Clear out transient scene payload data
        renderer_payload.reset();

        // Lambda to assemble render component from base_object
        auto assemble_render_component = [](base_object & obj) {
            render_component r;
            r.material = obj.get_component<material_component>();
            r.mesh = obj.get_component<mesh_component>();
            if (auto * xform = obj.get_component<transform_component>())
            {
                r.world_matrix = xform->get_world_transform().matrix() * make_scaling_matrix(xform->local_scale);
            }
            r.render_sort_order = 0;
            return r;
        };

        // Iterate all objects in the scene graph
        for (auto & [e, obj] : the_scene.get_graph().graph_objects)
        {
            // Does the entity have a material? If so, we can render it.
            if (auto * mat_c = obj.get_component<material_component>())
            {
                auto * mesh_c = obj.get_component<mesh_component>();
                if (!mesh_c) continue; // for the case that we just created a material component and haven't set a mesh yet

                render_component r = assemble_render_component(obj);
                renderer_payload.render_components.push_back(r);
            }

            // IBL cubemap
            if (auto * cubemap = obj.get_component<ibl_component>())
            {
                renderer_payload.ibl_cubemap = cubemap;
            }

            // Procedural skybox
            if (auto * proc_skybox = obj.get_component<procedural_skybox_component>())
            {
                renderer_payload.procedural_skybox = proc_skybox;
                if (proc_skybox->sun_directional_light != kInvalidEntity)
                {
                    // Use scene::get_object which returns nullptr for invalid entities,
                    // not scene_graph::get_object which creates empty entries via operator[]
                    if (base_object * sun_obj = the_scene.get_object(proc_skybox->sun_directional_light))
                    {
                        if (auto * sunlight = sun_obj->get_component<directional_light_component>())
                        {
                            renderer_payload.sunlight = sunlight;
                        }
                    }
                }
            }

            // Point lights
            if (auto * pt_light_c = obj.get_component<point_light_component>())
            {
                renderer_payload.point_lights.push_back(pt_light_c);
            }
        }

        // Add debug renderer entity
        entity debug_ent = polymer::global_debug_mesh_manager::get()->get_entity();
        if (debug_ent != kInvalidEntity)
        {
            base_object & debug_obj = the_scene.get_graph().get_object(debug_ent);
            const auto debug_renderer_entity = assemble_render_component(debug_obj);
            if (debug_renderer_entity.mesh && debug_renderer_entity.material)
                renderer_payload.render_components.push_back(debug_renderer_entity);
        }

        // Add single-viewport camera
        renderer_payload.views.push_back(view_data(0, cam.pose, projectionMatrix));

        editorProfiler.end("gather-scene");

        // Submit scene to the scene renderer
        editorProfiler.begin("submit-scene");
        the_scene.get_renderer()->render_frame(renderer_payload);
        editorProfiler.end("submit-scene");

        // Draw to screen framebuffer
        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        fullscreen_surface->draw(the_scene.get_renderer()->get_color_texture(0));

        if (show_grid)
        {
            grid.draw(viewMatrix, projectionMatrix, cam.get_eye_point(), grid_plane::xz, 0.25f);
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
            base_object & obj = the_scene.get_graph().get_object(e);
            auto * xform = obj.get_component<transform_component>();
            const transform p = xform->get_world_transform();
            const float3 scale = xform->local_scale;
            const float4x4 modelMatrix = (p.matrix() * make_scaling_matrix(scale));
            program.uniform("u_modelMatrix", modelMatrix);
            if (auto * mesh = obj.get_component<mesh_component>())
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

    gui::imgui_menu_stack menu(*this);
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

            the_scene.reset({ width, height }, true);

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
                // Get the source object
                base_object & src_obj = the_scene.get_graph().get_object(selection_list[0]);

                // Create a copy with a new name
                base_object copy_obj(src_obj.name + "_copy");

                // Copy transform with offset
                if (auto * src_xform = src_obj.get_component<transform_component>())
                {
                    transform_component xform_copy = *src_xform;
                    xform_copy.local_pose.position += 0.1f;
                    copy_obj.add_component(xform_copy);
                }

                // Copy other components
                if (auto * mat = src_obj.get_component<material_component>())
                    copy_obj.add_component(*mat);
                if (auto * mesh = src_obj.get_component<mesh_component>())
                    copy_obj.add_component(*mesh);
                if (auto * geom = src_obj.get_component<geometry_component>())
                    copy_obj.add_component(*geom);

                base_object & created = the_scene.instantiate(std::move(copy_obj));

                std::vector<entity> new_selection_list = { created.get_entity() };
                gizmo->set_selection(new_selection_list);
            }
        }
        if (menu.item("Delete", 0, GLFW_KEY_DELETE))
        {
            delete_selected_entity(gizmo.get(), the_scene);
        }
        if (menu.item("Select All", GLFW_MOD_CONTROL, GLFW_KEY_A))
        {
            std::vector<entity> all_entities;
            for (auto & [e, obj] : the_scene.get_graph().graph_objects)
            {
                all_entities.push_back(e);
            }
            gizmo->set_selection(all_entities);
        }
        menu.end();

        menu.begin("Create");
        if (menu.item("basic entity"))
        {
            base_object & new_obj = the_scene.instantiate_empty("new entity");
            std::vector<entity> list = { new_obj.get_entity() };
            gizmo->set_selection(list); // Newly spawned objects are selected by default
        }
        if (menu.item("renderable entity"))
        {
            base_object & new_obj = the_scene.instantiate_mesh(
                "new renderable entity",
                transform(),
                float3(1.f),
                "cube-uniform"
            );
            std::vector<entity> list = { new_obj.get_entity() };
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

            // Inspect selected entity using the new pattern
            entity selected_entity = gizmo->get_selection()[0];
            base_object & selected_obj = the_scene.get_graph().get_object(selected_entity);
            inspect_entity_new(im_ui_ctx, selected_obj);

            if (ImGui::BeginPopupModal("Create Component", NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                const entity selection = gizmo->get_selection()[0];
                base_object & sel_obj = the_scene.get_graph().get_object(selection);

                ImGui::Dummy({ 0, 6 });

                // List available component types
                std::vector<std::string> component_names = {
                    "mesh_component",
                    "material_component",
                    "geometry_component",
                    "point_light_component",
                    "directional_light_component"
                };

                static int componentTypeSelection = -1;
                gui::Combo("Component", &componentTypeSelection, component_names);

                ImGui::Dummy({ 0, 6 });

                if (ImGui::Button("OK", ImVec2(120, 0)))
                {
                    if (componentTypeSelection == -1) ImGui::CloseCurrentPopup();
                    else
                    {
                        const std::string type_name = component_names[componentTypeSelection];

                        if (type_name == "mesh_component") sel_obj.add_component(mesh_component());
                        else if (type_name == "material_component") sel_obj.add_component(material_component());
                        else if (type_name == "geometry_component") sel_obj.add_component(geometry_component());
                        else if (type_name == "point_light_component") sel_obj.add_component(point_light_component());
                        else if (type_name == "directional_light_component") sel_obj.add_component(directional_light_component());

                        ImGui::CloseCurrentPopup();
                    }
                }

                ImGui::SetItemDefaultFocus();
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
                ImGui::EndPopup();
            }

        }
        gui::imgui_fixed_window_end();

        gui::imgui_fixed_window_begin("Scene Entities", bottomRightPane);

        // Build list of root entities (those without parents)
        std::vector<entity> root_list;
        for (auto & [e, obj] : the_scene.get_graph().graph_objects)
        {
            // If it has a valid parent, it's a child, so we skip it
            if (the_scene.get_graph().get_parent(e) != kInvalidEntity) continue;
            root_list.push_back(e);
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

                renderer_settings lastSettings = the_scene.get_renderer()->settings;
                if (build_imgui(im_ui_ctx, "Renderer", *the_scene.get_renderer()))
                {
                    the_scene.get_renderer()->gpuProfiler.set_enabled(the_scene.get_renderer()->settings.performanceProfiling);
                    the_scene.get_renderer()->cpuProfiler.set_enabled(the_scene.get_renderer()->settings.performanceProfiling);
                }

                ImGui::Dummy({ 0, 10 });

                ImGui::Dummy({ 0, 10 });

                if (auto * shadows = the_scene.get_renderer()->get_shadow_pass())
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

            if (the_scene.get_renderer()->settings.performanceProfiling)
            {
                for (auto & t : the_scene.get_renderer()->gpuProfiler.get_data()) ImGui::Text("[Renderer GPU] %s %f ms", t.first.c_str(), t.second);
                for (auto & t : the_scene.get_renderer()->cpuProfiler.get_data()) ImGui::Text("[Renderer CPU] %s %f ms", t.first.c_str(), t.second);
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
