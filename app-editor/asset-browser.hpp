#pragma once

#ifndef asset_browser_hpp
#define asset_browser_hpp

#include "material.hpp"
#include "renderer-pbr.hpp"
#include "asset-handle-utils.hpp"
#include "gizmo-controller.hpp"
#include "environment.hpp"
#include "editor-inspector-ui.hpp"
#include "arcball.hpp"
#include "gl-texture-view.hpp"
#include "win32.hpp"
#include "model-io.hpp"

inline entity create_model(const std::string & geom_handle,
    const std::string & mesh_handle,
    environment & env,
    entity_orchestrator & orch)
{
    const entity e = env.track_entity(orch.create_entity());

    env.identifier_system->create(e, mesh_handle);
    env.xform_system->create(e, transform(float3(0, 0, 0)), { 1.f, 1.f, 1.f });

    polymer::material_component model_mat(e);
    model_mat.material = material_handle(material_library::kDefaultMaterialId);
    env.render_system->create(e, std::move(model_mat));

    polymer::mesh_component model_mesh(e);
    model_mesh.mesh = gpu_mesh_handle(mesh_handle);
    env.render_system->create(e, std::move(model_mesh));

    polymer::geometry_component model_geom(e);
    model_geom.geom = cpu_mesh_handle(mesh_handle);
    env.collision_system->create(e, std::move(model_geom));;

    return e;
}

inline std::vector<entity> import_asset(const std::string & filepath, 
    environment & env, 
    entity_orchestrator & orch)
{   
    std::vector<entity> created_entities;

    auto path = filepath;

    std::transform(path.begin(), path.end(), path.begin(), ::tolower);
    const std::string ext = get_extension(path);

    // Handle image/texture types
    if (ext == "png" || ext == "tga" || ext == "jpg")
    {
        create_handle_for_asset(get_filename_without_extension(path).c_str(), load_image(path, false));
        return {};
    }

    // Handle mesh types
    auto imported_models = import_model(path);
    const size_t num_models = imported_models.size();
    std::vector<entity> children;

    for (auto & m : imported_models)
    {
        auto & mesh = m.second;
        rescale_geometry(mesh, 1.f);

        const std::string handle_id = get_filename_without_extension(path) + "-" + m.first;

        create_handle_for_asset(handle_id.c_str(), make_mesh_from_geometry(mesh));
        create_handle_for_asset(handle_id.c_str(), std::move(mesh));

        if (num_models == 1) created_entities.push_back(create_model(handle_id, handle_id, env, orch));
        else children.push_back(create_model(handle_id, handle_id, env, orch));
    }

    if (children.size())
    {
        const entity root_entity = env.track_entity(orch.create_entity());
        created_entities.push_back(root_entity);
        env.identifier_system->create(root_entity, "root-" + std::to_string(root_entity));
        env.xform_system->create(root_entity, transform(float3(0, 0, 0)), { 1.f, 1.f, 1.f });
        for (const entity child : children)
        {
            env.xform_system->add_child(root_entity, child);
            created_entities.push_back(child);
        }
    }

    return created_entities;
}

struct asset_browser_window final : public glfw_window
{
    std::unique_ptr<gui::imgui_instance> auxImgui;

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
