#pragma once

#ifndef polymer_profiling_hpp
#define polymer_profiling_hpp

#include "circular_buffer.hpp"
#include "gfx/gl/gl-async-gpu-timer.hpp"
#include "simple_timer.hpp"

// A profiler can be templated on either a `simple_cpu_timer` or `gl_gpu_timer`
// These timers are completely unrelated, but use the notion of an implicit interface
// at compile time, such that both objects implement function signatures for 
// start(), stop(), and elapsed_ms().

namespace polymer
{
    template<typename T>
    class profiler
    {
        struct data_point
        {
            ring_buffer<double> average{ 30 };
            T timer;
        };

        std::unordered_map<std::string, data_point> dataPoints;

        bool enabled{ true };

    public:

        void set_enabled(bool newState)
        {
            enabled = newState;
            dataPoints.clear();
        }

        void begin(const std::string & id)
        {
            if (!enabled) return;
            dataPoints[id].timer.start();
        }

        void end(const std::string & id)
        {
            if (!enabled) return;
            dataPoints[id].timer.stop();
            const double t = dataPoints[id].timer.elapsed_ms();
            if (t > 0.0) dataPoints[id].average.put(t);
        }

        std::vector<std::pair<std::string, float>> get_data()
        {
            std::vector<std::pair<std::string, float>> data;
            for (auto & d : dataPoints)
            {
                data.emplace_back(d.first, static_cast<float>(compute_mean(d.second.average)));
            }
            return data;
        }
    };

} // end namespace polymer

#endif // end polymer_profiling_hpp