#ifndef animation_tweens_hpp
#define animation_tweens_hpp

#include "util.hpp"
#include "math-common.hpp"
#include <list>
#include <functional>
#include <thread>

namespace tween
{
    struct Linear
    {
        inline static float ease_in_out(const float t) { return t; }
    };

    struct Sine
    {
        inline static float ease_in_out(float t)
        {
            return -0.5f * (std::cos((float)POLYMER_PI * t) - 1.f);
        }
    };

    struct Smoothstep
    {
        inline static float ease_in_out(const float t)
        {
            float scale = t * t * (3.f - 2.f * t);
            return scale * 1.0f;
        }
    };

    struct Circular
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

    struct Exponential
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

    struct Cubic
    {
        inline static float ease_in_out(float t)
        {
            t *= 2;
            if (t < 1) return 0.5f * t*t*t;
            t -= 2;
            return 0.5f*(t*t*t + 2);
        }
    };

    struct Quartic
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
    class simple_animator
    {
        struct Tween
        {
            void * variable;
            float t0, t1;
            std::function<void(float t)> on_update;
            std::function<void()> on_finish;
        };

        std::list<Tween> tweens;
        float now = 0.0f;

    public:

        void update(float timestep)
        {
            now += timestep;
            for (auto it = begin(tweens); it != end(tweens);)
            {
                if (now < it->t1)
                {
                    it->on_update(static_cast<float>((now - it->t0) / (it->t1 - it->t0)));
                    ++it;
                }
                else
                {
                    it->on_update(1.0f);
                    if (it->on_finish) it->on_finish();
                    it = tweens.erase(it);
                }
            }
        }

        template<class VariableType, class EasingFunc>
        Tween & add_tween(VariableType * variable, VariableType targetValue, float seconds, EasingFunc ease)
        {
            VariableType initialValue = *variable;
            auto updateFunction = [variable, initialValue, targetValue, ease](float t)
            {
                *variable = static_cast<VariableType>(initialValue * (1 - ease(t)) + targetValue * ease(t));
            };

            tweens.push_back({ variable, now, now + seconds, updateFunction });
            return tweens.back();
        }

    };

}

#endif // end animation_tweens_hpp
