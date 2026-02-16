#version 330

layout(location=0) in vec3 position;
layout(location=1) in vec3 previous;
layout(location=2) in vec3 next;
layout(location=3) in float side;
layout(location=4) in float width;
layout(location=5) in vec2 uv;

uniform mat4 u_projMat;
uniform mat4 u_modelViewMat;

uniform vec2 resolution;
uniform float lineWidth;
uniform vec3 color;
uniform float opacity;
uniform float near;
uniform float far;
uniform float sizeAttenuation;

out vec2 vUV;
out vec4 vColor;
out vec3 vPosition;

vec2 fix(vec4 i, float aspect)
{
    vec2 res = i.xy / i.w;
    res.x *= aspect;
    return res;
}

void main()
{
    float aspect = resolution.x / resolution.y;
    float pixelWidthRatio = 1. / (resolution.x * u_projMat[0][0]);
    
    vColor = vec4(color, opacity);
    vUV = uv;
    
    mat4 m = u_projMat * u_modelViewMat;
    vec4 finalPosition = m * vec4(position, 1.0);
    vec4 prevPos = m * vec4(previous, 1.0);
    vec4 nextPos = m * vec4(next, 1.0);
    
    vec2 currentP = fix(finalPosition, aspect);
    vec2 prevP = fix(prevPos, aspect);
    vec2 nextP = fix(nextPos, aspect);
    
    float pixelWidth = finalPosition.w * pixelWidthRatio;
    float w = 1.8 * pixelWidth * lineWidth * width;
    
    if (sizeAttenuation == 1.)
    {
        w = 1.8 * lineWidth * width;
    }
    
    vec2 dir;
    
    if (nextP == currentP)
    {
        dir = normalize(currentP - prevP);
    }
    else if (prevP == currentP)
    {
        dir = normalize(nextP - currentP);
    }
    else
    {
        vec2 dir1 = normalize(currentP - prevP);
        vec2 dir2 = normalize(nextP - currentP);
        dir = normalize(dir1 + dir2);
        
        vec2 perp = vec2(-dir1.y, dir1.x);
        vec2 miter = vec2(-dir.y, dir.x);
    }
    
    vec2 normal = vec2(-dir.y, dir.x);
    normal.x /= aspect;
    normal *= .5 * w;
    
    vec4 offset = vec4(normal * side, 0.0, 1.0);
    finalPosition.xy += offset.xy;
    
    vPosition = (u_modelViewMat * vec4(position, 1.)).xyz;
    gl_Position = finalPosition;
}