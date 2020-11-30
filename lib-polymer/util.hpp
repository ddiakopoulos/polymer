#pragma once

#ifndef polymer_utils_hpp
#define polymer_utils_hpp

#include <sstream>
#include <iostream>
#include <vector>
#include <random>
#include <memory>
#include <chrono>
#include <mutex>
#include <functional>

#if (defined(__linux) || defined(__unix) || defined(__posix) || defined(__LINUX__) || defined(__linux__))
    #define POLYMER_PLATFORM_LINUX 1
#elif (defined(_WIN64) || defined(_WIN32) || defined(__CYGWIN32__) || defined(__MINGW32__))
    #define POLYMER_PLATFORM_WINDOWS 1
#elif (defined(MACOSX) || defined(__DARWIN__) || defined(__APPLE__))
    #define POLYMER_PLATFORM_OSX 1
#endif

#if (defined(WIN_32) || defined(__i386__) || defined(i386) || defined(__x86__))
    #define POLYMER_ARCH_32 1
#elif (defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__) || defined(_M_X64) || defined(__ia64__) || defined(_M_IA64))
    #define POLYMER_ARCH_64 1
#endif

#if (defined(__clang__))
    #define POLYMER_COMPILER_CLANG 1
#elif (defined(__GNUC__))
    #define POLYMER_COMPILER_GCC 1
#elif (defined _MSC_VER)
    #define POLYMER_COMPILER_VISUAL_STUDIO 1
#endif

#if defined (POLYMER_PLATFORM_WINDOWS)
    #define GL_PUSH_ALL_ATTRIB() glPushAttrib(GL_ALL_ATTRIB_BITS);
    #define GL_POP_ATTRIB() glPopAttrib();
#else
    #define GL_PUSH_ALL_ATTRIB();
    #define GL_POP_ATTRIB();
#endif

#if defined(POLYMER_PLATFORM_WINDOWS)
    #define ALIGNED(n) __declspec(align(n))
#else
    #define ALIGNED(n) alignas(n)
#endif

#ifdef POLYMER_PLATFORM_WINDOWS
    #include <malloc.h>
#endif

#include <stdlib.h>
#include <algorithm>
#include <cstddef>

inline void * polymer_aligned_alloc(size_t size, size_t align)
{
    const size_t min_align = std::max(align, sizeof(max_align_t));
#ifdef _MSC_VER
    return _aligned_malloc(size, min_align);
#else
    void* ptr = nullptr;
    posix_memalign(&ptr, min_align, size);
    return ptr;
#endif
}

inline void polymer_aligned_free(void * ptr)
{
#ifdef _MSC_VER
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

#include "math-core.hpp"

namespace polymer
{
	struct viewport_t
	{
		float2 bmin, bmax;
		uint32_t texture;
	};

    // 32 bit Fowler–Noll–Vo Hash
    inline uint32_t poly_hash_fnv1a(const std::string & str)
    {
        static const uint32_t fnv1aBase32 = 0x811C9DC5u;
        static const uint32_t fnv1aPrime32 = 0x01000193u;

        uint32_t result = fnv1aBase32;

        for (auto & c : str)
        {
            result ^= static_cast<uint32_t>(c);
            result *= fnv1aPrime32;
        }
        return result;
    }

    static inline uint64_t system_time_ns()
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }

    class scoped_timer
    {
        std::string message;
        std::chrono::high_resolution_clock::time_point t0;
    public:
        scoped_timer(std::string message) : message{ std::move(message) }, t0{ std::chrono::high_resolution_clock::now() } {}
        ~scoped_timer()
        {
			const auto timestamp_ms = (std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - t0).count() * 1000);
            std::cout << message << " completed in " << std::to_string(timestamp_ms) << " ms\n";
        }
    };

    class manual_timer
    {
        std::chrono::high_resolution_clock::time_point t0;
        double timestamp{ 0.f };
    public:
        void start() { t0 = std::chrono::high_resolution_clock::now(); }
        void stop() { timestamp = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - t0).count() * 1000; }
        const double running() { return std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - t0).count() * 1000; }
        const double & get() { return timestamp; }
    };

    class uniform_random_gen
    {
        std::mt19937_64 gen;
        std::uniform_real_distribution<float> full { 0.f, 1.f };
        std::uniform_real_distribution<float> safe { 0.001f, 0.999f };
        std::uniform_real_distribution<float> two_pi { 0.f, float(6.2831853071795862) };
    public:
        uniform_random_gen() 
        { 
            std::random_device r; 
            std::seed_seq seed{r(), r()};
            gen = std::mt19937_64(seed); 
        }
        uniform_random_gen (const uniform_random_gen & r) {}
        float random_float() { return full(gen); }
        float random_float(float max) { std::uniform_real_distribution<float> custom(0.f, max); return custom(gen); }
        float random_float(float min, float max) { std::uniform_real_distribution<float> custom(min, max); return custom(gen); }
        float random_float_sphere() { return two_pi(gen); }
        float random_float_safe() { return safe(gen); }
        uint32_t random_uint(uint32_t max) { std::uniform_int_distribution<uint32_t> dInt(0, max); return dInt(gen); }
        int32_t random_int(int32_t min, int32_t max) { std::uniform_int_distribution<int32_t> dInt(min, max); return dInt(gen); }
    };

    class non_copyable
    {
    protected:
        non_copyable() = default;
        ~non_copyable() = default;
        non_copyable (const non_copyable & r) = delete;
        non_copyable & operator = (const non_copyable & r) = delete;
    };

    class non_movable
    {
    protected:
        non_movable() = default;
        ~non_movable() = default;
    private:
        non_movable(non_movable &&) = delete;
        non_movable & operator = (non_movable &&) = delete;
    };
     
    template <typename T>
    class singleton : public non_copyable
    {
        singleton(const singleton<T> &);
        singleton & operator = (const singleton<T> &);
    protected:
        static T * single;
        singleton() = default;
        ~singleton() = default;
    public:
        static T * get() { if (!single) single = new T(); return single; };
    };

    class try_locker
    {
        std::mutex & mutex;
        bool locked{ false };
    public:
        try_locker(std::mutex & m) : mutex(m) { if (mutex.try_lock()) locked = true; }
        ~try_locker() { if (locked) mutex.unlock(); }
        bool is_locked() const { return locked; }
    };

    // Evenly splits a vector of items into n buckets
    template<typename T>
    inline std::vector<std::vector<T>> make_workgroup(const std::vector<T> & work, const size_t n)
    {
        std::vector<std::vector<T>> result;

        size_t length = work.size() / n;
        size_t remain = work.size() % n;
        size_t begin{ 0 }, end{ 0 };

        for (size_t i = 0; i < std::min(n, work.size()); ++i)
        {
            end += (remain > 0) ? (length + !!(remain--)) : length;
            result.push_back(std::vector<T>(work.begin() + begin, work.begin() + end));
            begin = end;
        }
        return result;
    }

    inline std::string codepoint_to_utf8(uint32_t codepoint)
    {
        int n = 0;
        if (codepoint < 0x80) n = 1;
        else if (codepoint < 0x800) n = 2;
        else if (codepoint < 0x10000) n = 3;
        else if (codepoint < 0x200000) n = 4;
        else if (codepoint < 0x4000000) n = 5;
        else if (codepoint <= 0x7fffffff) n = 6;
        
        std::string str(n, ' ');
        switch (n)
        {
            case 6: str[5] = 0x80 | (codepoint & 0x3f); codepoint = codepoint >> 6; codepoint |= 0x4000000;
            case 5: str[4] = 0x80 | (codepoint & 0x3f); codepoint = codepoint >> 6; codepoint |= 0x200000;
            case 4: str[3] = 0x80 | (codepoint & 0x3f); codepoint = codepoint >> 6; codepoint |= 0x10000;
            case 3: str[2] = 0x80 | (codepoint & 0x3f); codepoint = codepoint >> 6; codepoint |= 0x800;
            case 2: str[1] = 0x80 | (codepoint & 0x3f); codepoint = codepoint >> 6; codepoint |= 0xc0;
            case 1: str[0] = codepoint;
        }
        
        return str;
    }

    class periodic_function 
    {
        typedef std::chrono::steady_clock::duration timeduration;

        timeduration remaining;
        timeduration period;
        std::function<void()> func;

    public:

        periodic_function(timeduration period, std::function<void()> func) 
            : period(period), remaining(period), func(func)
        {
            remaining = period;
        }

        void update(const timeduration dt)
        {
            remaining -= dt;
            if (remaining <= std::chrono::steady_clock::duration::zero())
            {
                func();
                while (remaining < std::chrono::steady_clock::duration::zero())
                {
                    remaining += period;
                }
            }
        }

        void reset() { remaining = period; }
    };

    struct as_string
    {
        std::ostringstream ss;
        operator std::string() const { return ss.str(); }
        template<class T> as_string & operator << (const T & val) { ss << val; return *this; }
    };

    inline void pretty_print(const char * file, const int line, const std::string & message)
    {
        std::cout << file << " : " << line << " - " << message << std::endl;
    }

    #define POLYMER_ERROR(...) polymer::pretty_print(__FILE__, __LINE__, polymer::as_string() << __VA_ARGS__)
    #define POLYMER_INFO(...) polymer::pretty_print(__FILE__, __LINE__, polymer::as_string() << __VA_ARGS__)

} // end namespace polymer

#endif // end polymer_utils_hpp
