#include "index.hpp"
#include "gl-gizmo.hpp"
#include "gl-imgui.hpp"
#include "gl-texture-view.hpp"

#include "material.hpp"
#include "fwd_renderer.hpp"
#include "uniforms.hpp"
#include "asset-defs.hpp"
#include "scene.hpp"
#include "editor-ui.hpp"
#include "arcball.hpp"
#include "selection.hpp"

#include "material-editor.hpp"

struct scene_editor_app final : public polymer_app
{
    perspective_camera cam;
    fps_camera_controller flycam;
    gl_shader_monitor shaderMonitor { "../assets/" };

    uint32_t pbrProgramAsset = -1;

    GlShaderHandle wireframeHandle{ "wireframe" };
    GlShaderHandle iblHandle{ "ibl" };
    GlMeshHandle cubeHandle{ "cube" };

    profiler<simple_cpu_timer> editorProfiler;

    std::unique_ptr<material_editor_window> material_editor;
    std::unique_ptr<selection_controller<GameObject>> gizmo_selector;

    std::unique_ptr<gui::imgui_instance> igm;
    std::unique_ptr<forward_renderer> renderer;
    std::unique_ptr<fullscreen_texture> fullscreen_surface;

    render_payload sceneData;
    Scene scene;

    ImGui::editor_app_log log;
    auto_layout uiSurface;
    std::vector<std::shared_ptr<GLTextureView>> debugViews;
    bool showUI = true;

    scene_editor_app();
    ~scene_editor_app();

    void reset_renderer(int2 size, const renderer_settings & settings);

    void on_window_resize(int2 size) override;
    void on_input(const InputEvent & event) override;
    void on_update(const UpdateEvent & e) override;
    void on_draw() override;
    void on_drop(std::vector <std::string> filepaths) override;
};