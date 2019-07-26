#pragma once

#ifndef polymer_misc_algorithms_hpp
#define polymer_misc_algorithms_hpp

#include "util.hpp"
#include "math-core.hpp"
#include "math-primitives.hpp"

#include <functional>
#include <memory>
#include <algorithm>

#undef min
#undef max

namespace polymer
{
    template<typename T>
    class voxel_array
    {
        int3 size;
        std::vector<T> voxels;
    public:
        voxel_array(const int3 & size) : size(size), voxels(size.x * size.y * size.z) {}
        const int3 & get_size() const { return size; }
        T operator[](const int3 & coords) const { return voxels[coords.z * size.x * size.y + coords.y * size.x + coords.x]; }
        T & operator[](const int3 & coords) { return voxels[coords.z * size.x * size.y + coords.y * size.x + coords.x]; }
    };

    // Despite the Gielis formulation of this (which produces interesting biologically-inspired shapes),
    // it is patented: https://patents.justia.com/patent/9317627
    class super_formula
    {
        float m, n1, n2, n3, a, b;
    public:
        super_formula(const float m, const float n1, const float n2, const float n3, const float a = 1.0f, const float b = 1.0f) : m(m), n1(n1), n2(n2), n3(n3), a(a), b(b) {}

        float operator() (const float phi) const
        {
            float raux1 = std::pow(std::abs(1.f / a * std::cos(m * phi / 4.f)), n2) + std::pow(std::abs(1.f / b * std::sin(m * phi / 4.f)), n3);
            float r1 = std::pow(std::abs(raux1), -1.0f / n1);
            return r1;
        }
    };

    // Cantor set on the xz plane
    struct cantor_set
    {
        std::vector<line> lines{ {float3(-1, 0, 0), float3(1, 0, 0)} }; // initial

        std::vector<line> next(const line & l) const
        {
            const float3 p0 = l.origin;
            const float3 pn = l.direction;
            float3 p1 = (pn - p0) / 3.0f + p0;
            float3 p2 = ((pn - p0) * 2.f) / 3.f + p0;

            std::vector<line> next;
            next.push_back({ p0, p1 });
            next.push_back({ p2, pn });

            return next;
        }

        void step()
        {
            std::vector<line> recomputedSet;
            for (auto & l : lines)
            {
                std::vector<line> nextLines = next(l); // split
                std::copy(nextLines.begin(), nextLines.end(), std::back_inserter(recomputedSet));
            }
            recomputedSet.swap(lines);
        }
    };

    struct simple_harmonic_oscillator
    {
        float frequency = 0, amplitude = 0, phase = 0;
        float value() const { return std::sin(phase) * amplitude; }
        void update(float timestep) { phase += frequency * timestep; }
    };

    inline std::vector<bool> make_euclidean_pattern(int steps, int pulses)
    {
        std::vector<bool> pattern;

        std::function<void(int, int, std::vector<bool> &, std::vector<int> &, std::vector<int> &)> bjorklund;

        bjorklund = [&bjorklund](int level, int r, std::vector<bool> & pattern, std::vector<int> & counts, std::vector<int> & remainders)
        {
            r++;
            if (level > -1)
            {
                for (int i = 0; i < counts[level]; ++i) bjorklund(level - 1, r, pattern, counts, remainders);
                if (remainders[level] != 0) bjorklund(level - 2, r, pattern, counts, remainders);
            }
            else if (level == -1) pattern.push_back(false);
            else if (level == -2) pattern.push_back(true);
        };

        if (pulses > steps || pulses == 0 || steps == 0) return pattern;

        std::vector<int> counts;
        std::vector<int> remainders;

        int divisor = steps - pulses;
        remainders.push_back(pulses);
        int level = 0;

        while (true)
        {
            counts.push_back(divisor / remainders[level]);
            remainders.push_back(divisor % remainders[level]);
            divisor = remainders[level];
            level++;
            if (remainders[level] <= 1) break;
        }

        counts.push_back(divisor);

        bjorklund(level, 0, pattern, counts, remainders);

        return pattern;
    }

    struct universal_layout_container
    {
        struct ucoord
        {
            float a, b;
            float resolve(float min, float max) const { return min + a * (max - min) + b; }
        };

        struct urect
        {
            ucoord x0, y0, x1, y1;
            aabb_2d resolve(const aabb_2d & r) const { return{ x0.resolve(r.min().x, r.max().x), y0.resolve(r.min().y, r.max().y), x1.resolve(r.min().x, r.max().x), y1.resolve(r.min().y, r.max().y) }; }
            bool is_fixed_width() const { return x0.a == x1.a; }
            bool is_fixed_height() const { return y0.a == y1.a; }
            float fixed_width() const { return x1.b - x0.b; }
            float fixed_height() const { return y1.b - y0.b; }
        };

        float aspectRatio{ 1 };
        urect placement{ { 0,0 },{ 0,0 },{ 1,0 },{ 1,0 } };
        aabb_2d bounds;

        std::vector<std::shared_ptr<universal_layout_container>> children;

        void add_child(universal_layout_container::urect placement, std::shared_ptr<universal_layout_container> child = std::make_shared<universal_layout_container>())
        {
            child->placement = placement;
            children.push_back(child);
        }

        void recompute()
        {
            for (auto & child : children)
            {
                auto size = child->bounds.size();
                child->bounds = child->placement.resolve(bounds);
                auto childAspect = child->bounds.width() / child->bounds.height();
                if (childAspect > 0)
                {
                    float xpadding = (1 - std::min((child->bounds.height() * childAspect) / child->bounds.width(), 1.0f)) / 2;
                    float ypadding = (1 - std::min((child->bounds.width() / childAspect) / child->bounds.height(), 1.0f)) / 2;
                    child->bounds = urect{ { xpadding, 0 },{ ypadding, 0 },{ 1 - xpadding, 0 },{ 1 - ypadding, 0 } }.resolve(child->bounds);
                }
                if (child->bounds.size() != size) child->recompute();
            }
        }
    };

} // end namespace polymer

#endif // end polymer_misc_algorithms_hpp
