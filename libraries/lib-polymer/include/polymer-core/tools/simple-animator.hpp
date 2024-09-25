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
        inline static float ease_in_out(const float t) { return t; }
    };

    struct sine
    {
        inline static float ease_in_out(float t)
        {
            const float b = 0.f;
            const float c = 1.f;
            const float d = 1.f;
            return -0.5f * (std::cos((float)POLYMER_PI * t) - 1.f);
        }

        inline static float ease_in(float t)
        {
            const float b = 0.f;
            const float c = 1.f;
            const float d = 1.f;
            return -c * std::cos(t / d * (POLYMER_HALF_PI)) + c + b;
        }

        inline static float ease_out(float t)
        {
            const float b = 0.f;
            const float c = 1.f;
            const float d = 1.f;
            return c * std::sin(t / d * (POLYMER_HALF_PI)) + b;
        }
    };

    struct smoothstep
    {
        inline static float ease_in_out(const float t)
        {
            float scale = t * t * (3.f - 2.f * t);
            return scale * 1.0f;
        }
    };

    struct circular
    {
        inline static float ease_in_out(float t)
        {
            t *= 2;
            if (t < 1) return -0.5f * (sqrt(1 - t * t) - 1);
            else
            {
                t -= 2;
                return 0.5f * (sqrt(1 - t * t) + 1);
            }
        }
    };

    struct exp
    {
        inline static float ease_in_out(float t)
        {
            if (t == 0.f) return 0.f;
            if (t == 1.f) return 1.f;
            t *= 2;
            if (t < 1) return 0.5f * std::powf(2, 10 * (t - 1));
            return 0.5f * (-std::powf(2, -10 * (t - 1)) + 2);
        }

        inline static float ease_in(float t)
        {
            const float b = 0.f;
            const float c = 1.f;
            const float d = 1.f;
            return (t == 0) ? b : c * pow(2, 10 * (t / d - 1)) + b;
        }

        inline static float ease_out(float t)
        {
            const float b = 0.f;
            const float c = 1.f;
            const float d = 1.f;
            return (t == d) ? b + c : c * (-pow(2, -10 * t / d) + 1) + b;
        }
    };

    struct cubic
    {
        inline static float ease_in_out(float t)
        {
            t *= 2;
            if (t < 1) return 0.5f * t*t*t;
            t -= 2;
            return 0.5f*(t*t*t + 2);
        }

        inline static float ease_in(float t)
        {
            const float b = 0.f;
            const float c = 1.f;
            const float d = 1.f;
            return c * (t /= d) * t * t + b;
        }

        inline static float ease_out(float t)
        {
            const float b = 0.f;
            const float c = 1.f;
            const float d = 1.f;
            return c * ((t = t / d - 1) * t * t + 1) + b;
        }
    };

    struct quartic
    {
        inline static float ease_in_out(float t)
        {
            t *= 2;
            if (t < 1) return 0.5f*t*t*t*t;
            else
            {
                t -= 2;
                return -0.5f * (t*t*t*t - 2.0f);
            }
        }
    };

}

namespace polymer
{
    
    enum playback_state
    {
        none             = 0x1,
        loop             = 0x2,
        playback_forward = 0x4,
        playback_reverse = 0x8,
    };

    class tween_event
    {
        void * variable;
        float t0, t1;
        std::function<void(float t)> forward_update_impl;
        std::function<void(float t)> reverse_update_impl;
        friend class simple_animator;
        float duration_seconds;
    public:
        tween_event(const std::string & name, void * v, float t0, float t1, float duration, std::function<void(float t)> fwd, std::function<void(float t)> rvs)
            : name(name), variable(v), t0(t0), t1(t1), duration_seconds(duration), forward_update_impl(fwd), reverse_update_impl(rvs) {}
        std::function<void()> on_finish;
        std::function<void(float t)> on_update;
        int state {static_cast<int>(playback_forward)};
        std::string name;
    };

    // A simple playback manager for basic animation curves. 
    // @todo - threading, on_start callback, trigger delay, polymer::property support
    class simple_animator
    {
        std::list<tween_event> tweens;
        float now_seconds = 0.0f;
    public:

        void update(const float dt)
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

                    const float dx = static_cast<float>((now_seconds - it->t0) / (it->t1 - it->t0));
                    if (it->on_update) it->on_update(dx);

                    if (it->state & playback_forward) 
                    {
                        //std::cout << "forward" << std::endl;
                        it->forward_update_impl(dx);
                    }
                    if (it->state & playback_reverse)  
                    {
                        //std::cout << "reverse" << std::endl;
                        it->reverse_update_impl(dx);
                    }

                    ++it;
                }
                else
                {
                    if (it->state & loop)
                    {
                        it->t0 = now_seconds;
                        it->t1 = now_seconds + it->duration_seconds;

                        if (it->state & playback_forward)
                        {
                            it->state = static_cast<playback_state>(it->state & ~playback_forward); // unset fwd
                            it->state = static_cast<playback_state>(it->state | playback_reverse); // set reverse
                        }
                        else
                        {
                            it->state = static_cast<playback_state>(it->state & ~playback_reverse); 
                            it->state = static_cast<playback_state>(it->state | playback_forward);
                        }
                    }
                    else
                    {   
                        if (it->state & playback_forward) it->forward_update_impl(1.0);
                        else it->reverse_update_impl(1.0);
                        if (it->on_update) it->on_update(1.f);
                        if (it->on_finish) it->on_finish();
                        it->state = none;
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
        tween_event & add_tween(const std::string & name, VariableType * variable, VariableType targetValue, float duration_seconds, EasingFunc ease, float delay_sec = 0.f)
        {   
            VariableType initialValue = *variable;

            auto forward_update = [=](float t)
            {
                *variable = static_cast<VariableType>(initialValue * (1.f - ease(t)) + targetValue * ease(t));
            };

            auto reverse_update = [=](float t)
            {
                *variable = static_cast<VariableType>(targetValue * (1.f - ease(t)) + initialValue * ease(t));
            };

            tweens.emplace_back(name, variable, delay_sec + now_seconds, delay_sec + now_seconds + duration_seconds, duration_seconds, forward_update, reverse_update);
            return tweens.back();
        }

    };

}

#endif // end polymer_simple_animator_hpp
