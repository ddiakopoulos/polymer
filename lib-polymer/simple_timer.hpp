#ifndef simple_timer_hpp
#define simple_timer_hpp

#include <iostream>
#include <chrono>


class SimpleTimer 
{
    typedef std::chrono::high_resolution_clock::time_point timepoint;
    typedef std::chrono::high_resolution_clock::duration timeduration;

    bool isRunning;
    timepoint startTime;
    timepoint pauseTime;

    inline timepoint current_time_point() const  { return std::chrono::high_resolution_clock::now(); }
    inline timeduration running_time() const { return (isRunning) ? current_time_point() - startTime : pauseTime - startTime; }

    template<typename unit>
    inline unit running_time() const
    {
        return std::chrono::duration_cast<unit>(running_time());
    }

public:

    SimpleTimer(bool run = false) : isRunning(run)
    {
        if (run) start();
    }

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

    void pause() 
    {
        pauseTime = current_time_point();
        isRunning = false;
    }

    void unpause() 
    {
        if (isRunning) return;
        startTime += current_time_point() - pauseTime;
        isRunning = true;
    }

    std::chrono::nanoseconds nanoseconds() const { return running_time<std::chrono::nanoseconds>(); }
    std::chrono::microseconds microseconds() const { return running_time<std::chrono::microseconds>(); }
    std::chrono::milliseconds milliseconds() const { return running_time<std::chrono::milliseconds>(); }
    std::chrono::seconds seconds() const { return running_time<std::chrono::seconds>(); }

    double elapsed_ms() const { return (std::chrono::duration<float>(pauseTime - startTime).count() * 1000.0); }

    bool is_running() { return isRunning; }
};

#endif // end simple_timer_hpp
