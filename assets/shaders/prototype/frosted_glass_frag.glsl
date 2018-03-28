/*
    Quoting from http://www.adriancourreges.com/blog/2016/09/09/doom-2016-graphics-study/
    "Glasses render very nicely in DOOM especially frosted or dirty glasses: decals are used to affect just some part of the glass 
    to make its refraction more or less blurry. The pixel shader computes the refraction “blurriness” factor and selects from the
    blur chain the 2 maps closest to this blurriness factor. It reads from these 2 maps and then linearly interpolates between the 
    2 values to approximate the final blurry color the refraction is supposed to have. This is thanks to this process that glasses
    can produce nice refraction at different levels of blur on a per-pixel-basis."
*/

#version 330

#define NUM_MIPS 5.0

uniform mat4 u_viewProj;
uniform vec3 u_eye;

uniform sampler2D s_mip1;
uniform sampler2D s_mip2;
uniform sampler2D s_mip3;
uniform sampler2D s_mip4;
uniform sampler2D s_mip5;

uniform sampler2D s_frosted;

in vec3 v_position, v_normal;
in vec2 v_texcoord; 
in float v_fresnel;

out vec4 f_color;

// Converts a world space position to screen space position (NDC)
vec3 world_to_screen(vec3 world_pos) 
{
    vec4 proj = u_viewProj * vec4(world_pos, 1);
    proj.xyz /= proj.w;
    proj.xyz = proj.xyz * vec3(0.5) + vec3(0.5);
    return proj.xyz;
}

void main()
{
    float surfaceSmoothness = 1 - texture(s_frosted, v_texcoord).x;
    float blendValue = fract(surfaceSmoothness * (NUM_MIPS - 1.0));

    // planar refraction
    vec3 incident = normalize(v_position - u_eye);
    float surfaceIoR = .875;
    float refrDist = 0.5;
    vec3 refrVec = normalize(refract(incident, -v_normal, surfaceIoR)); 

    vec3 refrDest = v_position + refrVec * refrDist;
    vec2 texcoord = clamp(world_to_screen(refrDest).xy, 0, 1);

    vec3 refraction;
    if (surfaceSmoothness > (NUM_MIPS - 1.1) / (NUM_MIPS - 1.0)) 
    {
        refraction = texture(s_mip1, texcoord).xyz;
    }
    else if (surfaceSmoothness > (NUM_MIPS - 2.0) / (NUM_MIPS - 1.0)) 
    {
        refraction = mix(texture(s_mip2, texcoord).xyz, texture(s_mip1, texcoord).xyz, blendValue);
    }
    else if (surfaceSmoothness > (NUM_MIPS - 3.0) / (NUM_MIPS - 1.0)) 
    {
        refraction = mix(texture(s_mip3, texcoord).xyz, texture(s_mip2, texcoord).xyz, blendValue);
    }
    else if (surfaceSmoothness > (NUM_MIPS - 4.0) / (NUM_MIPS - 1.0)) 
    {
        refraction = mix(texture(s_mip4, texcoord).xyz, texture(s_mip3, texcoord).xyz, blendValue);
    }
    else refraction = mix(texture(s_mip5, texcoord).xyz, texture(s_mip4, texcoord).xyz, blendValue);

    f_color = vec4(refraction, 1 - surfaceSmoothness) * vec4(1 - surfaceSmoothness * v_fresnel);
}
