#version 330

out vec4 f_color;

in vec3 worldPos;
in vec3 norm;
in vec3 p;

uniform mat4 u_modelView;
uniform mat3 u_modelMatrixIT;
uniform vec3 u_lightPosition;
uniform vec3 u_eyePosition;
uniform vec4 u_clipPlane;
uniform vec3 u_surfaceColor;

#define FOG_DENSITY 0.015
#define FOG_COLOR vec3(1.0)

float exp_fog(const float dist, const float density) 
{
    float d = density * dist;
    return exp2(d * d * -1.44);
}

void main(void) 
{
    if (dot(u_clipPlane, vec4(worldPos,1)) < 0) discard;

    vec3 p0 = dFdx(p);
    vec3 p1 = dFdy(p);
    vec3 n = u_modelMatrixIT * normalize(cross(p0, p1));

    vec3 surfaceColor = u_surfaceColor;
    float ambientIntensity = 0.33;

    vec3 surfacePos = (u_modelView * vec4(p, 0.0)).xyz;
    vec3 surfaceToLight = normalize(u_lightPosition - surfacePos);

    vec3 ambient = ambientIntensity * surfaceColor;
    float diffuseCoefficient = max(0.0, dot(n, surfaceToLight));
    vec3 diffuse = diffuseCoefficient * surfaceColor;

    vec3 lightFactor = ambient + diffuse;

    f_color = vec4(lightFactor, 1.0);
    f_color.rgb = mix(f_color.rgb, FOG_COLOR, 1.0 - exp_fog(gl_FragCoord.z / gl_FragCoord.w, FOG_DENSITY));
}