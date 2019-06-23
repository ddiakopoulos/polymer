#pragma once

#ifndef polymer_simple_timer_hpp
#define polymer_simple_timer_hpp

#include <iostream>
#include <chrono>

namespace polymer
{
    class simple_cpu_timer
    {
        typedef std::chrono::high_resolution_clock::time_point timepoint;
        typedef std::chrono::high_resolution_clock::duration timeduration;

        bool isRunning;
        timepoint startTime, pauseTime;

        inline timepoint current_time_point() const { return std::chrono::high_resolution_clock::now(); }
        inline timeduration running_time() const { return (isRunning) ? current_time_point() - startTime : timeduration::zero(); }
        template<typename unit> inline unit running_time() const { return std::chrono::duration_cast<unit>(running_time()); }

    public:

        void start()
        {
            reset();
            isRunning = true;
        }

        void stop()
        {
            pauseTime = current_time_point();
            isRunning = false;
        }

        void reset()
        {
            startTime = std::chrono::high_resolution_clock::now();
            pauseTime = startTime;
        }

        double elapsed_ms() const
        {
            return (std::chrono::duration<float>(pauseTime - startTime).count() * 1000.0);
        }

        std::chrono::milliseconds milliseconds() const
        {
            return running_time<std::chrono::milliseconds>();
        }

        bool is_running()
        {
            return isRunning;
        }
    };

} // end namespace polymer

#endif // end polymer_simple_timer_hpp
