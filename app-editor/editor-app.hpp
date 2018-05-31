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
#include "uniforms.hpp"
#include "asset-handle-utils.hpp"
#include "environment.hpp"
#include "arcball.hpp"
#include "gizmo-controller.hpp"
#include "asset-resolver.hpp"
#include "material-editor.hpp"

struct scene_editor_app final : public polymer_app
{
    perspective_camera cam;
    fps_camera_controller flycam;
    profiler<simple_cpu_timer> editorProfiler;
    gl_shader_monitor shaderMonitor { "../assets/" };
    gl_renderable_grid grid{ 1.f, 512, 512 };

    ImGui::editor_app_log log;
    bool show_imgui = true;
    bool show_grid = true;
    bool should_open_material_window = false;
    std::string working_dir_on_launch;

    shader_handle wireframeHandle{ "wireframe" };

    std::unique_ptr<gui::imgui_instance> igm;
    std::unique_ptr<asset_resolver> resolver;
    std::unique_ptr<material_editor_window> material_editor;
    std::unique_ptr<simple_texture_view> fullscreen_surface;
    std::shared_ptr<gizmo_controller> gizmo;

    render_payload the_render_payload;
    entity_orchestrator orchestrator;
    environment scene;

    entity create_model(const std::string & geom_handle, const std::string & mesh_handle);
    void draw_entity_scenegraph(const entity e, int32_t depth, uint32_t & stack_open);

    scene_editor_app();
    ~scene_editor_app() {}

    void open_material_editor();

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
    void on_drop(std::vector<std::string> filepaths) override;
};
