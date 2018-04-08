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

/*
// Find missing Geometry and GlMesh asset_handles first
std::unordered_map<std::string, uint32_t> missingGeometryAssets;
std::unordered_map<std::string, uint32_t> missingMeshAssets;

for (auto & obj : scene->objects)
{
if (auto * mesh = dynamic_cast<StaticMesh*>(obj.get()))
{
bool foundGeom = false;
bool foundMesh = false;

for (auto & h : asset_handle<Geometry>::list())
{
if (h.name == mesh->geom.name) foundGeom = true;
}

for (auto & h : asset_handle<GlMesh>::list())
{
if (h.name == mesh->mesh.name) foundMesh = true;
}

if (!foundGeom) missingGeometryAssets[mesh->geom.name] += 1;
if (!foundMesh) missingMeshAssets[mesh->mesh.name] += 1;

}
}

for (auto & e : missingGeometryAssets) std::cout << "Asset table does not have " << e.first << " geometry required by " << e.second << " game object instances" << std::endl;
for (auto & e : missingMeshAssets) std::cout << "Asset table does not have " << e.first << " mesh required by " << e.second << " game object instances" << std::endl;
*/

template <typename T>
void remove_duplicates(std::vector<T> & vec)
{
    std::sort(vec.begin(), vec.end());
    vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
}

// The purpose of an asset resolver is to match an asset_handle to an asset on disk. This is done
// for scene objects (meshes, geometry) and materials (shaders, textures). 
class asset_resolver
{
    // Unresolved asset names
    std::vector<std::string> mesh_names;
    std::vector<std::string> geometry_names;
    std::vector<std::string> shader_names;
    std::vector<std::string> material_names;
    std::vector<std::string> texture_names;

    // What to do if we find multiples? 
    void walk_directory(path root)
    {
        scoped_timer t("load + resolve");

        for (auto & entry : recursive_directory_iterator(root))
        {
            const size_t root_len = root.string().length(), ext_len = entry.path().extension().string().length();
            auto path = entry.path().string(), name = path.substr(root_len + 1, path.size() - root_len - ext_len - 1);
            for (auto & chr : path) if (chr == '\\') chr = '/';

            const auto ext = entry.path().extension(); // also includes the dot

            if (ext == ".png" || ext == ".PNG" || ext == ".tga" || ext == ".jpg" || ext == ".jpeg")
            {
                const auto filename_no_extension = get_filename_without_extension(path);
                for (auto & name : texture_names)
                {
                    if (name == filename_no_extension)
                    {
                        create_handle_for_asset(name.c_str(), load_image(path, false));
                        std::cout << "resolved: " << name << std::endl;
                        Logger::get_instance()->assetLog->info("resolved {} ({})", name, typeid(GlTexture2D).name());
                    }
                }
            }

            if (ext == ".obj" || ".OBJ")
            {
                // todo - .mesh, .fbx, .gltf
                // all meshes are currently intrinsics, handled separately (for now)
            }
        }
    }

public:

    void resolve(const std::string & asset_dir, poly_scene * scene, material_library * library)
    {
        assert(scene != nullptr && library != nullptr && asset_dir.size() > 1);

        for (auto & obj : scene->objects)
        {
            if (auto * mesh = dynamic_cast<StaticMesh*>(obj.get()))
            {
                material_names.push_back(mesh->mat.name);
                mesh_names.push_back(mesh->mesh.name);
                geometry_names.push_back(mesh->geom.name);
            }
        }

        remove_duplicates(material_names);
        remove_duplicates(mesh_names);
        remove_duplicates(geometry_names);

        for (auto & mat : library->instances)
        {
            if (auto * pbr = dynamic_cast<MetallicRoughnessMaterial*>(mat.second.get()))
            {
                shader_names.push_back(pbr->program.name);
                texture_names.push_back(pbr->albedo.name);
                texture_names.push_back(pbr->normal.name);
                texture_names.push_back(pbr->metallic.name);
                texture_names.push_back(pbr->roughness.name);
                texture_names.push_back(pbr->emissive.name);
                texture_names.push_back(pbr->height.name);
                texture_names.push_back(pbr->occlusion.name);
            }
        }

        remove_duplicates(shader_names);
        remove_duplicates(texture_names);

        walk_directory(asset_dir);

        // todo - shader_names and material_names need to be resolved somewhat differently, since shaders have includes
        // and materials come from the library.
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
    void on_input(const InputEvent & event) override;
    void on_update(const UpdateEvent & e) override;
    void on_draw() override;
    void on_drop(std::vector <std::string> filepaths) override;
};