#include "polymer-engine/scene.hpp"
#include "polymer-engine/serialization.hpp"
#include "polymer-engine/asset/asset-resolver.hpp"
#include "polymer-engine/object.hpp"

#include "polymer-core/util/file-io.hpp"

using namespace polymer;

void scene::copy(entity src, entity dest)
{
    log::get()->engine_log->info("[scene] copied entity {} to {}", src.as_string(), dest.as_string());
}

void scene::destroy(entity e)
{
    if (e == kInvalidEntity) return;
}

void scene::import_environment(const std::string & import_path)
{
    manual_timer t;
    t.start();

    const std::string json_txt = read_file_text(import_path);
    const json env_doc = json::parse(json_txt);

    for (auto entityIterator = env_doc.begin(); entityIterator != env_doc.end(); ++entityIterator)
    {
        const entity parsed_entity = entityIterator.key().c_str();
    }

    t.stop();
    log::get()->engine_log->info("importing {} took {}ms", import_path, t.get());
}

void scene::export_environment(const std::string & export_path) 
{
    manual_timer t;
    t.start();

    json scene;

    // for (const auto & e : entity_list())
    // {
    //     json entity; 
    //     scene[e] = entity;
    // }

    write_file_text(export_path, scene.dump(4));

    t.stop();
    log::get()->engine_log->info("exporting {} took {}ms", export_path, t.get());
}

void scene::reset(int2 default_renderer_resolution, bool create_default_entities)
{
    // Create a material mat_library
    mat_library.reset(new polymer::material_library());

    // Resolving assets is the last thing we should do
    resolver.reset(new polymer::asset_resolver(this, mat_library.get()));
}
