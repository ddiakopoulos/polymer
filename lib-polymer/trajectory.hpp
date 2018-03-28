// This software is dual-licensed to the public domain and under the following
// license: you are granted a perpetual, irrevocable license to copy, modify,
// publish, and distribute this file as you see fit. Authored in 2016 by Forrest Smith.
// Original Source: https://github.com/forrestthewoods/lib_fts/blob/master/code/fts_ballistic_trajectory.cs

#pragma once

#ifndef trajectory_hpp
#define trajectory_hpp

#include "util.hpp"
#include "math-core.hpp"
#include "solvers.hpp"

using namespace avl;

// Calculate the maximum range that a ballistic projectile can be fired on given speed and gravity.
//
// speed (float): projectile velocity
// gravity (float): force of gravity, positive is down
// initial_height (float): distance above flat terrain
//
// return (float): maximum range
inline float ballistic_range(float speed, float gravity, float initial_height) 
{
    if (speed < 0 || gravity < 0 || initial_height <= 0) throw std::range_error("invalid initial conditions");

    float angle = 45.0 * ANVIL_DEG_TO_RAD; // no air resistence, so 45 degrees provides maximum range
    float cos = std::cos(angle);
    float sin = std::sin(angle);

    float range = (speed*cos/gravity) * (speed*sin + std::sqrt(speed*speed*sin*sin + 2*gravity*initial_height));
    return range;
}

// Solve firing angles for a ballistic projectile with speed and gravity to hit a fixed position.
//
// origin (float3): point projectile will fire from
// speed (float): scalar speed of projectile
// target (float3): point projectile is trying to hit
// gravity (float): force of gravity, positive down
//
// s0 (out float3): firing solution (low angle) 
// s1 (out float3): firing solution (high angle)
//
// return (int): number of unique solutions found: 0, 1, or 2.
inline int solve_ballistic_arc(const float3 origin, const float speed, const float3 target, const float gravity, float3 & s0, float3 & s1) 
{
    if (origin == target || speed < 0 || gravity < 0) throw std::range_error("invalid initial conditions");

    float3 diff = target - origin;
    float3 diffXZ = float3(diff.x, 0.f, diff.z);
    float groundDist = length(diffXZ);

    float speed2 = speed*speed;
    float speed4 = speed*speed*speed*speed;
    float y = diff.y;
    float x = groundDist;
    float gx = gravity*x;

    float root = speed4 - gravity*(gravity*x*x + 2*y*speed2);

    // No solution
    if (root < 0) return 0;

    root = std::sqrt(root);

    float lowAng = std::atan2(speed2 - root, gx);
    float highAng = std::atan2(speed2 + root, gx);
    int numSolutions = lowAng != highAng ? 2 : 1;

    const float3 worldUp = float3(0, 1, 0);
    const float3 groundDir = normalize(diffXZ);
    s0 = groundDir * std::cos(lowAng)*speed + worldUp * std::sin(lowAng) * speed;
    if (numSolutions > 1)
    {
        s1 = groundDir * std::cos(highAng)*speed + worldUp * std::sin(highAng) * speed;
    }

    return numSolutions;
}

// Solve firing angles for a ballistic projectile with speed and gravity to hit a target moving with constant, linear velocity.
//
// proj_pos (float3): point projectile will fire from
// proj_speed (float): scalar speed of projectile
// target (float3): point projectile is trying to hit
// target_velocity (float3): velocity of target
// gravity (float): force of gravity, positive down
//
// s0 (out float3): firing solution (fastest time impact) 
// s1 (out float3): firing solution (next impact)
// s2 (out float3): firing solution (next impact)
// s3 (out float3): firing solution (next impact)
//
// return (int): number of unique solutions found: 0, 1, 2, 3, or 4.
inline int solve_ballistic_arc(const float3 proj_pos, const float proj_speed, const float3 target_pos, const float3 target_velocity, const float gravity, float3 & s0, float3 & s1) 
{
    double G = gravity;

    double A = proj_pos.x;
    double B = proj_pos.y;
    double C = proj_pos.z;
    double M = target_pos.x;
    double N = target_pos.y;
    double O = target_pos.z;
    double P = target_velocity.x;
    double Q = target_velocity.y;
    double R = target_velocity.z;
    double S = proj_speed;

    double H = M - A;
    double J = O - C;
    double K = N - B;
    double L = -.5f * G;

    // Quartic Coeffecients
    double c0 = L*L;
    double c1 = 2*Q*L;
    double c2 = Q*Q + 2*K*L - S*S + P*P + R*R;
    double c3 = 2*K*Q + 2*H*P + 2*J*R;
    double c4 = K*K + H*H + J*J;

    // Solve quartic
    std::array<double, 4> times;
    int numTimes = solve_quartic(c0, c1, c2, c3, c4, times[0], times[1], times[2], times[3]);

    // Sort so faster collision is found first
    std::sort(times.begin(), times.end());

    // Plug quartic solutions into base equations
    // There should never be more than 2 positive, real roots.
    std::vector<float3> solutions(2);
    int numSolutions = 0;

    for (int i = 0; i < numTimes && numSolutions < 2; ++i) 
    {
        double t = times[i];
        if ( t <= 0) continue;

        solutions[numSolutions].x = (float)((H+P*t)/t);
        solutions[numSolutions].y = (float)((K+Q*t-L*t*t)/ t);
        solutions[numSolutions].z = (float)((J+R*t)/t);
        ++numSolutions;
    }

    // Write out solutions
    if (numSolutions > 0) s0 = solutions[0];
    if (numSolutions > 1) s1 = solutions[1];

    return numSolutions;
}

// Solve the firing arc with a fixed lateral speed. Vertical speed and gravity varies. 
// This enables a visually pleasing arc.
//
// proj_pos (float3): point projectile will fire from
// lateral_speed (float): scalar speed of projectile along XZ plane
// target_pos (float3): point projectile is trying to hit
// max_height (float): height above Max(proj_pos, impact_pos) for projectile to peak at
//
// fire_velocity (out float3): firing velocity
// gravity (out float): gravity necessary to projectile to hit precisely max_height
//
// return (bool): true if a valid solution was found
inline bool solve_ballistic_arc_lateral(const float3 proj_pos, const float lateral_speed, const float3 target_pos, const float max_height, float3 & fire_velocity, float & gravity) 
{
    if (proj_pos == target_pos || lateral_speed < 0 || max_height < proj_pos.y) throw std::range_error("invalid initial conditions");

    float3 diff = target_pos - proj_pos;
    float3 diffXZ = float3(diff.x, 0.f, diff.z);
    float lateralDist = length(diffXZ);

    if (lateralDist == 0) return false;

    float time = lateralDist / lateral_speed;

    fire_velocity = normalize(diffXZ) * lateral_speed;

    // System of equations. Hit max_height at t=.5*time. Hit target at t=time.
    float a = proj_pos.y; // initial
    float b = max_height; // peak
    float c = target_pos.y; // final

    gravity = -4*(a - 2*b + c) / (time * time);
    fire_velocity.y = -(3*a - 4*b + c) / time;

    return true;
}

// Solve the firing arc with a fixed lateral speed. Vertical speed and gravity varies. 
// This enables a visually pleasing arc.
//
// proj_pos (float3): point projectile will fire from
// lateral_speed (float): scalar speed of projectile along XZ plane
// target_pos (float3): point projectile is trying to hit
// max_height (float): height above Max(proj_pos, impact_pos) for projectile to peak at
//
// fire_velocity (out float3): firing velocity
// gravity (out float): gravity necessary to projectile to hit precisely max_height
// impact_point (out float3): point where moving target will be hit
//
// return (bool): true if a valid solution was found
inline bool solve_ballistic_arc_lateral(const float3 proj_pos, const float lateral_speed, const float3 target, const float3 target_velocity, const float max_height_offset,
    float3 & fire_velocity, float & gravity, float3 & impact_point) 
{
    if (proj_pos == target || lateral_speed < 0) throw std::range_error("invalid initial conditions");

    // Ground plane terms
    float3 targetVelXZ = float3(target_velocity.x, 0.f, target_velocity.z);
    float3 diffXZ = target - proj_pos;
    diffXZ.y = 0;

    float c0 = dot(targetVelXZ, targetVelXZ) - lateral_speed*lateral_speed;
    float c1 = 2.f * dot(diffXZ, targetVelXZ);
    float c2 = dot(diffXZ, diffXZ);
    double t0, t1;
    int n = solve_quadratic(c0, c1, c2, t0, t1);

    // Pick smallest, positive time
    bool valid0 = n > 0 && t0 > 0;
    bool valid1 = n > 1 && t1 > 0;
        
    float t;
    if (!valid0 && !valid1) return false;
    else if (valid0 && valid1) t = std::min(t0, t1);
    else t = valid0 ? (float)t0 : (float)t1;

    // Calculate impact point
    impact_point = target + (target_velocity*t);

    // Calculate fire velocity along XZ plane
    float3 dir = impact_point - proj_pos;
    fire_velocity = normalize(float3(dir.x, 0.f, dir.z)) * lateral_speed;

    // Solve system of equations. Hit max_height at t=.5*time. Hit target at t=time.
    float a = proj_pos.y; // initial
    float b = std::max(proj_pos.y, impact_point.y) + max_height_offset; // peak
    float c = impact_point.y; // final

    gravity = -4*(a - 2*b + c) / (t* t);
    fire_velocity.y = -(3*a - 4*b + c) / t;

    return true;
}

#endif // end trajectory_hpp
