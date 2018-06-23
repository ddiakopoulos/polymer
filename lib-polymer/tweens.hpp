#pragma once

#ifndef animation_tweens_hpp
#define animation_tweens_hpp

#include "util.hpp"
#include "math-common.hpp"
#include <list>
#include <functional>
#include <thread>

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
            return -0.5f * (std::cos((float)POLYMER_PI * t) - 1.f);
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

// A simple playback manager for basic animation curves. 
// todo - explore threaded approach, on_start callback & delay
namespace polymer
{
    class tween_event
    {
        void * variable;
        float t0, t1;
        std::function<void(float t)> update_impl;
        friend class simple_animator;
    public:
        tween_event(void * v, float t0, float t1, std::function<void(float t)> update)
            : variable(v), t0(t0), t1(t1), update_impl(update) {}
        std::function<void()> on_finish;
        std::function<void(float t)> on_update;
    };

    class simple_animator
    {
        std::list<tween_event> tweens;
        float now_seconds = 0.0f;
    public:

        void update(const float dt)
        {
            now_seconds += dt;
            for (auto it = begin(tweens); it != end(tweens);)
            {
                if (now_seconds < it->t1)
                {
                    const float dx = static_cast<float>((now_seconds - it->t0) / (it->t1 - it->t0));
                    if (it->on_update) it->on_update(dx);
                    it->update_impl(dx);
                    ++it;
                }
                else
                {
                    it->update_impl(1.f);
                    if (it->on_update) it->on_update(1.f);
                    if (it->on_finish) it->on_finish();
                    it = tweens.erase(it);
                }
            }
        }

        void cancel_all()
        {
            tweens.clear();
        }

        template<class VariableType, class EasingFunc>
        tween_event & add_tween(VariableType * variable, VariableType targetValue, float duration_seconds, EasingFunc ease)
        {
            VariableType initialValue = *variable;
            auto update = [initialValue, variable, targetValue, ease](float t)
            {
                *variable = static_cast<VariableType>(initialValue * (1.f - ease(t)) + targetValue * ease(t));
            };

            tweens.emplace_back(variable, now_seconds, now_seconds + duration_seconds, update);
            return tweens.back();
        }

    };

}

#endif // end animation_tweens_hpp
