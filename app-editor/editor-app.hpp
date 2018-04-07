#include "index.hpp"
#include "gl-gizmo.hpp"
#include "gl-imgui.hpp"

#include "material.hpp"
#include "fwd_renderer.hpp"
#include "uniforms.hpp"
#include "asset-defs.hpp"
#include "scene.hpp"
#include "editor-ui.hpp"
#include "arcball.hpp"
#include "selection.hpp"

struct fullscreen_texture
{
    GlShader shader;
    GlMesh fullscreen_quad_ndc;

    fullscreen_texture()
    {
        static const char s_textureVert[] = R"(#version 330
            layout(location = 0) in vec3 position;
            layout(location = 1) in vec2 uvs;
            uniform mat4 u_mvp = mat4(1);
            out vec2 texCoord;
            void main()
            {
                texCoord = uvs;
                gl_Position = u_mvp * vec4(position.xy, 0.0, 1.0);
    
            }
        )";

        static const char s_textureFrag[] = R"(#version 330
            uniform sampler2D s_texture;
            in vec2 texCoord;
            out vec4 f_color;
            void main()
            {
                vec4 sample = texture(s_texture, texCoord);
                f_color = vec4(sample.rgb, 1.0);
            }
        )";

        shader = GlShader(s_textureVert, s_textureFrag);

        struct Vertex { float3 position; float2 texcoord; };
        const float3 verts[6] = { { -1.0f, -1.0f, 0.0f },{ 1.0f, -1.0f, 0.0f },{ -1.0f, 1.0f, 0.0f },{ -1.0f, 1.0f, 0.0f },{ 1.0f, -1.0f, 0.0f },{ 1.0f, 1.0f, 0.0f } };
        const float2 texcoords[6] = { { 0, 0 },{ 1, 0 },{ 0, 1 },{ 0, 1 },{ 1, 0 },{ 1, 1 } };
        const uint3 faces[2] = { { 0, 1, 2 },{ 3, 4, 5 } };
        std::vector<Vertex> vertices;
        for (int i = 0; i < 6; ++i) vertices.push_back({ verts[i], texcoords[i] });

        fullscreen_quad_ndc.set_vertices(vertices, GL_STATIC_DRAW);
        fullscreen_quad_ndc.set_attribute(0, &Vertex::position);
        fullscreen_quad_ndc.set_attribute(1, &Vertex::texcoord);
        fullscreen_quad_ndc.set_elements(faces, GL_STATIC_DRAW);
    }

    void draw(GLuint texture_handle)
    {
        shader.bind();
        shader.texture("s_texture", 0, texture_handle, GL_TEXTURE_2D);
        fullscreen_quad_ndc.draw_elements();
        shader.unbind();
    }
};

template<typename AssetHandleType>
void draw_listbox(const std::string & label, ImGuiTextFilter & filter, int & selection)
{
    std::vector<std::string> assets;
    for (auto & m : AssetHandleType::list()) assets.push_back(m.name);

    ImGui::Text(label.c_str());

    ImGui::PushItemWidth(-1);
    if (ImGui::ListBoxHeader("##assets"))
    {
        for (int n = 0; n < assets.size(); ++n)
        {
            const std::string name = assets[n];
            if (!filter.PassFilter(name.c_str())) continue;
            if (ImGui::Selectable(name.c_str(), n == selection))
            {
                selection = n;
            }
        }

        ImGui::ListBoxFooter();
    }
    ImGui::PopItemWidth();
}

struct aux_window final : public glfw_window
{
    std::unique_ptr<fullscreen_texture> fullscreen_surface;
    std::unique_ptr<forward_renderer> preview_renderer;
    std::unique_ptr<gui::imgui_instance> auxImgui;
    std::unique_ptr<StaticMesh> previewMesh;
    std::unique_ptr<arcball_controller> arcball;

    perspective_camera previewCam;
    render_payload previewSceneData;

    int assetSelection = -1;
    const uint32_t previewHeight = 400;
    aux_window(gl_context * context, int w, int h, const std::string title, int samples) : glfw_window(context, w, h, title, samples) 
    { 
        glfwMakeContextCurrent(window);

        fullscreen_surface.reset(new fullscreen_texture());

        // These are created on the this individual context and cached as global variables. It will be re-assigned...
        auto cap = make_icosasphere(3);
        create_handle_for_asset("preview-cube", make_mesh_from_geometry(cap));
        create_handle_for_asset("preview-cube", std::move(cap));

        previewMesh.reset(new StaticMesh());
        previewMesh->mesh = "preview-cube";
        previewMesh->geom = "preview-cube";
        previewMesh->mat = "pbr-material/floor"; 
        previewMesh->pose.position = float3(0, 0, -1);

        renderer_settings previewSettings;
        previewSettings.renderSize = int2(w, previewHeight);
        previewSettings.msaaSamples = 8;
        previewSettings.performanceProfiling = false;
        previewSettings.useDepthPrepass = false;
        previewSettings.tonemapEnabled = false;
        previewSettings.shadowsEnabled = false;

        preview_renderer.reset(new forward_renderer(previewSettings));

        previewCam.pose = look_at_pose_rh(float3(0, 0.25f, 2), previewMesh->pose.position);
        auxImgui.reset(new gui::imgui_instance(window, true));

        auto fontAwesomeBytes = read_file_binary("../assets/fonts/font_awesome_4.ttf");
        auxImgui->append_icon_font(fontAwesomeBytes);

        arcball.reset(new arcball_controller({ (float)w, (float)h }));

        gui::make_light_theme();
    }

    virtual void on_input(const polymer::InputEvent & e) override final
    {
        if (e.window == window) auxImgui->update_input(e);

        if (ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard)
        {
            return;
        }

        if (e.type == InputEvent::MOUSE && e.is_down())
        {
            arcball->mouse_down(e.cursor);
        }
        else if (e.type == InputEvent::CURSOR && e.drag)
        {
            arcball->mouse_drag(e.cursor);
            previewMesh->pose.orientation = safe_normalize(qmul(arcball->currentQuat, previewMesh->pose.orientation));
        }
    }

    virtual void on_window_close() override final
    {
        glfwMakeContextCurrent(window);

        // Why do we do all these resets? Well, the way the GlObject handle system works, the
        // destructor is called on the context of the destroyer. In the case of a secondary window,
        // that would be the main thread. Instead, we need to manually clean up everything here before
        // the destructor is called. 
        fullscreen_surface.reset();
        preview_renderer.reset();
        auxImgui.reset();
        previewMesh.reset();

        asset_handle<GlMesh>::destroy("preview-cube");
        asset_handle<Geometry>::destroy("preview-cube");

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
            glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Single-viewport camera
            previewSceneData = {};
            previewSceneData.clear_color = float4(0, 0, 0, 0);
            previewSceneData.renderSet.push_back(previewMesh.get());
            previewSceneData.ibl_irradianceCubemap = "wells-irradiance-cubemap";
            previewSceneData.ibl_radianceCubemap = "wells-radiance-cubemap";
            previewSceneData.views.push_back(view_data(0, previewCam.pose, previewCam.get_projection_matrix(width / float(previewHeight))));
            preview_renderer->render_frame(previewSceneData);

            glUseProgram(0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, width, height);

            auxImgui->begin_frame();
            gui::imgui_fixed_window_begin("material-editor", { { 0, 0 },{ width, int(height - previewHeight) } });

            ImGui::Dummy({ 0, 8 });
            ImGuiTextFilter textFilter;
            textFilter.Draw(" " ICON_FA_SEARCH "  ");
            ImGui::Dummy({ 0, 12 });

            draw_listbox<MaterialHandle>("Materials", textFilter, assetSelection);

            ImGui::Dummy({ 0, 12 });

            if (assetSelection >= 0)
            {
                auto mat = asset_handle<std::shared_ptr<Material>>::list()[assetSelection].get();
                inspect_object(nullptr, mat.get());
            }

            ImGui::Dummy({ 0, 12 });

            gui::imgui_fixed_window_end();
            auxImgui->end_frame();

            glViewport(0, 0, width, previewHeight);
            fullscreen_surface->draw(preview_renderer->get_color_texture(0));

            /*
            auxImgui->begin_frame();
            gui::imgui_fixed_window_begin("asset-browser", { {0, 0}, {width, height} });

            ImGui::Dummy({ 0, 12 });

            ImGui::Text(ICON_FA_FILE_IMAGE_O " Asset Types");
            ImGui::Dummy({ 0, 6 });
            ImGui::PushItemWidth(-1);
            std::vector<std::string> assetTypes = { {"Shaders", "Materials", "Textures", "GPU Mesh", "CPU Geometry"} };
            static int assetTypeSelection = -1;
            gui::Combo("##asset_type", &assetTypeSelection, assetTypes, 20);
            ImGui::PopItemWidth();

            ImGui::Dummy({ 0, 8 });

            ImGui::Separator();

            ImGui::Dummy({ 0, 8 });

            ImGuiTextFilter textFilter;
            textFilter.Draw(" " ICON_FA_SEARCH "  ");

            ImGui::Dummy({ 0, 12 });

            switch (assetTypeSelection)
            {
                case 0: draw_listbox<GlShaderHandle>("Shaders", textFilter, assetSelection); break;
                case 1: draw_listbox<MaterialHandle>("Materials", textFilter, assetSelection); break;
                case 2: draw_listbox<GlTextureHandle>("Textures", textFilter, assetSelection); break;
                case 3: draw_listbox<GlMeshHandle>("GPU Geometry", textFilter, assetSelection); break;
                case 4: draw_listbox<GeometryHandle>("CPU Geometry", textFilter, assetSelection); break;
                default: break;
            }
            gui::imgui_fixed_window_end();
            auxImgui->end_frame();
            */


            glFlush();

            glfwSwapBuffers(window);
        }
    }

    ~aux_window() 
    { 
        if (window)
        {
            glfwDestroyWindow(window);
            window = nullptr;
        }
    }
};

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

    std::unique_ptr<aux_window> auxWindow;

    std::unique_ptr<gui::imgui_instance> igm;
    std::unique_ptr<selection_controller<GameObject>> editor;
    std::unique_ptr<forward_renderer> renderer;
    std::unique_ptr<fullscreen_texture> fullscreen_surface;

    Scene scene;
    render_payload sceneData;

    ImGui::ImGuiAppLog log;
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