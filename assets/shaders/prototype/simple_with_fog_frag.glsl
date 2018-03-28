#version 330

uniform vec3 u_heightFogColor;
uniform sampler2D s_gradientFogTexture;

vec3 apply_fog(vec3 c, vec2 coord, float fogMultiplier)
{
    // Gradient Fog
    vec4 f = texture(s_gradientFogTexture, vec2(coord.x, 0.0f));
    c.rgb = mix(c.rgb, f.rgb * fogMultiplier, f.a);

    // Height Fog
    //c.rgb = mix(c.rgb, u_heightFogColor.rgb * fogMultiplier, coord.y);

    return c.rgb;
}

vec3 apply_fog(vec3 c, vec2 coord)
{
    return apply_fog(c, coord, 1.0);
}

///////////

uniform vec3 u_emissive;
uniform vec3 u_diffuse;
uniform vec3 u_eye;

struct PointLight
{
    vec3 position;
    vec3 color;
};

uniform PointLight u_lights[2];

in vec3 v_position;
in vec3 v_normal;
in vec2 v_fogCoord;

out vec4 f_color;

void main()
{
    vec3 eyeDir = normalize(u_eye - v_position);
    vec3 light = vec3(0, 0, 0);
    for(int i = 0; i < 2; ++i)
    {
        vec3 lightDir = normalize(u_lights[i].position - v_position);
        light += u_lights[i].color * u_diffuse * max(dot(v_normal, lightDir), 0);

        vec3 halfDir = normalize(lightDir + eyeDir);
        light += u_lights[i].color * u_diffuse * pow(max(dot(v_normal, halfDir), 0), 128);
    }
    //f_color = vec4(light + u_emissive, 1.0);
    f_color = vec4(1, 1, 1, 1); ////vec4(apply_fog(vec3(0, 0, 0), v_fogCoord), 1.0);
}