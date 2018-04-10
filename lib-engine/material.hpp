#pragma once

#ifndef vr_material_hpp
#define vr_material_hpp

#include "gl-api.hpp"
#include "math-core.hpp"
#include "asset-defs.hpp"
#include "gl-shader-monitor.hpp"

namespace polymer
{
    typedef std::shared_ptr<polymer::shader_variant> cached_variant;
    struct Material
    {
        mutable cached_variant compiled_shader{ nullptr };  // cached on first access (because needs to happen on GL thread)
        ShaderHandle shader;                                // typically set during object inflation / deserialization
        virtual void update_uniforms() {}                   // generic interface for overriding specific uniform sets
        virtual void use() {}                               // generic interface for binding the program
        virtual void resolve_variants() const = 0;          // all overridden functions need to call this to cache the shader
        virtual uint32_t id() const = 0;                    // returns the gl handle, used for sorting materials by type to minimize state changes in the renderer
    };

    struct DefaultMaterial final : public Material
    {
        DefaultMaterial() 
        { 
            shader = { "default-shader" }; 
        }

        virtual void use() override final
        { 
            resolve_variants();
            compiled_shader->shader.bind();
        }

        virtual void resolve_variants() const override final 
        { 
            if (!compiled_shader) compiled_shader = shader.get()->get_variant();
        }

        virtual uint32_t id() const override final
        {
            resolve_variants();
            return compiled_shader->shader.handle();
        }
    };

    class MetallicRoughnessMaterial final : public Material
    {
        int bindpoint = 0;

        std::vector<std::string> required_defines = { "TWO_CASCADES", "USE_PCF_3X3", "ENABLE_SHADOWS", "USE_IMAGE_BASED_LIGHTING",
            "HAS_ROUGHNESS_MAP", "HAS_METALNESS_MAP", "HAS_ALBEDO_MAP", "HAS_NORMAL_MAP", "HAS_OCCLUSION_MAP" };

    public:

        void update_uniforms_shadow(GLuint handle);
        void update_uniforms_ibl(GLuint irradiance, GLuint radiance);

        virtual void update_uniforms() override final;
        virtual void use() override final;
        virtual void resolve_variants() const override final;
        virtual uint32_t id() const override final;

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
