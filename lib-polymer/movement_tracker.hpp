#pragma once

#ifndef movement_tracker_hpp
#define movement_tracker_hpp

#include "math-common.hpp"
#include <deque>
#include <utility>
#include <memory>

template<typename T>
class movement_tracker
{

    struct sample
    {
        double when;
        T where;
    };

    // From where shall we calculate velocity? Return false on "not at all"
    bool velocity_calc_begin(size_t & out_index, double now) const
    {
       // if (timeList.size() < 2) return false; // Not enough data
       // if (duration() < min_velocity_time()) return false; // Not enough data
        double vel_time = velocity_time();
        for (size_t i=0; i<timeList.size()-1; ++i) 
        {
            if (now - timeList[i].when < vel_time) 
            {
                //if (timeList.size() - i < min_velocity_samples()) return false; // Too few samples
                out_index = i;
                return true;
            }
        }
        return false;
    }

    // The minimum number of samples for there to be any velocity calculated.
    static size_t min_velocity_samples() { return 15; }

    // Minimum time before we have a good velocity
    static double min_velocity_time() { return 0.01f; }

    // The time over which we calculate velocity.
    static double velocity_time() { return 0.5f; }

    std::unique_ptr<sample> start;
    std::deque<sample> timeList;
    const uint32_t maxHistory = 10; // Do not keep points older than this

public:

    void clear()
    {
        timeList.clear();
    }

    void add(const T & pos, double time)
    {
        if (!start) start.reset(new sample{time, pos});
        timeList.push_back(sample{time, pos});
        flush(time);
    }

    std::vector<T> points() const
    {
        std::vector<T> points;
        for (auto && p : timeList) points.push_back(p.where);
        return points;
    }

    bool empty() const { return timeList.empty(); }
    size_t size() const { return timeList.size(); }

    double start_time() const { return start->when; }
    double latest_time() const { return timeList.back().when; }

    T start_pos() const { return start->where; }
    T latest_pos() const { return timeList.back().where; }

    // Last movement delta
    T rel() const
    {
        assert(size() >= 2);
        return timeList[timeList.size() - 1].where - timeList[timeList.size() - 2].where;
    }

    double duration() const
    {
        return latest_time() - start_time();
    }

    // Calculates the average velocity over the last `vT` seconds. Return T() on fail.
    virtual T velocity(double now) const
    {
        size_t begin;

        if (!velocity_calc_begin(begin, now)) 
            return T();

        double dt = timeList.back().when - timeList[begin].when;

        if (dt <= 0) 
            return T();

        T dx = timeList.back().where - timeList[begin].where;

        return dx / (float) dt;
    }

    T velocity() const
    {
        if (timeList.size() < 2) return T();
        else return velocity(latest_time());
    }

    // Has all movement been within `max_dist` radius, during the last `duration` seconds?
    template<typename F>
    bool is_still(F max_dist, double duration) const
    {
        const double now = latest_time();

        for (size_t i = 0; i < timeList.size(); ++i) 
        {
            if (now - timeList[i].when < duration)
            {
                if (any(greater(linalg::distance(timeList[i].where, timeList.back().where), max_dist)))
                    return false;
            }
        }
        return true;
    }

    // Flush out old entries.
    void flush(double now)
    {
        while (!timeList.empty() && timeList.front().when < now - (double) maxHistory) 
        {
            timeList.pop_front();
        }
    }

};

#endif // end movement_tracker_hpp
