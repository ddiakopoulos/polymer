#include "index.hpp"
#include "gl-gizmo.hpp"
#include "gl-imgui.hpp"
#include "gl-texture-view.hpp"

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
    gl_shader_monitor shaderMonitor { "../assets/" };

    uint32_t pbrProgramAsset = -1;

    shader_handle wireframeHandle{ "wireframe" };
    shader_handle iblHandle{ "ibl" };

    profiler<simple_cpu_timer> editorProfiler;

    std::unique_ptr<asset_resolver> resolver;
    std::unique_ptr<material_editor_window> material_editor;
    std::unique_ptr<selection_controller<GameObject>> gizmo_selector;

    std::unique_ptr<gui::imgui_instance> igm;
    std::unique_ptr<forward_renderer> renderer;
    std::unique_ptr<fullscreen_texture> fullscreen_surface;

    render_payload sceneData;
    poly_scene scene;

    ImGui::editor_app_log log;
    auto_layout uiSurface;
    std::vector<std::shared_ptr<GLTextureView>> debugViews;
    bool showUI = true;

    scene_editor_app();
    ~scene_editor_app();

    void reset_renderer(int2 size, const renderer_settings & settings);

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
    void on_drop(std::vector <std::string> filepaths) override;
};