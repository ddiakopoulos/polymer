#version 330

uniform mat4 u_modelMatrix;
uniform mat4 u_modelMatrixIT;
uniform mat4 u_viewProj;
uniform vec3 u_eye;

uniform vec2 u_gradientFogScaleAdd;
uniform vec3 u_gradientFogLimitColor;
uniform vec3 u_heightFogParams;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inVertexColor;
layout(location = 3) in vec2 inTexcoord;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBitangent;

out vec3 v_position, v_normal;
out vec2 v_texcoord;
out vec3 v_eyeDir;
out vec2 v_fogCoord;

vec2 calculate_fog_coords(vec3 worldPos)
{
    vec2 results = vec2(0.0, 0.0);

    // Gradient fog
    float d = distance(worldPos, u_eye);
    results.x = clamp(u_gradientFogScaleAdd.x * d + u_gradientFogScaleAdd.y, 0, 1);

    // Height fog
    vec3 cameraToPositionRayWs = worldPos.xyz - u_eye.xyz;
    float cameraToPositionDist = length(cameraToPositionRayWs.xyz);
    vec3 cameraToPositionDirWs = normalize(cameraToPositionRayWs.xyz);
    float h = u_eye.y - u_heightFogParams.z;

    results.y = u_heightFogParams.x * exp(-h * u_heightFogParams.y) *
        (1.0 - exp(-cameraToPositionDist * cameraToPositionDirWs.y * u_heightFogParams.y)) / cameraToPositionDirWs.y;
    
    return clamp(results.xy, 0, 1);
}

void main()
{
    vec4 worldPos = u_modelMatrix * vec4(inPosition, 1);
    v_position = worldPos.xyz;
    v_normal = normalize((u_modelMatrixIT * vec4(inNormal,0)).xyz);
    v_texcoord = inTexcoord;
    v_eyeDir = normalize(u_eye - worldPos.xyz);
    v_fogCoord = calculate_fog_coords(worldPos.xyz);
    gl_Position = u_viewProj * worldPos;
}