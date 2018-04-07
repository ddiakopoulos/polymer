#include "material.hpp"
#include "fwd_renderer.hpp"

using namespace polymer;

//////////////////////////////////////////////////////
//   Physically-Based Metallic-Roughness Material   //
//////////////////////////////////////////////////////

void MetallicRoughnessMaterial::update_uniforms()
{
    bindpoint = 0;

    auto & shader = program.get();
    shader.bind();

    shader.uniform("u_roughness", roughnessFactor);
    shader.uniform("u_metallic", metallicFactor);
    shader.uniform("u_opacity", opacity);
    shader.uniform("u_albedo", baseAlbedo);
    shader.uniform("u_emissive", baseEmissive);
    shader.uniform("u_specularLevel", specularLevel);
    shader.uniform("u_occlusionStrength", occlusionStrength);
    shader.uniform("u_ambientStrength", ambientStrength);
    shader.uniform("u_emissiveStrength", emissiveStrength);
    shader.uniform("u_shadowOpacity", shadowOpacity);
    shader.uniform("u_texCoordScale", float2(texcoordScale));

    if (shader.has_define("HAS_ALBEDO_MAP")) shader.texture("s_albedo", bindpoint++, albedo.get(), GL_TEXTURE_2D);
    if (shader.has_define("HAS_NORMAL_MAP")) shader.texture("s_normal", bindpoint++, normal.get(), GL_TEXTURE_2D);
    if (shader.has_define("HAS_ROUGHNESS_MAP")) shader.texture("s_roughness", bindpoint++, roughness.get(), GL_TEXTURE_2D);
    if (shader.has_define("HAS_METALNESS_MAP")) shader.texture("s_metallic", bindpoint++, metallic.get(), GL_TEXTURE_2D);
   

    if (shader.has_define("HAS_EMISSIVE_MAP")) shader.texture("s_emissive", bindpoint++, emissive.get(), GL_TEXTURE_2D);
    if (shader.has_define("HAS_HEIGHT_MAP")) shader.texture("s_height", bindpoint++, height.get(), GL_TEXTURE_2D);
    if (shader.has_define("HAS_OCCLUSION_MAP")) shader.texture("s_occlusion", bindpoint++, occlusion.get(), GL_TEXTURE_2D);

    shader.unbind();
}

void MetallicRoughnessMaterial::update_uniforms_ibl(GLuint irradiance, GLuint radiance)
{
    auto & shader = program.get();
    shader.bind();
    if (shader.has_define("USE_IMAGE_BASED_LIGHTING")) shader.texture("sc_irradiance", bindpoint++, irradiance, GL_TEXTURE_CUBE_MAP);
    if (shader.has_define("USE_IMAGE_BASED_LIGHTING")) shader.texture("sc_radiance", bindpoint++, radiance, GL_TEXTURE_CUBE_MAP);
    shader.unbind();
}

void MetallicRoughnessMaterial::update_uniforms_shadow(GLuint handle)
{
    auto & shader = program.get();

    if (shader.has_define("ENABLE_SHADOWS"))
    {
        shader.bind();
        shader.texture("s_csmArray", bindpoint++, handle, GL_TEXTURE_2D_ARRAY);
        shader.unbind();
    }
    else
    {
        throw std::runtime_error("should not be called unless shadows are defined.");
    }
}

void MetallicRoughnessMaterial::use()
{
    auto & shader = program.get();
    shader.bind();
}
