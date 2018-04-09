#pragma once

#ifndef vr_material_hpp
#define vr_material_hpp

#include "gl-api.hpp"
#include "math-core.hpp"
#include "asset-defs.hpp"
#include "gl-shader-monitor.hpp"

namespace polymer
{

    struct Material
    {
        ShaderHandle shader;
        mutable std::shared_ptr<polymer::shader_variant> compiled_variant{ nullptr };
        virtual void update_uniforms() {}
        virtual void use() {}
        uint32_t id() const 
        { 
            if (!compiled_variant) compiled_variant = shader.get()->get_variant();
            return compiled_variant->shader.handle();
        }
    };

    struct DefaultMaterial final : public Material
    {
        DefaultMaterial() 
        { 
            shader = { "default-shader" }; 
        }

        void use() override 
        { 
            if (!compiled_variant) compiled_variant = shader.get()->get_variant();
            compiled_variant->shader.bind();
        }
    };

    class MetallicRoughnessMaterial final : public Material
    {
        int bindpoint = 0;

    public:

        void update_uniforms_shadow(GLuint handle);
        void update_uniforms_ibl(GLuint irradiance, GLuint radiance);
        void update_uniforms() override;
        void use() override;

        float3 baseAlbedo{1.f, 1.f, 1.f};

        float roughnessFactor{ 0.04f };
        float metallicFactor{ 1.f };

        float3 baseEmissive{ 0.f, 0.f, 0.f };
        float emissiveStrength{ 1.f };

        float specularLevel{ 0.04f };
        float occlusionStrength{ 1.f };
        float ambientStrength{ 1.f };

        float opacity{ 1.f };
        float shadowOpacity{ 1.f };

        int2 texcoordScale{ 1, 1 };

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
