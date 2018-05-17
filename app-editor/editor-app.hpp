#include "index.hpp"
#include "gl-gizmo.hpp"
#include "gl-imgui.hpp"
#include "gl-texture-view.hpp"

#include "ecs/component-pool.hpp"
#include "ecs/core-ecs.hpp"
#include "ecs/core-events.hpp"
#include "ecs/system-name.hpp"
#include "ecs/system-transform.hpp"
#include "ecs/typeid.hpp"

#include "material.hpp"
#include "renderer-standard.hpp"
#include "uniforms.hpp"
#include "asset-handle-utils.hpp"
#include "scene.hpp"
#include "editor-ui.hpp"
#include "arcball.hpp"
#include "selection.hpp"
#include "asset-resolver.hpp"
#include "material-editor.hpp"

struct scene_editor_app final : public polymer_app
{
    perspective_camera cam;
    fps_camera_controller flycam;
    profiler<simple_cpu_timer> editorProfiler;
    gl_shader_monitor shaderMonitor { "../assets/" };
    ImGui::editor_app_log log;
    bool show_imgui = true;

    shader_handle wireframeHandle{ "wireframe" };
    shader_handle iblHandle{ "ibl" };

    std::unique_ptr<asset_resolver> resolver;
    std::unique_ptr<material_editor_window> material_editor;
    std::unique_ptr<simple_texture_view> fullscreen_surface;
    std::unique_ptr<gui::imgui_instance> igm;
    std::shared_ptr<selection_controller> gizmo_selector;

    entity_orchestrator orchestrator;
    render_payload scene_payload;
    poly_scene scene;

    scene_editor_app();
    ~scene_editor_app();

    void reset_renderer(int2 size, const renderer_settings & settings);

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
    void on_drop(std::vector<std::string> filepaths) override;
};