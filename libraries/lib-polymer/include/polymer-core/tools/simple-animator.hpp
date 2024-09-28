#pragma once

#ifndef polymer_simple_animator_hpp
#define polymer_simple_animator_hpp

#include "polymer-core/util/util.hpp"
#include "polymer-core/math/math-common.hpp"

#include <list>
#include <functional>
#include <thread>

// https://github.com/LiveMirror/cpptweener/blob/fee53371b05e94e24f3df40335023205f8d0abd9/src/CppTweener.cpp

namespace tween
{
    struct linear
    {
        inline static double ease_in_out(const double t) { return t; }
    };

    struct sine
    {
        inline static double ease_in_out(double t)
        {
            const double b = 0.0;
            const double c = 1.0;
            const double d = 1.0;
            return -0.5 * (std::cos(POLYMER_PI * t) - 1.0);
        }

        inline static double ease_in(double t)
        {
            const double b = 0.0;
            const double c = 1.0;
            const double d = 1.0;
            return -c * std::cos(t / d * (POLYMER_HALF_PI)) + c + b;
        }

        inline static double ease_out(double t)
        {
            const double b = 0.0;
            const double c = 1.0;
            const double d = 1.0;
            return c * std::sin(t / d * (POLYMER_HALF_PI)) + b;
        }
    };

    struct smoothstep
    {
        inline static double ease_in_out(double t)
        {
            double scale = t * t * (3 - 2 * t);
            return scale * 1.0;
        }
    };

    struct circular
    {
        inline static double ease_in_out(double t)
        {
            t *= 2;
            if (t < 1) return -0.5 * (sqrt(1 - t * t) - 1);
            else
            {
                t -= 2;
                return 0.5 * (sqrt(1 - t * t) + 1);
            }
        }
    };

    struct exp
    {
        inline static double ease_in_out(double t)
        {
            if (t == 0.0) return 0.0;
            if (t == 1.0) return 1.0;
            t *= 2;
            if (t < 1) return 0.5 * std::pow(2, 10 * (t - 1));
            return 0.5 * (-std::pow(2, -10 * (t - 1)) + 2);
        }

        inline static double ease_in(double t)
        {
            const double b = 0.0;
            const double c = 1.0;
            const double d = 1.0;
            return (t == 0) ? b : c * pow(2, 10 * (t / d - 1)) + b;
        }

        inline static double ease_out(double t)
        {
            const double b = 0.0;
            const double c = 1.0;
            const double d = 1.0;
            return (t == d) ? b + c : c * (-pow(2, -10 * t / d) + 1) + b;
        }
    };

    struct cubic
    {
        inline static double ease_in_out(double t)
        {
            t *= 2;
            if (t < 1) return 0.5 * t*t*t;
            t -= 2;
            return 0.5*(t*t*t + 2);
        }

        inline static double ease_in(double t)
        {
            const double b = 0.0;
            const double c = 1.0;
            const double d = 1.0;
            return c * (t /= d) * t * t + b;
        }

        inline static double ease_out(double t)
        {
            const double b = 0.0;
            const double c = 1.0;
            const double d = 1.0;
            return c * ((t = t / d - 1) * t * t + 1) + b;
        }
    };

    struct quartic
    {
        inline static double ease_in_out(double t)
        {
            t *= 2;
            if (t < 1) return 0.5*t*t*t*t;
            else
            {
                t -= 2;
                return -0.5 * (t*t*t*t - 2.0);
            }
        }
    };

}

namespace polymer
{
    
    enum class playback_state : uint32_t
    {
        none    = 0x1,
        loop    = 0x2,
        forward = 0x4,
        reverse = 0x8,
    };

    playback_state operator | (playback_state a, playback_state b)
    {
        return static_cast<playback_state>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
    }

    playback_state operator & (playback_state a, playback_state b)
    {
        return static_cast<playback_state>(static_cast<unsigned int>(a) & static_cast<unsigned int>(b));
    }

    playback_state operator~(playback_state a)
    {
        return static_cast<playback_state>(~static_cast<unsigned int>(a));
    }

    inline bool is_set(playback_state flags)
    {
        return static_cast<unsigned int>(flags) != 0;
    }

    class tween_event
    {
        void * variable;
        double t0, t1;
        std::function<void(double t)> forward_update_impl;
        std::function<void(double t)> reverse_update_impl;
        friend class simple_animator;
        double duration_seconds;
    public:
        tween_event(const std::string & name, void * v, double t0, double t1, double duration, std::function<void(double t)> fwd, std::function<void(double t)> rvs)
            : name(name), variable(v), t0(t0), t1(t1), duration_seconds(duration), forward_update_impl(fwd), reverse_update_impl(rvs) {}
        std::function<void()> on_finish;
        std::function<void(double t)> on_update;
        playback_state state {playback_state::forward};
        std::string name;
    };

    // A simple playback manager for basic animation curves. 
    // @todo - threading, on_start callback, trigger delay, polymer::property support
    class simple_animator
    {
        std::list<tween_event> tweens;
        double now_seconds = 0.0f;

    public:

        void update(const double dt)
        {
            //std::cout << "Tweens size: " << tweens.size() << std::endl;

            now_seconds += dt;

            for (auto it = std::begin(tweens); it != std::end(tweens);)
            {
                if (now_seconds < it->t1) // now less than the end
                {
                    // don't need to update quite yet; delay in effect
                    if (now_seconds < it->t0) 
                    {
                        ++it;
                        continue;
                    }

                    const double dx = static_cast<double>((now_seconds - it->t0) / (it->t1 - it->t0));
                    if (it->on_update) it->on_update(dx);

                    if (is_set(it->state & playback_state::forward)) 
                    {
                        //std::cout << "forward" << std::endl;
                        it->forward_update_impl(dx);
                    }
                    if (is_set(it->state & playback_state::reverse))  
                    {
                        //std::cout << "reverse" << std::endl;
                        it->reverse_update_impl(dx);
                    }

                    ++it;
                }
                else
                {
                    if (is_set(it->state & playback_state::loop))
                    {
                        it->t0 = now_seconds;
                        it->t1 = now_seconds + it->duration_seconds;

                        // temp hack... needs an on_loop variant
                        if (it->on_finish) it->on_finish();

                        if (is_set(it->state & playback_state::forward))
                        {
                            it->state = static_cast<playback_state>(it->state & ~playback_state::forward); 
                            it->state = static_cast<playback_state>(it->state | playback_state::reverse);
                        }
                        else
                        {
                            it->state = static_cast<playback_state>(it->state & ~playback_state::reverse); 
                            it->state = static_cast<playback_state>(it->state | playback_state::forward);
                        }
                    }
                    else
                    {   
                        if (is_set(it->state & playback_state::forward)) it->forward_update_impl(1.0);
                        else it->reverse_update_impl(1.0);
                        if (it->on_update) it->on_update(1.f);
                        if (it->on_finish) it->on_finish();
                        it->state = playback_state::none;
                        it = tweens.erase(it);
                    }
                }
            }
        }

        void cancel_all()
        {
            tweens.clear();
        }

        template<class VariableType, class EasingFunc>
        tween_event & add_tween(const std::string & name, VariableType * variable, VariableType targetValue, double duration_seconds, EasingFunc ease, double delay_sec = 0.f)
        {   
            VariableType initialValue = *variable;

            auto forward_update = [=](double t)
            {
                *variable = static_cast<VariableType>(initialValue * (1.f - ease(t)) + targetValue * ease(t));
            };

            auto reverse_update = [=](double t)
            {
                *variable = static_cast<VariableType>(targetValue * (1.f - ease(t)) + initialValue * ease(t));
            };

            tweens.emplace_back(name, variable, delay_sec + now_seconds, delay_sec + now_seconds + duration_seconds, duration_seconds, forward_update, reverse_update);
            return tweens.back();
        }

    };

}

#endif // end polymer_simple_animator_hpp
