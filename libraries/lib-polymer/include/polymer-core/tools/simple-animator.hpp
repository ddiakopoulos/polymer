#pragma once

#ifndef polymer_simple_animator_hpp
#define polymer_simple_animator_hpp

#include "polymer-core/util/util.hpp"
#include "polymer-core/math/math-common.hpp"

#include <list>
#include <functional>
#include <thread>

// https://github.com/LiveMirror/cpptweener/blob/fee53371b05e94e24f3df40335023205f8d0abd9/src/CppTweener.cpp

// Credit to https://theorangeduck.com/page/spring-roll-call
// MIT License
namespace spring
{

    inline float fast_negexp(float x)
    {
        return 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
    }

    inline float squaref(float x)
    {
        return x * x;
    }

    inline float halflife_to_damping(float halflife, float eps = 1e-5f)
    {
        return (4.0f * 0.69314718056f) / (halflife + eps);
    }

    inline float damping_to_halflife(float damping, float eps = 1e-5f)
    {
        return (4.0f * 0.69314718056f) / (damping + eps);
    }

    inline float frequency_to_stiffness(float frequency)
    {
        return squaref(POLYMER_TAU * frequency);
    }

    inline float stiffness_to_frequency(float stiffness)
    {
        return sqrtf(stiffness) / (POLYMER_TAU);
    }

    inline float critical_halflife(float frequency)
    {
        return damping_to_halflife(sqrtf(frequency_to_stiffness(frequency) * 4.0f));
    }

    inline float critical_frequency(float halflife)
    {
        return stiffness_to_frequency(squaref(halflife_to_damping(halflife)) / 4.0f);
    }

    inline float resonant_frequency(float goal_frequency, float halflife)
    {
        float d                  = halflife_to_damping(halflife);
        float goal_stiffness     = frequency_to_stiffness(goal_frequency);
        float resonant_stiffness = goal_stiffness - (d * d) / 4.0f;
        return stiffness_to_frequency(resonant_stiffness);
    }

    inline void spring_damper_exact_stiffness_damping(float & x, float & v, float x_goal, float v_goal, float stiffness, float damping, float dt, float eps = 1e-5f)
    {
        float g = x_goal;
        float q = v_goal;
        float s = stiffness;
        float d = damping;
        float c = g + (d * q) / (s + eps);
        float y = d / 2.0f;

        if (fabs(s - (d * d) / 4.0f) < eps)  // Critically Damped
        {
            float j0 = x - c;
            float j1 = v + j0 * y;

            float eydt = fast_negexp(y * dt);

            x = j0 * eydt + dt * j1 * eydt + c;
            v = -y * j0 * eydt - y * dt * j1 * eydt + j1 * eydt;
        }
        else if (s - (d * d) / 4.0f > 0.0)  // Under Damped
        {
            float w = sqrtf(s - (d * d) / 4.0f);
            float j = sqrtf(squaref(v + y * (x - c)) / (w * w + eps) + squaref(x - c));
            float p = atan((v + (x - c) * y) / (-(x - c) * w + eps));

            j = (x - c) > 0.0f ? j : -j;

            float eydt = fast_negexp(y * dt);

            x = j * eydt * cosf(w * dt + p) + c;
            v = -y * j * eydt * cosf(w * dt + p) - w * j * eydt * sinf(w * dt + p);
        }
        else if (s - (d * d) / 4.0f < 0.0)  // Over Damped
        {
            float y0 = (d + sqrtf(d * d - 4 * s)) / 2.0f;
            float y1 = (d - sqrtf(d * d - 4 * s)) / 2.0f;
            float j1 = (c * y0 - x * y0 - v) / (y1 - y0);
            float j0 = x - j1 - c;

            float ey0dt = fast_negexp(y0 * dt);
            float ey1dt = fast_negexp(y1 * dt);

            x = j0 * ey0dt + j1 * ey1dt + c;
            v = -y0 * j0 * ey0dt - y1 * j1 * ey1dt;
        }
    }

    inline void critical_spring_damper_exact(float & x, float & v, float x_goal, float v_goal, float halflife, float dt)
    {

        float d    = halflife_to_damping(halflife);
        float c    = x_goal + (d * v_goal) / ((d * d) / 4.0f);
        float y    = d / 2.0f;
        float j0   = x - c;
        float j1   = v + j0 * y;
        float eydt = fast_negexp(y * dt);

        x = eydt * (j0 + j1 * dt) + c;
        v = eydt * (v - j1 * y * dt);
    }

}

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

    inline playback_state operator|(playback_state a, playback_state b)
    {
        return static_cast<playback_state>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
    }

    inline playback_state operator&(playback_state a, playback_state b)
    {
        return static_cast<playback_state>(static_cast<unsigned int>(a) & static_cast<unsigned int>(b));
    }

    inline playback_state operator~(playback_state a)
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
        std::function<void()> on_loop;
        playback_state state {playback_state::forward};
        std::string name;
    };

    class simple_animator
    {
        std::list<tween_event> tweens;
        double now_seconds = 0.0f;

    public:

        void update(const double dt)
        {
            now_seconds += dt;

            for (auto it = std::begin(tweens); it != std::end(tweens);)
            {
                if (now_seconds < it->t1)
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
                        it->forward_update_impl(dx);
                    }
                    if (is_set(it->state & playback_state::reverse))  
                    {
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

                        if (it->on_loop) it->on_loop();

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
        
        void cancel(const std::string & name)
        {
            for (auto it = tweens.begin(); it != tweens.end(); ++it)
            {
                if (it->name == name)
                {
                    tweens.erase(it);
                    break;
                }
            }
        }

        template<class VariableType, class EasingFunc>
        tween_event & add_tween(const std::string & name, VariableType * variable, VariableType target_value, double duration_sec, EasingFunc ease, double delay_sec = 0.f)
        {   
            VariableType initialValue = *variable;

            auto forward_update = [=](double t)
            {
                *variable = static_cast<VariableType>(initialValue * (1.f - ease(t)) + target_value * ease(t));
            };

            auto reverse_update = [=](double t)
            {
                *variable = static_cast<VariableType>(target_value * (1.f - ease(t)) + initialValue * ease(t));
            };

            tweens.emplace_back(name, variable, delay_sec + now_seconds, delay_sec + now_seconds + duration_sec, duration_sec, forward_update, reverse_update);
            return tweens.back();
        }

        template <class VariableType, class EasingFunc>
        tween_event & add_tween(VariableType * variable, VariableType target_value, double duration_sec, EasingFunc ease, double delay_sec = 0.f)
        {
            return add_tween<VariableType, EasingFunc>("", variable, target_value, duration_sec, ease, delay_sec);
        }

    };

}

#endif // end polymer_simple_animator_hpp
