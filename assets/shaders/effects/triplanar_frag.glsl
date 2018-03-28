#version 330

uniform float u_time;
uniform vec3 u_eye;
uniform vec3 u_scale = vec3(0.25, 0.25, 0.25);

uniform sampler2D s_diffuseTextureA, s_diffuseTextureB;

in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_texcoord;
in vec3 v_eyeDir;

out vec4 f_color;

const float yCapHeight = 0.5;
const float yCapTransition = 1.0;
const float blendSharpness = 0.8;
const float occlusionStrength = 1.0;

void main()
{
    // calculate a blend factor for triplanar mapping. Sharpness should be obvious.
    vec3 blendFactor = pow(abs(v_normal), vec3(blendSharpness));
    blendFactor /= dot(blendFactor, vec3(1.0));

    // compute texcoords for each axis based on world position of the fragment
    vec2 tx = v_world_position.yz * u_scale.x;
    vec2 ty = v_world_position.zx * u_scale.y;
    vec2 tz = v_world_position.xy * u_scale.z;

    vec4 cx = texture(s_diffuseTextureA, tx);
    vec4 cy = texture(s_diffuseTextureB, ty);
    vec4 cz = texture(s_diffuseTextureA, tz); // texture C here

    float h = clamp(v_world_position.y - yCapHeight, 0, yCapTransition) / yCapTransition;
    vec3 yDiff = cy.rgb * h;

    /*
    vec3 nx = (texture(s_normalTexture, tx).xyz * 2 - 1) * blendFactor.x;
    vec3 ny = (texture(s_normalTexture, ty).xyz * 2 - 1) * blendFactor.y;
    vec3 nz = (texture(s_normalTexture, tz).xyz * 2 - 1) * blendFactor.z;
    vec3 normal = nx + ny + nz;

    float ox = texture(s_occlusionTexture, tx).g * blendFactor.x;
    float oy = texture(s_occlusionTexture, ty).g * blendFactor.y;
    float oz = texture(s_occlusionTexture, tz).g * blendFactor.z;
    float occlusion = mix(1.0, ox + oy + oz, occlusionStrength);
    */

    f_color = vec4((cx.rgb * blendFactor.x) + (yDiff * blendFactor.y) + (cz.rgb * blendFactor.z), 1);
}