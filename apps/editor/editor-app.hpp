#include "polymer-core/lib-polymer.hpp"
#include "polymer-app-base/glfw-app.hpp"
#include "polymer-app-base/camera-controllers.hpp"
#include "polymer-model-io/model-io.hpp"

#include "polymer-app-base/wrappers/gl-gizmo.hpp"
#include "polymer-app-base/wrappers/gl-imgui.hpp"
#include "polymer-gfx-gl/gl-texture-view.hpp"
#include "polymer-gfx-gl/gl-renderable-grid.hpp"

#include "polymer-engine/ecs/component-pool.hpp"
#include "polymer-engine/ecs/core-ecs.hpp"
#include "polymer-engine/ecs/core-events.hpp"
#include "polymer-engine/ecs/typeid.hpp"

#include "polymer-engine/system/system-collision.hpp"

#include "polymer-engine/material.hpp"
#include "polymer-engine/renderer/renderer-uniforms.hpp"
#include "polymer-engine/asset/asset-handle-utils.hpp"
#include "polymer-engine/asset/asset-import.hpp"
#include "polymer-engine/scene.hpp"
#include "polymer-engine/object.hpp"
#include "polymer-engine/asset/asset-resolver.hpp"

#include "polymer-app-base/ui-actions.hpp"
#include "polymer-core/tools/arcball.hpp"

// Note: editor-inspector-ui.hpp must come before gizmo-controller.hpp
// to avoid 'log' symbol ambiguity with imgui_internal.h
#include "editor-inspector-ui.hpp"
#include "gizmo-controller.hpp"
#include "material-editor.hpp"
#include "asset-browser.hpp"

struct scene_editor_app final : public polymer_app
{
    perspective_camera cam;
    camera_controller_fps flycam;
    profiler<simple_cpu_timer> editorProfiler;
    gl_shader_monitor shaderMonitor { "../assets/" };
    gl_renderable_grid grid{ 1.f, 512, 512 };

    imgui_ui_context im_ui_ctx;
    undo_manager undo_mgr;

    ImGui::editor_app_log editor_console_log;
    bool show_imgui = true;
    bool show_grid = true;
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
    scene the_scene;

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
