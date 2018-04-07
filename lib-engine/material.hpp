#pragma once

#ifndef vr_material_hpp
#define vr_material_hpp

#include "gl-api.hpp"
#include "math-core.hpp"
#include "asset-defs.hpp"

namespace polymer
{

    struct Material
    {
        GlShaderHandle program;
        virtual void update_uniforms() {}
        virtual void use() {}
        uint32_t id() const { return program.get().handle(); }
    };

    struct DefaultMaterial final : public Material
    {
        DefaultMaterial() { program = { "default-shader" }; }
        void use() override { program.get().bind(); }
    };

    class MetallicRoughnessMaterial final : public Material
    {
        int bindpoint = 0;

    public:

        void update_uniforms_shadow(GLuint handle);
        void update_uniforms_ibl(GLuint irradiance, GLuint radiance);
        void update_uniforms() override;
        void use() override;

        float3 baseAlbedo{ float3(1, 1, 1) };
        float opacity{ 1.f };

        float roughnessFactor{ 0.04f };
        float metallicFactor{ 1.f };

        float3 baseEmissive{ float3(0, 0, 0) };
        float emissiveStrength{ 1.f };

        float specularLevel{ 0.04f };
        float occlusionStrength{ 1.f };
        float ambientStrength{ 1.f };
        float shadowOpacity{ 1.f };

        int2 texcoordScale{ 4, 4 };

        GlTextureHandle albedo;
        GlTextureHandle normal;
        GlTextureHandle metallic;
        GlTextureHandle roughness;
        GlTextureHandle emissive;
        GlTextureHandle height;
        GlTextureHandle occlusion;
    };

}

#endif // end vr_material_hpp
