// See COPYING file for attribution information - based on https://github.com/simongeilfus/PoissonDiskDistribution (Simon Geilfus, MIT License)

#pragma once

#ifndef poisson_disk_sampling_h
#define poisson_disk_sampling_h

#include "math-spatial.hpp"
#include "util.hpp"
#include <functional>
#include <vector>

#if defined(ANVIL_PLATFORM_WINDOWS)
#pragma warning(push)
#pragma warning(disable : 4244)
#endif

namespace poisson
{
    using namespace avl;

    class Grid 
    {
        std::vector<std::vector<float2>> grid;
        int2 numCells, offset;
        Bounds2D bounds;
        uint32_t kValue, cellSize;
    public:

        Grid(const Bounds2D & bounds, uint32_t k)
        {
            this->bounds = bounds;
            kValue = k;
            cellSize = 1 << k;
            offset = int2(abs(bounds.min()));
            numCells = int2(ceil(float2(bounds.size()) / (float) cellSize));
            grid.clear();
            grid.resize(numCells.x * numCells.y);
        }

        void add(const float2 & position)
        {
            int x = uint32_t(position.x + offset.x) >> kValue;
            int y = uint32_t(position.y + offset.y) >> kValue;
            int j = x + numCells.x * y;
            if (j < grid.size()) grid[j].push_back(position);
        }

        bool has_neighbors(const float2 & p, float radius)
        {
            float sqRadius = radius * radius;
            int2 radiusVec = int2(radius);
            int2 min = linalg::max(linalg::min(int2(p) - radiusVec, int2(bounds.max()) - int2(1)), int2(bounds.min()));
            int2 max = linalg::max(linalg::min(int2(p) + radiusVec, int2(bounds.max()) - int2(1)), int2(bounds.min()));
            int2 minCell = int2((min.x + offset.x) >> kValue, (min.y + offset.y) >> kValue);
            int2 maxCell = linalg::min(1 + int2((max.x + offset.x) >> kValue, (max.y + offset.y) >> kValue), numCells);
            
            for (size_t y = minCell.y; y < maxCell.y; y++)
            {
                for (size_t x = minCell.x; x < maxCell.x; x++) 
                {
                    for (auto cell : grid[x + numCells.x * y])
                    {
                        if (length2(p - cell) < sqRadius) return true;
                    }
                }
            }
            return false;
        }
    };
    
    class Volume
    {
        std::vector<std::vector<float3>> volume;
        int3 numCells, offset;
        Bounds3D bounds;
        uint32_t kValue, cellSize;
    public:
        
        Volume(const Bounds3D & bounds, uint32_t k)
        {
            this->bounds = bounds;
            kValue = k;
            cellSize = 1 << k;
            offset = int3(abs(bounds.min()));
            numCells = int3(ceil(float3(bounds.size()) / (float) cellSize));
            volume.clear();
            volume.resize(numCells.x * numCells.y * numCells.z);
        }
        
        void add(const float3 & position)
        {
            int x = uint32_t(position.x + offset.x) >> kValue;
            int y = uint32_t(position.y + offset.y) >> kValue;
            int z = uint32_t(position.z + offset.z) >> kValue;
            int j = z * numCells.x * numCells.y + y * numCells.x + x;
            if (j < volume.size()) volume[j].push_back(position);
        }
        
        bool has_neighbors(const float3 & p, float radius)
        {
            float sqRadius = radius * radius;
            int3 radiusVec = int3(radius);
            int3 min = linalg::max(linalg::min(int3(p) - radiusVec, int3(bounds.max()) - int3(1)), int3(bounds.min()));
            int3 max = linalg::max(linalg::min(int3(p) + radiusVec, int3(bounds.max()) - int3(1)), int3(bounds.min()));
            int3 minCell = int3((min.x + offset.x) >> kValue, (min.y + offset.y) >> kValue, (min.z + offset.z) >> kValue);
            int3 maxCell = linalg::min(1 + int3((max.x + offset.x) >> kValue, (max.y + offset.y) >> kValue, (max.z + offset.z) >> kValue), numCells);
            
             for (size_t z = minCell.z; z < maxCell.z; z++)
             {
                for (size_t y = minCell.y; y < maxCell.y; y++)
                {
                    for (size_t x = minCell.x; x < maxCell.x; x++)
                    {
                        for (auto cell : volume[z * numCells.x * numCells.y + y * numCells.x + x])
                        {
                            if (length2(p - cell) < sqRadius) return true;
                        }
                    }
                }
            }
            return false;
        }
    };
    
    struct PoissonDiskGenerator2D
    {
        std::function<float(const float2 &)> distFunction;
        std::function<bool(const float2 &)> boundsFunction;
        
        std::vector<float2> build(const Bounds2D & bounds, const std::vector<float2> & initialSet, int k, float separation = 1.0)
        {
            std::vector<float2> processingList;
            std::vector<float2> outputList;
            Grid grid(bounds, 3);
            UniformRandomGenerator r;
            
            // add the initial points
            for (auto p : initialSet)
            {
                processingList.push_back(p);
                outputList.push_back(p);
                grid.add(p);
            }
            
            // if there's no initial points add the center point
            if (!processingList.size())
            {
                processingList.push_back(bounds.center());
                outputList.push_back(bounds.center());
                grid.add(bounds.center());
            }
            
            // while there's points in the processing list
            while (processingList.size())
            {
                // pick a random point in the processing list
                int randPoint = r.random_int(int(processingList.size()) - 1);
                float2 center = processingList[randPoint];
                
                // remove it
                processingList.erase(processingList.begin() + randPoint);
                
                if (distFunction)
                    separation = distFunction(center);
                
                // spawn k points in an anulus around that point
                // the higher k is, the higher the packing will be and slower the algorithm
                for (int i = 0; i < k; i++)
                {
                    float randRadius = separation * (1.0f + r.random_float());
                    float randAngle = r.random_float() * ANVIL_PI * 2.0f;
                    float2 newPoint = center + float2(cos(randAngle), sin(randAngle)) * randRadius;
                    
                    // check if the new random point is in the window bounds
                    // and if it has no neighbors that are too close to them
                    if (bounds.contains(newPoint) && !grid.has_neighbors(newPoint, separation))
                    {
                        if (boundsFunction && boundsFunction(newPoint)) continue;
                        
                        // if the point has no close neighbors add it to the processing list, output list and grid
                        processingList.push_back(newPoint);
                        outputList.push_back(newPoint);
                        grid.add(newPoint);
                    }
                }
            }
            return outputList;
        }
    };
    
    struct PoissonDiskGenerator3D
    {
        std::function<float(const float3)> distFunction;
        std::function<bool(const float3)> boundsFunction;
        UniformRandomGenerator r;

        std::vector<float3> build(const Bounds3D & bounds, const std::vector<float3> & initialSet, int k, float separation = 1.0)
        {
            std::vector<float3> processingList;
            std::vector<float3> outputList;
            Volume grid(bounds, 3);

            
            for (auto p : initialSet)
            {
                processingList.push_back(p);
                outputList.push_back(p);
                grid.add(p);
            }
            
            if (!processingList.size())
            {
                processingList.push_back(bounds.center());
                outputList.push_back(bounds.center());
                grid.add(bounds.center());
            }
            
            while (processingList.size())
            {
                const uint32_t randPoint = r.random_int(int(processingList.size()) - 1);
                const float3 center = processingList[randPoint];
                
                processingList.erase(processingList.begin() + randPoint);
                
                if (distFunction)
                {
                    separation = distFunction(center);
                }

                for (int i = 0; i < k; i++)
                {
                    float randRadius = separation * (1.0f + r.random_float());
                    float angle1 = r.random_float() * ANVIL_PI * 2.0f;
                    float angle2 = r.random_float() * ANVIL_PI * 2.0f;
                    
                    float newX = center.x + randRadius * cos(angle1) * sin(angle2);
                    float newY = center.y + randRadius * sin(angle1) * sin(angle2);
                    float newZ = center.z + randRadius * cos(angle2);
                    float3 newPoint = {newX, newY, newZ};
                    
                    if (bounds.contains(newPoint) && !grid.has_neighbors(newPoint, separation))
                    {
                        if (boundsFunction && boundsFunction(newPoint)) continue;
                        processingList.push_back(newPoint);
                        outputList.push_back(newPoint);
                        grid.add(newPoint);
                    }
                }
            }
            return outputList;
        }
    };

    // Returns a set of poisson disk samples inside a rectangular area, with a minimum separation and with
    // a packing determined by how high k is. The higher k is the higher the algorithm will be slow.
    // If no initialSet of points is provided the area center will be used as the initial point.
    inline std::vector<float2> make_poisson_disk_distribution(const Bounds2D & bounds, const std::vector<float2> & initialSet, int k, float separation = 1.0)
    {
        poisson::PoissonDiskGenerator2D gen;
        return gen.build(bounds, initialSet, k, separation);
    }

    inline std::vector<float3> make_poisson_disk_distribution(const Bounds3D & bounds, const std::vector<float3> & initialSet, int k, float separation = 1.0)
    {
        poisson::PoissonDiskGenerator3D gen;
        return gen.build(bounds, initialSet, k, separation);
    } 
}

#pragma warning(pop)

#endif
