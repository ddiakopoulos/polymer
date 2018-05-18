#include "index.hpp"
#include "editor-app.hpp"
#include "editor-inspector-ui.hpp"
#include "serialization.inl"
#include "logging.hpp"
#include "win32.hpp"

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
    flycam.set_camera(&cam);

    load_editor_intrinsic_assets("../assets/models/runtime/");

    shaderMonitor.watch("wireframe",
        "../assets/shaders/wireframe_vert.glsl",
        "../assets/shaders/wireframe_frag.glsl",
        "../assets/shaders/wireframe_geom.glsl",
        "../assets/shaders/renderer");

    shaderMonitor.watch("sky-hosek",
        "../assets/shaders/sky_vert.glsl",
        "../assets/shaders/sky_hosek_frag.glsl");

    shaderMonitor.watch("depth-prepass",
        "../assets/shaders/renderer/depth_prepass_vert.glsl",
        "../assets/shaders/renderer/depth_prepass_frag.glsl",
        "../assets/shaders/renderer");

    shaderMonitor.watch("default-shader",
        "../assets/shaders/renderer/forward_lighting_vert.glsl",
        "../assets/shaders/renderer/default_material_frag.glsl",
        "../assets/shaders/renderer");

    shaderMonitor.watch("post-tonemap",
        "../assets/shaders/renderer/post_tonemap_vert.glsl",
        "../assets/shaders/renderer/post_tonemap_frag.glsl");

    shaderMonitor.watch("cascaded-shadows",
        "../assets/shaders/renderer/shadowcascade_vert.glsl",
        "../assets/shaders/renderer/shadowcascade_frag.glsl",
        "../assets/shaders/renderer/shadowcascade_geom.glsl",
        "../assets/shaders/renderer");

    shaderMonitor.watch("pbr-forward-lighting",
        "../assets/shaders/renderer/forward_lighting_vert.glsl",
        "../assets/shaders/renderer/forward_lighting_frag.glsl",
        "../assets/shaders/renderer");

    fullscreen_surface.reset(new simple_texture_view());

    renderer_settings initialSettings;
    initialSettings.renderSize = int2(width, height);
    scene.render_system = orchestrator.create_system<pbr_render_system>(&orchestrator, initialSettings);
    scene.collision_system = orchestrator.create_system<collision_system>(&orchestrator);
    scene.xform_system = orchestrator.create_system<transform_system>(&orchestrator);
    scene.name_system = orchestrator.create_system<name_system>(&orchestrator);

    gizmo_selector.reset(new selection_controller(scene.xform_system));

    const entity sunlight = scene.track_entity(orchestrator.create_entity());
    scene.xform_system->create(sunlight, transform(), {});
    scene.name_system->create(sunlight, "sunlight");

    // Setup the skybox; link internal parameters to a directional light entity owned by the render system. 
    scene.skybox.reset(new gl_hosek_sky());
    scene.skybox->onParametersChanged = [this, sunlight]
    {
        scene.render_system->directional_lights[sunlight].data.direction = scene.skybox->get_sun_direction();
        scene.render_system->directional_lights[sunlight].data.color = float3(1.f, 1.0f, 1.0f);
        scene.render_system->directional_lights[sunlight].data.amount = 1.0f;
    };

    // Set initial values on the skybox with the sunlight entity we just created
    scene.skybox->onParametersChanged();

    // Only need to set the skybox on the |render_payload| once (unless we clear the payload)
    the_render_payload.skybox = scene.skybox.get();
    the_render_payload.xform_system = scene.xform_system;

    // fixme to be resolved
    auto radianceBinary = read_file_binary("../assets/textures/envmaps/wells_radiance.dds");
    auto irradianceBinary = read_file_binary("../assets/textures/envmaps/wells_irradiance.dds");
    gli::texture_cube radianceHandle(gli::load_dds((char *)radianceBinary.data(), radianceBinary.size()));
    gli::texture_cube irradianceHandle(gli::load_dds((char *)irradianceBinary.data(), irradianceBinary.size()));
    create_handle_for_asset("wells-radiance-cubemap", load_cubemap(radianceHandle));
    create_handle_for_asset("wells-irradiance-cubemap", load_cubemap(irradianceHandle));

    the_render_payload.ibl_irradianceCubemap = texture_handle("wells-irradiance-cubemap");
    the_render_payload.ibl_radianceCubemap = texture_handle("wells-radiance-cubemap");

    //scene.objects.clear();
    //cereal::deserialize_from_json("../assets/scene.json", scene.objects);

    scene.mat_library.reset(new polymer::material_library("../assets/materials.json"));

    // Resolve asset_handles to resources on disk
    resolver.reset(new asset_resolver());
    resolver->resolve("../assets/", &scene, scene.mat_library.get());

    // Debugging: 
    {
        create_handle_for_asset("debug-icosahedron", make_mesh_from_geometry(make_icosasphere(3)));

        // Create a debug entity
        const entity debug_icosa = scene.track_entity(orchestrator.create_entity());

        // Create mesh component for the gpu mesh
        polymer::mesh_component mesh_component(debug_icosa);
        mesh_component.mesh = gpu_mesh_handle("debug-icosahedron");
        scene.render_system->meshes[debug_icosa] = mesh_component;
        scene.render_system->materials[debug_icosa].material = material_handle(material_library::kDefaultMaterialId);
        scene.xform_system->create(debug_icosa, transform(float3(0, 0, 0)), { 1.f, 1.f, 1.f });
        scene.name_system->create(debug_icosa, "debug-icosahedron");
        the_render_payload.render_set.push_back(debug_icosa);
    }
}

scene_editor_app::~scene_editor_app()
{ 

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
            return;
        }

        auto importedModel = import_model(path);

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
        reset_renderer(size, settings);
        scene.skybox->onParametersChanged(); // reconfigure directional light
    }
}

void scene_editor_app::on_input(const app_input_event & event)
{
    igm->update_input(event);
    gizmo_selector->on_input(event);

    if (ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard)
    {
        flycam.reset();
        gizmo_selector->reset_input();
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
                gizmo_selector->clear();
            }

            // Focus on currently selected object
            if (event.value[0] == GLFW_KEY_F && event.action == GLFW_RELEASE)
            {
                if (gizmo_selector->get_selection().size() == 0) return;
                if (entity theSelection = gizmo_selector->get_selection()[0])
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
                // ... 
            }
        }

        // Raycast for editor/gizmo selection on mouse up
        if (event.type == app_input_event::MOUSE && event.action == GLFW_RELEASE && event.value[0] == GLFW_MOUSE_BUTTON_LEFT)
        {
            int width, height;
            glfwGetWindowSize(window, &width, &height);

            const ray r = cam.get_world_ray(event.cursor, float2(static_cast<float>(width), static_cast<float>(height)));

            if (length(r.direction) > 0 && !gizmo_selector->active())
            {
                std::vector<entity> selectedObjects;
                float best_t = std::numeric_limits<float>::max();

                entity hitObject = kInvalidEntity;

                for (auto & mesh : scene.collision_system->meshes)
                {
                    raycast_result result = scene.collision_system->raycast(mesh.first, r);
                    if (result.hit)
                    {
                        if (result.distance < best_t)
                        {
                            best_t = result.distance;
                            hitObject = mesh.first;
                        }
                    }
                }

                if (hitObject != kInvalidEntity) selectedObjects.push_back(hitObject);

                // New object was selected
                if (selectedObjects.size() > 0)
                {
                    // Multi-selection
                    if (event.mods & GLFW_MOD_CONTROL)
                    {
                        auto existingSelection = gizmo_selector->get_selection();
                        for (auto s : selectedObjects)
                        {
                            if (!gizmo_selector->selected(s)) existingSelection.push_back(s);
                        }
                        gizmo_selector->set_selection(existingSelection);
                    }
                    // Single Selection
                    else
                    {
                        gizmo_selector->set_selection(selectedObjects);
                    }
                }
            }

        }
    }
}

void scene_editor_app::reset_renderer(int2 size, const renderer_settings & settings)
{
    scene.render_system = orchestrator.create_system<pbr_render_system>(&orchestrator, settings);
}

void scene_editor_app::open_material_editor()
{
    if (!material_editor)
    {
       material_editor.reset(new material_editor_window(get_shared_gl_context(), 500, 1200, "", 1, scene, gizmo_selector, orchestrator));
    }
    else if (!material_editor->get_window())
    {
        // Workaround since there's no convenient way to reset the material_editor when it's been closed
        material_editor.reset(new material_editor_window(get_shared_gl_context(), 500, 1200, "", 1, scene, gizmo_selector, orchestrator));
    }

    glfwMakeContextCurrent(window);
}

void scene_editor_app::on_update(const app_update_event & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    editorProfiler.begin("on_update");
    flycam.update(e.timestep_ms);
    shaderMonitor.handle_recompile();
    gizmo_selector->on_update(cam, float2(static_cast<float>(width), static_cast<float>(height)));
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

        the_render_payload.views.clear();

        /*
        // Remember to clear any transient per-frame data
        the_render_payload.pointLights.clear();
        the_render_payload.renderSet.clear();
        the_render_payload.views.clear();

        // Gather Lighting
        for (auto & obj : scene.objects)
        {
            if (auto * r = dynamic_cast<PointLight*>(obj.get()))
            {
                the_render_payload.pointLights.push_back(r->data);
            }
        }

        // Gather Objects
        std::vector<Renderable *> sceneObjects;
        for (auto & obj : scene.objects)
        {
            if (auto * r = dynamic_cast<Renderable*>(obj.get()))
            {
                the_render_payload.renderSet.push_back(r);
            }
        }
        */

        // Add single-viewport camera
        the_render_payload.views.push_back(view_data(0, cam.pose, projectionMatrix));

        editorProfiler.end("gather-scene");

        // Submit scene to the scene renderer
        editorProfiler.begin("submit-scene");
        scene.render_system->render_frame(the_render_payload);
        editorProfiler.end("submit-scene");

        // Draw to screen framebuffer
        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        fullscreen_surface->draw(scene.render_system->get_color_texture(0));

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
        for (const entity e : gizmo_selector->get_selection())
        {
            const transform p = scene.xform_system->get_world_transform(e)->world_pose;
            const float3 scale = scene.xform_system->get_local_transform(e)->local_scale;
            const float4x4 modelMatrix = mul(p.matrix(), make_scaling_matrix(scale));
            program.uniform("u_modelMatrix", modelMatrix);
            scene.render_system->meshes[e].draw();
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
        bool mod_enabled = !gizmo_selector->active();
        if (menu.item("Open Scene", GLFW_MOD_CONTROL, GLFW_KEY_O, mod_enabled))
        {
            const auto selected_open_path = windows_file_dialog("polymer scene", "json", true);
            if (!selected_open_path.empty())
            {
                scene.render_system->destroy(kAllEntities);
                gizmo_selector->clear();
                //cereal::deserialize_from_json(selected_open_path, scene.objects);
                glfwSetWindowTitle(window, selected_open_path.c_str());
            }
        }

        if (menu.item("Save Scene", GLFW_MOD_CONTROL, GLFW_KEY_S, mod_enabled))
        {
            const auto save_path = windows_file_dialog("polymer scene", "json", false);
            if (!save_path.empty())
            {
                gizmo_selector->clear();
                //write_file_text(save_path, cereal::serialize_to_json(scene.objects));
                glfwSetWindowTitle(window, save_path.c_str());
            }
        }

        if (menu.item("New Scene", GLFW_MOD_CONTROL, GLFW_KEY_N, mod_enabled))
        {
            gizmo_selector->clear();
            scene.render_system->destroy(kAllEntities);
        }

        if (menu.item("Take Screenshot", GLFW_MOD_CONTROL, GLFW_KEY_EQUAL, mod_enabled))
        {
            request_screenshot("scene-editor");
        }

        if (menu.item("Exit", GLFW_MOD_ALT, GLFW_KEY_F4)) exit();
        menu.end();

        menu.begin("Edit");
        if (menu.item("Clone", GLFW_MOD_CONTROL, GLFW_KEY_D)) {}
        if (menu.item("Delete", 0, GLFW_KEY_DELETE)) 
        {
            // todo - destroy on all systems

            //auto it = std::remove_if(std::begin(scene.objects), std::end(scene.objects), [this](std::shared_ptr<GameObject> obj) 
            //{ 
            //    return gizmo_selector->selected(obj.get());
            //});
            //scene.objects.erase(it, std::end(scene.objects));

            gizmo_selector->clear();
        }
        if (menu.item("Select All", GLFW_MOD_CONTROL, GLFW_KEY_A)) 
        {
            std::vector<entity> all_entities;
            for (auto mesh : scene.render_system->meshes)
            {
                all_entities.push_back(mesh.first);
            }
            gizmo_selector->set_selection(all_entities);
        }
        menu.end();

        menu.begin("Create");
        if (menu.item("entity")) 
        {
            // Newly spawned objects are selected by default
            std::vector<entity> list = { scene.track_entity(orchestrator.create_entity()) };
            scene.xform_system->create(list[0], transform(), {});
            scene.name_system->create(list[0], "new entity");
            gizmo_selector->set_selection(list);
        }
        menu.end();

        menu.begin("Windows");
        if (menu.item("Material Editor"))
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
        if (gizmo_selector->get_selection().size() >= 1)
        {
            inspect_scene_entity(nullptr, gizmo_selector->get_selection()[0], scene);
        }
        gui::imgui_fixed_window_end();

        gui::imgui_fixed_window_begin("Scene Entities", bottomRightPane);
        for (auto entity : scene.entity_list())
        {
            ImGui::PushID(static_cast<int>(entity));

            const bool selected = gizmo_selector->selected(entity);
            std::string name = scene.name_system->get_name(entity);
            name = name.empty() ? "entity" : name;

            if (ImGui::Selectable(name.c_str(), &selected))
            {
                if (!ImGui::GetIO().KeyCtrl) gizmo_selector->clear();
                gizmo_selector->update_selection(entity);
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

            if (ImGui::TreeNode("Core"))
            {
                renderer_settings lastSettings = scene.render_system->settings;

                if (build_imgui("renderer", *scene.render_system))
                {
                    scene.render_system->gpuProfiler.set_enabled(scene.render_system->settings.performanceProfiling);
                    scene.render_system->cpuProfiler.set_enabled(scene.render_system->settings.performanceProfiling);
                }

                ImGui::TreePop();
            }

            ImGui::Dummy({ 0, 10 });

            //if (ImGui::TreeNode("Procedural Sky"))
            //{
            //    inspect_entity(nullptr, the_render_payload.skybox);
            //    ImGui::TreePop();
            //}

            ImGui::Dummy({ 0, 10 });

            if (auto * shadows = scene.render_system->get_shadow_pass())
            {
                if (ImGui::TreeNode("Cascaded Shadow Mapping"))
                {
                    build_imgui("shadows", *shadows);
                    ImGui::TreePop();
                }
            }

            ImGui::Dummy({ 0, 10 });

            if (scene.render_system->settings.performanceProfiling)
            {
                for (auto & t : scene.render_system->gpuProfiler.get_data()) ImGui::Text("[Renderer GPU] %s %f ms", t.first.c_str(), t.second);
                for (auto & t : scene.render_system->cpuProfiler.get_data()) ImGui::Text("[Renderer CPU] %s %f ms", t.first.c_str(), t.second);
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
        gizmo_selector->on_draw();
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
