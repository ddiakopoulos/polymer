#include "material.hpp"
#include "renderer-pbr.hpp"
#include "asset-handle.hpp"
#include "asset-handle-utils.hpp"
#include "shader.hpp"
#include "shader-library.hpp"

using namespace polymer;

//////////////////////////
//   Default Material   //
//////////////////////////

polymer_default_material::polymer_default_material()
{
    shader = shader_handle("default-shader");
}

void polymer_default_material::use()
{
    resolve_variants();
    compiled_shader->shader.bind();
}

void polymer_default_material::resolve_variants()
{
    if (!compiled_shader)
    {
        std::shared_ptr<gl_shader_asset> asset = shader.get();
        compiled_shader = asset->get_variant();
    }
}

uint32_t polymer_default_material::id()
{
    resolve_variants();
    return compiled_shader->shader.handle();
}

/////////////////////////////
//   Procedural Material   //
/////////////////////////////

polymer_procedural_material::polymer_procedural_material() {}

void polymer_procedural_material::use()
{
    if (!shader.assigned()) return;
    resolve_variants();
    compiled_shader->shader.bind();
}

void polymer_procedural_material::resolve_variants()
{
    if (!compiled_shader && shader.assigned())
    {
        std::shared_ptr<gl_shader_asset> asset = shader.get();
        compiled_shader = asset->get_variant();
    }
}

uint32_t polymer_procedural_material::id()
{
    if (!shader.assigned()) return 0;
    resolve_variants();
    return compiled_shader->shader.handle();
}

void polymer_procedural_material::update_uniforms()
{
    if (update_uniform_func)
    {
        resolve_variants();
        update_uniform_func();
    }
}

////////////////////////////
//   Wireframe Material   //
////////////////////////////

polymer_wireframe_material::polymer_wireframe_material()
{
    shader = shader_handle("renderer-wireframe");
    cast_shadows = false;
}

void polymer_wireframe_material::use()
{
    resolve_variants();
    compiled_shader->shader.bind();
    compiled_shader->shader.uniform("u_color", color);
}

void polymer_wireframe_material::resolve_variants()
{
    if (!compiled_shader)
    {
        std::shared_ptr<gl_shader_asset> asset = shader.get();
        compiled_shader = asset->get_variant();
    }
}

uint32_t polymer_wireframe_material::id()
{
    resolve_variants();
    return compiled_shader->shader.handle();
}

/////////////////////////////////////////
//   Lambertian Blinn-Phong Material   //
/////////////////////////////////////////

polymer_blinn_phong_standard::polymer_blinn_phong_standard()
{
    shader = shader_handle("phong-forward-lighting");
}

void polymer_blinn_phong_standard::resolve_variants()
{
    std::vector<std::string> processed_defines;

    // Required Features
    processed_defines.push_back("ENABLE_SHADOWS");
    processed_defines.push_back("TWO_CASCADES");
    processed_defines.push_back("USE_PCF_3X3");

    // Material slots
    if (diffuse.assigned()) processed_defines.push_back("HAS_DIFFUSE_MAP");
    if (normal.assigned()) processed_defines.push_back("HAS_NORMAL_MAP");

    const auto variant_hash = shader.get()->hash(processed_defines);

    // First time
    if (!compiled_shader)
    {
        compiled_shader = shader.get()->get_variant(processed_defines);
        return;
    }
    else if (compiled_shader->hash != variant_hash)
    {
        // We updated the set of defines and need to recompile
        compiled_shader = shader.get()->get_variant(processed_defines);
        return;
    }
}

uint32_t polymer_blinn_phong_standard::id()
{
    resolve_variants();
    return compiled_shader->shader.handle();
}

void polymer_blinn_phong_standard::use()
{
    resolve_variants();
    gl_shader & program = compiled_shader->shader;
    program.bind();
}

void polymer_blinn_phong_standard::update_uniforms()
{
    resolve_variants();
    gl_shader & program = compiled_shader->shader;
    program.bind();

    program.uniform("u_diffuseColor", diffuseColor);
    program.uniform("u_specularColor", specularColor);
    program.uniform("u_specularShininess", specularShininess);
    program.uniform("u_specularStrength", specularStrength);
    program.uniform("u_opacity", opacity);

    program.uniform("u_texCoordScale", float2(texcoordScale));
    bindpoint = 0;

    if (compiled_shader->enabled("HAS_DIFFUSE_MAP")) program.texture("s_diffuse", bindpoint++, diffuse.get(), GL_TEXTURE_2D);
    if (compiled_shader->enabled("HAS_NORMAL_MAP")) program.texture("s_normal", bindpoint++, normal.get(), GL_TEXTURE_2D);

    program.unbind();
}

void polymer_blinn_phong_standard::update_uniforms_shadow(GLuint handle)
{
    resolve_variants();
    gl_shader & program = compiled_shader->shader;
    if (!compiled_shader->enabled("ENABLE_SHADOWS")) throw std::runtime_error("should not be called unless ENABLE_SHADOWS is defined.");

    program.bind();
    program.texture("s_csmArray", bindpoint++, handle, GL_TEXTURE_2D_ARRAY);
    program.unbind();
}

//////////////////////////////////////////////////////
//   Physically-Based Metallic-Roughness Material   //
//////////////////////////////////////////////////////

polymer_pbr_standard::polymer_pbr_standard()
{
    shader = shader_handle("pbr-forward-lighting");
}

void polymer_pbr_standard::resolve_variants() 
{
    std::vector<std::string> processed_defines;

    // Required Features
    processed_defines.push_back("ENABLE_SHADOWS");
    processed_defines.push_back("TWO_CASCADES");
    processed_defines.push_back("USE_PCF_3X3");
    processed_defines.push_back("USE_IMAGE_BASED_LIGHTING");

    // Material slots
    if (albedo.assigned()) processed_defines.push_back("HAS_ALBEDO_MAP");
    if (roughness.assigned()) processed_defines.push_back("HAS_ROUGHNESS_MAP");
    if (metallic.assigned()) processed_defines.push_back("HAS_METALNESS_MAP");
    if (normal.assigned()) processed_defines.push_back("HAS_NORMAL_MAP");
    if (occlusion.assigned()) processed_defines.push_back("HAS_OCCLUSION_MAP");
    if (emissive.assigned()) processed_defines.push_back("HAS_EMISSIVE_MAP");

    const auto variant_hash = shader.get()->hash(processed_defines);

    // First time
    if (!compiled_shader)
    {
        compiled_shader = shader.get()->get_variant(processed_defines);
        return;
    }
    else if (compiled_shader->hash != variant_hash)
    {
        // We updated the set of defines and need to recompile
        compiled_shader = shader.get()->get_variant(processed_defines);
        return;
    }
}

uint32_t polymer_pbr_standard::id()
{
    resolve_variants();
    return compiled_shader->shader.handle();
}

void polymer_pbr_standard::update_uniforms()
{
    resolve_variants();
    gl_shader & program = compiled_shader->shader;
    program.bind();

    program.uniform("u_roughness", roughnessFactor);
    program.uniform("u_metallic", metallicFactor);
    program.uniform("u_opacity", opacity);
    program.uniform("u_albedo", baseAlbedo);
    program.uniform("u_emissive", baseEmissive);
    program.uniform("u_specularLevel", specularLevel);
    program.uniform("u_occlusionStrength", occlusionStrength);
    program.uniform("u_ambientStrength", ambientStrength);
    program.uniform("u_emissiveStrength", emissiveStrength);
    program.uniform("u_shadowOpacity", shadowOpacity);
    program.uniform("u_texCoordScale", texcoordScale);

    bindpoint = 0;

    if (compiled_shader->enabled("HAS_ALBEDO_MAP")) program.texture("s_albedo", bindpoint++, albedo.get(), GL_TEXTURE_2D);
    if (compiled_shader->enabled("HAS_NORMAL_MAP")) program.texture("s_normal", bindpoint++, normal.get(), GL_TEXTURE_2D);
    if (compiled_shader->enabled("HAS_ROUGHNESS_MAP")) program.texture("s_roughness", bindpoint++, roughness.get(), GL_TEXTURE_2D);
    if (compiled_shader->enabled("HAS_METALNESS_MAP")) program.texture("s_metallic", bindpoint++, metallic.get(), GL_TEXTURE_2D);
    if (compiled_shader->enabled("HAS_EMISSIVE_MAP")) program.texture("s_emissive", bindpoint++, emissive.get(), GL_TEXTURE_2D);
    if (compiled_shader->enabled("HAS_HEIGHT_MAP")) program.texture("s_height", bindpoint++, height.get(), GL_TEXTURE_2D);
    if (compiled_shader->enabled("HAS_OCCLUSION_MAP")) program.texture("s_occlusion", bindpoint++, occlusion.get(), GL_TEXTURE_2D);

    program.unbind();
}

void polymer_pbr_standard::update_uniforms_ibl(GLuint irradiance, GLuint radiance)
{
    resolve_variants();
    gl_shader & program = compiled_shader->shader;
    if (!compiled_shader->enabled("USE_IMAGE_BASED_LIGHTING")) throw std::runtime_error("should not be called unless USE_IMAGE_BASED_LIGHTING is defined.");

    program.bind();
    program.texture("sc_irradiance", bindpoint++, irradiance, GL_TEXTURE_CUBE_MAP);
    program.texture("sc_radiance", bindpoint++, radiance, GL_TEXTURE_CUBE_MAP);
    program.unbind();
}

void polymer_pbr_standard::update_uniforms_shadow(GLuint handle)
{
    resolve_variants();
    gl_shader & program = compiled_shader->shader;
    if (!compiled_shader->enabled("ENABLE_SHADOWS")) throw std::runtime_error("should not be called unless ENABLE_SHADOWS is defined.");

    program.bind();
    program.texture("s_csmArray", bindpoint++, handle, GL_TEXTURE_2D_ARRAY);
    program.unbind();
}

void polymer_pbr_standard::use()
{
    resolve_variants();
    compiled_shader->shader.bind();
}
