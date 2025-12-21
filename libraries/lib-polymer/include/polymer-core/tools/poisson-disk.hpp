// Based on 'Fast Poisson Disk Sampling in Arbitrary Dimensions' by Robert Bridson
// Adapted from https://github.com/thinks/poisson-disk-sampling (MIT License)

#pragma once

#ifndef polymer_poisson_disk_sampling_hpp
#define polymer_poisson_disk_sampling_hpp

#include "polymer-core/math/math-core.hpp"
#include "polymer-core/math/math-primitives.hpp"
#include "polymer-core/util/util.hpp"

#include <functional>
#include <random>
#include <stack>
#include <vector>

#if defined(POLYMER_PLATFORM_WINDOWS)
#pragma warning(push)
#pragma warning(disable : 4244)
#endif

namespace poisson
{
    using namespace polymer;

    // Bridson's algorithm configuration for 2D
    struct config_2d
    {
        float2 min       = {0, 0};
        float2 max       = {1, 1};
        float min_distance = 1.0f;
        int max_attempts   = 30;
        float2 start       = {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()};
    };

    // Bridson's algorithm configuration for 3D
    struct config_3d
    {
        float3 min       = {0, 0, 0};
        float3 max       = {1, 1, 1};
        float min_distance = 1.0f;
        int max_attempts   = 30;
        float3 start       = {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()};
    };

    namespace detail
    {
        constexpr float infinity = std::numeric_limits<float>::infinity();

        // Grid index computation for 2D
        inline int grid_index_2d(const float2 & p, float cell_size, int grid_width)
        {
            int x = static_cast<int>(std::floor(p.x / cell_size));
            int y = static_cast<int>(std::floor(p.y / cell_size));
            return y * grid_width + x;
        }

        // Grid index computation for 3D
        inline int grid_index_3d(const float3 & p, float cell_size, int grid_width, int grid_height)
        {
            int x = static_cast<int>(std::floor(p.x / cell_size));
            int y = static_cast<int>(std::floor(p.y / cell_size));
            int z = static_cast<int>(std::floor(p.z / cell_size));
            return z * grid_width * grid_height + y * grid_width + x;
        }
    }

    // 2D Poisson disk sampler using Bridson's algorithm
    struct poisson_sampler_2d
    {
        std::function<bool(float2)> in_bounds_callback;

        // boundsFunction returns true if a point should be REJECTED (is outside valid region)
        // This is the inverse of in_bounds_callback which returns true if point is VALID
        std::function<bool(float2)> boundsFunction;

        std::vector<float2> build(const aabb_2d & bounds, const std::vector<float2> & initial_set, int k, float separation)
        {
            using namespace detail;

            std::vector<float2> result;

            // Random number generator
            std::random_device rd;
            std::mt19937 gen(rd());

            float2 bounds_min = bounds.min();
            float2 bounds_max = bounds.max();
            float width  = bounds_max.x - bounds_min.x;
            float height = bounds_max.y - bounds_min.y;

            if (width <= 0 || height <= 0 || separation <= 0)
            {
                return result;
            }

            // Cell size for 2D is min_distance / sqrt(2)
            float cell_size = separation / std::sqrt(2.0f);
            int grid_width  = static_cast<int>(std::ceil(width / cell_size));
            int grid_height = static_cast<int>(std::ceil(height / cell_size));

            // Grid stores indices into result vector, -1 means empty
            std::vector<int> grid(grid_width * grid_height, -1);
            std::stack<int> active_list;

            float min_dist_squared = separation * separation;

            // Lambda to convert world coords to grid-relative coords
            auto to_local = [&](const float2 & p) -> float2 {
                return {p.x - bounds_min.x, p.y - bounds_min.y};
            };

            // Lambda to check if point is in bounds
            auto in_bounds = [&](const float2 & p) -> bool {
                if (p.x < bounds_min.x || p.x >= bounds_max.x ||
                    p.y < bounds_min.y || p.y >= bounds_max.y)
                {
                    return false;
                }
                // boundsFunction returns true if point should be REJECTED
                if (boundsFunction && boundsFunction(p))
                {
                    return false;
                }
                // in_bounds_callback returns true if point is VALID
                if (in_bounds_callback)
                {
                    return in_bounds_callback(p);
                }
                return true;
            };

            // Lambda to check if a point is too close to existing points
            auto is_valid = [&](const float2 & p) -> bool {
                float2 local = to_local(p);
                int x_idx = static_cast<int>(std::floor(local.x / cell_size));
                int y_idx = static_cast<int>(std::floor(local.y / cell_size));

                // Check the cell itself
                int cell_idx = y_idx * grid_width + x_idx;
                if (cell_idx < 0 || cell_idx >= static_cast<int>(grid.size()))
                {
                    return false;
                }
                if (grid[cell_idx] >= 0)
                {
                    return false; // Cell already occupied
                }

                // Check neighboring cells (2-cell radius for safety)
                int min_x = std::max(x_idx - 2, 0);
                int min_y = std::max(y_idx - 2, 0);
                int max_x = std::min(x_idx + 2, grid_width - 1);
                int max_y = std::min(y_idx + 2, grid_height - 1);

                for (int y = min_y; y <= max_y; ++y)
                {
                    for (int x = min_x; x <= max_x; ++x)
                    {
                        int idx = grid[y * grid_width + x];
                        if (idx >= 0)
                        {
                            float2 neighbor = result[idx];
                            float dx        = p.x - neighbor.x;
                            float dy        = p.y - neighbor.y;
                            if (dx * dx + dy * dy < min_dist_squared)
                            {
                                return false;
                            }
                        }
                    }
                }
                return true;
            };

            // Lambda to add a point
            auto add_point = [&](const float2 & p) {
                int point_idx = static_cast<int>(result.size());
                result.push_back(p);
                active_list.push(point_idx);

                float2 local  = to_local(p);
                int grid_x    = static_cast<int>(std::floor(local.x / cell_size));
                int grid_y    = static_cast<int>(std::floor(local.y / cell_size));
                int grid_idx  = grid_y * grid_width + grid_x;
                if (grid_idx >= 0 && grid_idx < static_cast<int>(grid.size()))
                {
                    grid[grid_idx] = point_idx;
                }
            };

            // Lambda to generate random point in annulus around p
            auto point_around = [&](const float2 & p) -> float2 {
                std::uniform_real_distribution<float> dist_r(0.0f, 3.0f);
                std::uniform_real_distribution<float> dist_angle(0.0f, static_cast<float>(POLYMER_TAU));

                float radius = separation * std::sqrt(dist_r(gen) + 1.0f); // Range [r, 2r]
                float angle  = dist_angle(gen);

                return {p.x + std::cos(angle) * radius, p.y + std::sin(angle) * radius};
            };

            // Add initial points or generate a random starting point
            if (!initial_set.empty())
            {
                for (const auto & p : initial_set)
                {
                    if (in_bounds(p) && is_valid(p))
                    {
                        add_point(p);
                    }
                }
            }

            // If no valid initial points, pick a random starting point
            if (result.empty())
            {
                std::uniform_real_distribution<float> dist_x(bounds_min.x, bounds_max.x);
                std::uniform_real_distribution<float> dist_y(bounds_min.y, bounds_max.y);

                float2 start;
                int attempts = 0;
                do
                {
                    start = {dist_x(gen), dist_y(gen)};
                    ++attempts;
                } while (!in_bounds(start) && attempts < 1000);

                if (in_bounds(start))
                {
                    add_point(start);
                }
            }

            // Main Bridson algorithm loop
            while (!active_list.empty())
            {
                int current_idx = active_list.top();
                active_list.pop();
                float2 current = result[current_idx];

                for (int i = 0; i < k; ++i)
                {
                    float2 candidate = point_around(current);
                    if (in_bounds(candidate) && is_valid(candidate))
                    {
                        add_point(candidate);
                    }
                }
            }

            return result;
        }
    };

    // 3D Poisson sphere sampler using Bridson's algorithm
    struct poisson_sampler_3d
    {
        std::function<bool(float3)> in_bounds_callback;

        std::vector<float3> build(const aabb_3d & bounds, const std::vector<float3> & initial_set, int k, float separation)
        {
            using namespace detail;

            std::vector<float3> result;

            // Random number generator
            std::random_device rd;
            std::mt19937 gen(rd());

            float3 bounds_min = bounds.min();
            float3 bounds_max = bounds.max();
            float width  = bounds_max.x - bounds_min.x;
            float height = bounds_max.y - bounds_min.y;
            float depth  = bounds_max.z - bounds_min.z;

            if (width <= 0 || height <= 0 || depth <= 0 || separation <= 0)
            {
                return result;
            }

            // Cell size for 3D is min_distance / sqrt(3)
            float cell_size  = separation / std::sqrt(3.0f);
            int grid_width   = static_cast<int>(std::ceil(width / cell_size));
            int grid_height  = static_cast<int>(std::ceil(height / cell_size));
            int grid_depth   = static_cast<int>(std::ceil(depth / cell_size));

            // Grid stores indices into result vector, -1 means empty
            std::vector<int> grid(grid_width * grid_height * grid_depth, -1);
            std::stack<int> active_list;

            float min_dist_squared = separation * separation;

            // Lambda to convert world coords to grid-relative coords
            auto to_local = [&](const float3 & p) -> float3 {
                return {p.x - bounds_min.x, p.y - bounds_min.y, p.z - bounds_min.z};
            };

            // Lambda to check if point is in bounds
            auto in_bounds = [&](const float3 & p) -> bool {
                if (p.x < bounds_min.x || p.x >= bounds_max.x ||
                    p.y < bounds_min.y || p.y >= bounds_max.y ||
                    p.z < bounds_min.z || p.z >= bounds_max.z)
                {
                    return false;
                }
                if (in_bounds_callback)
                {
                    return in_bounds_callback(p);
                }
                return true;
            };

            // Lambda to check if a point is too close to existing points
            auto is_valid = [&](const float3 & p) -> bool {
                float3 local = to_local(p);
                int x_idx = static_cast<int>(std::floor(local.x / cell_size));
                int y_idx = static_cast<int>(std::floor(local.y / cell_size));
                int z_idx = static_cast<int>(std::floor(local.z / cell_size));

                // Check the cell itself
                int cell_idx = z_idx * grid_width * grid_height + y_idx * grid_width + x_idx;
                if (cell_idx < 0 || cell_idx >= static_cast<int>(grid.size()))
                {
                    return false;
                }
                if (grid[cell_idx] >= 0)
                {
                    return false; // Cell already occupied
                }

                // Check neighboring cells (2-cell radius for safety)
                int min_x = std::max(x_idx - 2, 0);
                int min_y = std::max(y_idx - 2, 0);
                int min_z = std::max(z_idx - 2, 0);
                int max_x = std::min(x_idx + 2, grid_width - 1);
                int max_y = std::min(y_idx + 2, grid_height - 1);
                int max_z = std::min(z_idx + 2, grid_depth - 1);

                for (int z = min_z; z <= max_z; ++z)
                {
                    for (int y = min_y; y <= max_y; ++y)
                    {
                        for (int x = min_x; x <= max_x; ++x)
                        {
                            int idx = grid[z * grid_width * grid_height + y * grid_width + x];
                            if (idx >= 0)
                            {
                                float3 neighbor = result[idx];
                                float dx        = p.x - neighbor.x;
                                float dy        = p.y - neighbor.y;
                                float dz        = p.z - neighbor.z;
                                if (dx * dx + dy * dy + dz * dz < min_dist_squared)
                                {
                                    return false;
                                }
                            }
                        }
                    }
                }
                return true;
            };

            // Lambda to add a point
            auto add_point = [&](const float3 & p) {
                int point_idx = static_cast<int>(result.size());
                result.push_back(p);
                active_list.push(point_idx);

                float3 local  = to_local(p);
                int grid_x    = static_cast<int>(std::floor(local.x / cell_size));
                int grid_y    = static_cast<int>(std::floor(local.y / cell_size));
                int grid_z    = static_cast<int>(std::floor(local.z / cell_size));
                int grid_idx  = grid_z * grid_width * grid_height + grid_y * grid_width + grid_x;
                if (grid_idx >= 0 && grid_idx < static_cast<int>(grid.size()))
                {
                    grid[grid_idx] = point_idx;
                }
            };

            // Lambda to generate random point in spherical shell around p (radius [r, 2r])
            auto point_around = [&](const float3 & p) -> float3 {
                std::uniform_real_distribution<float> dist_r(0.0f, 3.0f);
                std::uniform_real_distribution<float> dist_theta(0.0f, static_cast<float>(POLYMER_TAU));
                std::uniform_real_distribution<float> dist_cos_phi(-1.0f, 1.0f);

                float radius  = separation * std::sqrt(dist_r(gen) + 1.0f);  // Range [r, 2r]
                float theta   = dist_theta(gen);                             // Azimuthal angle
                float cos_phi = dist_cos_phi(gen);                           // Uniform on [-1, 1]
                float sin_phi = std::sqrt(1.0f - cos_phi * cos_phi);

                return {
                    p.x + radius * sin_phi * std::cos(theta),
                    p.y + radius * sin_phi * std::sin(theta),
                    p.z + radius * cos_phi
                };
            };

            // Add initial points or generate a random starting point
            if (!initial_set.empty())
            {
                for (const auto & p : initial_set)
                {
                    if (in_bounds(p) && is_valid(p))
                    {
                        add_point(p);
                    }
                }
            }

            // If no valid initial points, pick a random starting point
            if (result.empty())
            {
                std::uniform_real_distribution<float> dist_x(bounds_min.x, bounds_max.x);
                std::uniform_real_distribution<float> dist_y(bounds_min.y, bounds_max.y);
                std::uniform_real_distribution<float> dist_z(bounds_min.z, bounds_max.z);

                float3 start;
                int attempts = 0;
                do
                {
                    start = {dist_x(gen), dist_y(gen), dist_z(gen)};
                    ++attempts;
                } while (!in_bounds(start) && attempts < 1000);

                if (in_bounds(start))
                {
                    add_point(start);
                }
            }

            // Main Bridson algorithm loop
            while (!active_list.empty())
            {
                int current_idx = active_list.top();
                active_list.pop();
                float3 current = result[current_idx];

                for (int i = 0; i < k; ++i)
                {
                    float3 candidate = point_around(current);
                    if (in_bounds(candidate) && is_valid(candidate))
                    {
                        add_point(candidate);
                    }
                }
            }

            return result;
        }
    };

    // Returns a set of poisson disk samples inside a rectangular area, with a minimum separation and with
    // a packing determined by how high k is. The higher k is the higher the algorithm will be slow.
    // If no initialSet of points is provided a random starting point will be chosen.
    inline std::vector<float2> make_poisson_disc_distribution(const aabb_2d & bounds, const std::vector<float2> & initialSet, int k, float separation = 1.0f)
    {
        poisson::poisson_sampler_2d sampler;
        return sampler.build(bounds, initialSet, k, separation);
    }

    inline std::vector<float3> make_poisson_sphere_distribution(const aabb_3d & bounds, const std::vector<float3> & initialSet, int k, float separation = 1.0f)
    {
        poisson::poisson_sampler_3d sampler;
        return sampler.build(bounds, initialSet, k, separation);
    }
}

#if defined(POLYMER_PLATFORM_WINDOWS)
#pragma warning(pop)
#endif

#endif
