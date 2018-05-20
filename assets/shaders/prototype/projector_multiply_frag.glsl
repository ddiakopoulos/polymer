#version 330

in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_texcoord;
in vec3 v_eyeDir;
in vec4 v_projector_coords;

uniform sampler2D s_cookieTex;

out vec4 f_color;

// Helpers to sample coordinates that have been projected
vec4 tex1Dproj(sampler1D samp, vec2 texcoord) { return textureProj(samp, texcoord); }
vec4 tex2Dproj(sampler2D samp, vec3 texcoord) { return textureProj(samp, texcoord); }
vec4 tex3Dproj(sampler3D samp, vec4 texcoord) { return textureProj(samp, texcoord); }

void main()
{
    vec4 sample = vec4(0, 0, 0, 1);

    // Front-facing check
    if (v_projector_coords.w > 0)
    {
        // v_projector_coords are uv coords in texture space, .xyw since textureProj will divide by 3rd component for us
        sample = tex2Dproj(s_cookieTex, v_projector_coords.xyw);

        // The above statement is equivalent to: 
        // vec2 projected_texcoord = vec2(v_uv_shadow.xy / v_uv_shadow.w);
        // sample = texture(s_cookieTex, projected_texcoord);

        //sample.a = 1.0 - sample.a;
    }

    f_color = sample;
}