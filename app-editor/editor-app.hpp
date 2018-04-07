#include "index.hpp"
#include "gl-gizmo.hpp"
#include "gl-imgui.hpp"

#include "material.hpp"
#include "fwd_renderer.hpp"
#include "uniforms.hpp"
#include "assets.hpp"
#include "scene.hpp"
#include "editor-ui.hpp"
#include "arcball.hpp"

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
        previewMesh->pose.position = float3(0, 0, -2);

        renderer_settings previewSettings;
        previewSettings.renderSize = int2(w, h);
        previewSettings.msaaSamples = 8;
        previewSettings.performanceProfiling = false;
        previewSettings.useDepthPrepass = false;
        previewSettings.tonemapEnabled = false;
        previewSettings.shadowsEnabled = false;
        preview_renderer.reset(new forward_renderer(previewSettings));

        previewCam.pose = look_at_pose_rh(float3(0, 0.25f, 4), previewMesh->pose.position);
        auxImgui.reset(new gui::imgui_instance(window, true));

        auto fontAwesomeBytes = read_file_binary("../assets/fonts/font_awesome_4.ttf");
        auxImgui->append_icon_font(fontAwesomeBytes);

        arcball.reset(new arcball_controller({ (float)w, (float)h }));

        gui::make_light_theme();
    }

    virtual void on_input(const polymer::InputEvent & e) override final
    {
        if (e.window == window) auxImgui->update_input(e);

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
        // it would be the main thread. Instead, we need to manually clean up everything here. 
        fullscreen_surface.reset();
        preview_renderer.reset();
        auxImgui.reset();
        previewMesh.reset();

        AssetHandle<GlMesh>::destroy("preview-cube");
        AssetHandle<Geometry>::destroy("preview-cube");

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
            previewSceneData.clear_color = float4(0, 0, 0, 1);
            previewSceneData.renderSet.push_back(previewMesh.get());
            previewSceneData.views.push_back(view_data(0, previewCam.pose, previewCam.get_projection_matrix(width / float(height))));
            preview_renderer->render_frame(previewSceneData);

            glUseProgram(0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, width, height);

            fullscreen_surface->draw(preview_renderer->get_color_texture(0));

            gl_check_error(__FILE__, __LINE__);

            glFlush();

            glfwSwapBuffers(window);

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

static inline Pose to_linalg(tinygizmo::rigid_transform & t)
{
    return{ reinterpret_cast<float4 &>(t.orientation), reinterpret_cast<float3 &>(t.position) };
}

static inline tinygizmo::rigid_transform from_linalg(Pose & p)
{
    return{ reinterpret_cast<minalg::float4 &>(p.orientation), reinterpret_cast<minalg::float3 &>(p.position) };
}

template<typename ObjectType>
class editor_controller
{
    GlGizmo gizmo;
    tinygizmo::rigid_transform gizmo_selection;     // Center of mass of multiple objects or the pose of a single object
    tinygizmo::rigid_transform last_gizmo_selection; 

    Pose selection;
    std::vector<ObjectType *> selected_objects;     // Array of selected objects
    std::vector<Pose> relative_transforms;          // Pose of the objects relative to the selection
    
    bool gizmo_active = false;

    void compute_selection()
    {
        // No selected objects? The selection pose is nil
        if (selected_objects.size() == 0)
        {
            selection = {};
        }
        // Single object selection
        else if (selected_objects.size() == 1)
        {
            selection = (*selected_objects.begin())->get_pose();
        }
        // Multi-object selection
        else
        {
            // todo: orientation... bounding boxes?
            float3 center_of_mass = {};
            float numObjects = 0;
            for (auto & obj : selected_objects)
            {
                center_of_mass += obj->get_pose().position;
                numObjects++;
            }
            center_of_mass /= numObjects;
            selection.position = center_of_mass;
        }

        compute_relative_transforms();

        gizmo_selection = from_linalg(selection);

    }

    void compute_relative_transforms()
    {
        relative_transforms.clear();

        for (auto & s : selected_objects)
        {
            relative_transforms.push_back(selection.inverse() * s->get_pose());
        }
    }

public:

    editor_controller() { }

    bool selected(ObjectType * object) const
    {
        return std::find(selected_objects.begin(), selected_objects.end(), object) != selected_objects.end();
    }

    std::vector<ObjectType *> & get_selection()
    {
        return selected_objects;
    }

    void set_selection(const std::vector<ObjectType *> & new_selection)
    {
        selected_objects = new_selection;
        compute_selection();
    }

    void update_selection(ObjectType * object)
    { 
        auto it = std::find(std::begin(selected_objects), std::end(selected_objects), object);
        if (it == std::end(selected_objects)) selected_objects.push_back(object);
        else selected_objects.erase(it);
        compute_selection();
    }

    void clear()
    {
        selected_objects.clear();
        compute_selection();
    }

    bool active() const
    {
        return gizmo_active;
    }
    
    void on_input(const InputEvent & event)
    {
        gizmo.handle_input(event);
    }

    void reset_input()
    {
        gizmo.reset_input();
    }

    void on_update(const perspective_camera & camera, const float2 viewport_size)
    {
        gizmo.update(camera, viewport_size);
        gizmo_active = tinygizmo::transform_gizmo("editor-controller", gizmo.gizmo_ctx, gizmo_selection);

        // Perform editing updates on selected objects
        if (gizmo_selection != last_gizmo_selection)
        {
            for (int i = 0; i < selected_objects.size(); ++i)
            {
                ObjectType * object = selected_objects[i];
                auto newPose = to_linalg(gizmo_selection) * relative_transforms[i];
                object->set_pose(newPose);
            }
        }

        last_gizmo_selection = gizmo_selection;
    }

    void on_draw()
    {
        if (selected_objects.size())
        {
            gizmo.draw();
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

    profiler<SimpleTimer> editorProfiler;

    std::unique_ptr<aux_window> auxWindow;

    std::unique_ptr<gui::imgui_instance> igm;
    std::unique_ptr<editor_controller<GameObject>> editor;
    std::unique_ptr<forward_renderer> renderer;
    std::unique_ptr<fullscreen_texture> fullscreen_surface;

    Scene scene;
    render_payload sceneData;

    ImGui::ImGuiAppLog log;
    auto_layout uiSurface;
    std::vector<std::shared_ptr<GLTextureView>> debugViews;
    bool showUI = true;
    std::vector<ui_rect> active_imgui_regions;

    scene_editor_app();
    ~scene_editor_app();

    void reset_renderer(int2 size, const renderer_settings & settings);

    void on_window_resize(int2 size) override;
    void on_input(const InputEvent & event) override;
    void on_update(const UpdateEvent & e) override;
    void on_draw() override;
    void on_drop(std::vector <std::string> filepaths) override;
};