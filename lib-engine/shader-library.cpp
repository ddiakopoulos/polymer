#include "shader-library.hpp"

using namespace polymer;
using namespace std::experimental::filesystem;
using namespace std::chrono;

system_clock::time_point write_time(const std::string & file_path)
{
    try { return last_write_time(path(file_path)); }
    catch (...) { return system_clock::time_point::min(); };
}

gl_shader_monitor::gl_shader_monitor(const std::string & root_path) : root_path(root_path)
{
    watch_thread = std::thread([this, root_path]()
    {
        while (!watch_should_exit)
        {
            try
            {
                std::lock_guard<std::mutex> guard(watch_mutex);
                walk_asset_dir();
            }
            catch (const std::exception & e)
            {
                //@todo use logger
                std::cout << "Filesystem error: " << e.what() << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    });
}

gl_shader_monitor::~gl_shader_monitor()
{
    watch_should_exit = true;
    if (watch_thread.joinable()) watch_thread.join();
}

void gl_shader_monitor::watch(const std::string & name, const std::string & vert_path, const std::string & frag_path)
{
    auto asset = std::make_shared<gl_shader_asset>(name, vert_path, frag_path);
    assets[name] = asset;
    create_handle_for_asset(name.c_str(), std::move(asset));
}

void gl_shader_monitor::watch( const std::string & name, const std::string & vert_path, const std::string & frag_path, const std::string & include_path)
{
    auto asset = std::make_shared<gl_shader_asset>(name, vert_path, frag_path, "", include_path);
    assets[name] = asset;
    create_handle_for_asset(name.c_str(), std::move(asset));
}

void gl_shader_monitor::watch(const std::string & name, const std::string & vert_path, const std::string & frag_path, const std::string & geom_path, const std::string & include_path)
{
    auto asset = std::make_shared<gl_shader_asset>(name, vert_path, frag_path, geom_path, include_path);
    assets[name] = asset;
    create_handle_for_asset(name.c_str(), std::move(asset));
}

void gl_shader_monitor::walk_asset_dir()
{
    const path root = root_path;

    for (auto & entry : recursive_directory_iterator(root))
    {
        const size_t root_len = root.string().length(), ext_len = entry.path().extension().string().length();
        auto path = entry.path().string(), name = path.substr(root_len + 1, path.size() - root_len - ext_len - 1);
        for (auto & chr : path) if (chr == '\\') chr = '/';

        for (auto & asset : assets)
        {
            // Regular shader assets
            if (path == asset.second->vertexPath || path == asset.second->fragmentPath || path == asset.second->geomPath)
            {
                auto writeTime = duration_cast<seconds>(write_time(path).time_since_epoch()).count();
                if (writeTime > asset.second->writeTime)
                {
                    asset.second->writeTime = writeTime;
                    asset.second->shouldRecompile = true;
                    //@todo use logger
                    std::cout << "Modified Shader: " << asset.second->vertexPath << std::endl;
                }
            }

            // Each shader keeps a list of the files it includes. gl_shader_monitor watches a base path,
            // so we should be able to recompile shaders dependent on common includes
            for (auto & includePath : asset.second->includes)
            {
                if (get_filename_with_extension(path) == get_filename_with_extension(includePath))
                {
                    auto writeTime = duration_cast<seconds>(write_time(path).time_since_epoch()).count();
                    if (writeTime > asset.second->writeTime)
                    {
                        asset.second->writeTime = writeTime;
                        asset.second->shouldRecompile = true;

                        //@todo use logger
                        std::cout << "Modified Include: " << includePath << std::endl;
                        break;
                    }
                }
            }

        }
    }
}

void gl_shader_monitor::handle_recompile()
{
    for (auto & asset : assets)
    {
        if (asset.second->shouldRecompile)
        {
            std::lock_guard<std::mutex> guard(watch_mutex);

            asset.second->recompile_all();
            asset.second->shouldRecompile = false;
        }
    }
}