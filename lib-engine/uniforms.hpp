#pragma once

#ifndef polymer_scene_uniforms
#define polymer_scene_uniforms

#include "util.hpp"
#include "math-common.hpp"

using namespace polymer;

namespace uniforms
{
    static const int MAX_POINT_LIGHTS = 4;
    static const int NUM_CASCADES = 2;

    struct point_light
    {
        ALIGNED(16) float3    color;
        ALIGNED(16) float3    position;
        float                 radius;
    };

    struct directional_light
    {
        ALIGNED(16) float3    color;
        ALIGNED(16) float3    direction;
        float                 amount; // constant
    };

    struct spot_light
    {
        ALIGNED(16) float3    color;
        ALIGNED(16) float3    direction;
        ALIGNED(16) float3    position;
        ALIGNED(16) float3    attenuation; // constant, linear, quadratic
        float                 cutoff;
    };

    struct per_scene
    {
        static const int      binding = 0;
        directional_light     directional_light;
        point_light           point_lights[MAX_POINT_LIGHTS];
        float                 time;
        int                   activePointLights;
        ALIGNED(8)  float2    resolution;
        ALIGNED(8)  float2    invResolution;
        ALIGNED(16) float4    cascadesPlane[NUM_CASCADES];
        ALIGNED(16) float4x4  cascadesMatrix[NUM_CASCADES];
        float                 cascadesNear[NUM_CASCADES];
        float                 cascadesFar[NUM_CASCADES];
    };

    struct per_view
    {
        static const int      binding = 1;
        ALIGNED(16) float4x4  view;
        ALIGNED(16) float4x4  viewProj;
        ALIGNED(16) float4    eyePos;
    };

    struct per_object
    {
        static const int      binding = 2;
        ALIGNED(16) float4x4  modelMatrix;
        ALIGNED(16) float4x4  modelMatrixIT;
        ALIGNED(16) float4x4  modelViewMatrix;
        ALIGNED(16) float     receiveShadow;
    };

}

#endif // end polymer_scene_uniforms
