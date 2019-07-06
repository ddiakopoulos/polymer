#include "lib-polymer.hpp"

#include "gl-gizmo.hpp"
#include "gl-imgui.hpp"
#include "gl-texture-view.hpp"
#include "gl-renderable-grid.hpp"

#include "ecs/component-pool.hpp"
#include "ecs/core-ecs.hpp"
#include "ecs/core-events.hpp"
#include "ecs/typeid.hpp"

#include "system-identifier.hpp"
#include "system-transform.hpp"
#include "system-render.hpp"
#include "system-collision.hpp"

#include "material.hpp"
#include "renderer-uniforms.hpp"
#include "asset-handle-utils.hpp"
#include "environment.hpp"
#include "arcball.hpp"
#include "asset-resolver.hpp"
#include "ui-actions.hpp"

#include "material-editor.hpp"
#include "asset-browser.hpp"
#include "gizmo-controller.hpp"

struct scene_editor_app final : public polymer_app
{
    perspective_camera cam;
    fps_camera_controller flycam;
    profiler<simple_cpu_timer> editorProfiler;
    gl_shader_monitor shaderMonitor { "../assets/" };
    gl_renderable_grid grid{ 1.f, 512, 512 };

    imgui_ui_context im_ui_ctx;
    undo_manager undo_mgr;

    ImGui::editor_app_log log;
    bool show_imgui = true;
    bool show_grid = false;
    bool should_open_material_window = false;
    bool should_open_asset_browser = false;
    std::string working_dir_on_launch;
    std::string currently_open_scene {"New Scene"};

    shader_handle wireframeHandle{ "wireframe" };

    std::unique_ptr<gui::imgui_instance> igm;
    std::unique_ptr<material_editor_window> material_editor;
    std::unique_ptr<asset_browser_window> asset_browser;
    std::unique_ptr<simple_texture_view> fullscreen_surface;
    std::shared_ptr<gizmo_controller> gizmo;

    render_payload renderer_payload;
    entity_orchestrator orchestrator;
    environment scene;

    void draw_entity_scenegraph(const entity e);

    scene_editor_app();
    ~scene_editor_app() {}

    void open_material_editor();
    void open_asset_browser();

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
    void on_drop(std::vector<std::string> filepaths) override;

    void import_scene(const std::string & path);
};
