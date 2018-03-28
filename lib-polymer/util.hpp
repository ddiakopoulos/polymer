#ifndef main_util_h
#define main_util_h

#include <sstream>
#include <iostream>
#include <vector>
#include <random>
#include <memory>
#include <chrono>
#include <mutex>

#if (defined(__linux) || defined(__unix) || defined(__posix) || defined(__LINUX__) || defined(__linux__))
    #define ANVIL_PLATFORM_LINUX 1
#elif (defined(_WIN64) || defined(_WIN32) || defined(__CYGWIN32__) || defined(__MINGW32__))
    #define ANVIL_PLATFORM_WINDOWS 1
#elif (defined(MACOSX) || defined(__DARWIN__) || defined(__APPLE__))
    #define ANVIL_PLATFORM_OSX 1
#endif

#if (defined(WIN_32) || defined(__i386__) || defined(i386) || defined(__x86__))
    #define ANVIL_ARCH_32 1
#elif (defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__) || defined(_M_X64) || defined(__ia64__) || defined(_M_IA64))
    #define ANVIL_ARCH_64 1
#endif

#if (defined(__clang__))
    #define ANVIL_COMPILER_CLANG 1
#elif (defined(__GNUC__))
    #define ANVIL_COMPILER_GCC 1
#elif (defined _MSC_VER)
    #define ANVIL_COMPILER_VISUAL_STUDIO 1
#endif

#if defined (ANVIL_PLATFORM_WINDOWS)
    #define GL_PUSH_ALL_ATTRIB() glPushAttrib(GL_ALL_ATTRIB_BITS);
    #define GL_POP_ATTRIB() glPopAttrib();
#else
    #define GL_PUSH_ALL_ATTRIB();
    #define GL_POP_ATTRIB();
#endif

#if defined(ANVIL_PLATFORM_WINDOWS)
    #define ALIGNED(n) __declspec(align(n))
#else
    #define ALIGNED(n) alignas(n)
#endif

namespace avl
{
    class try_locker
    {
        std::mutex & mutex;
        bool locked{ false };
    public:
        try_locker(std::mutex & m) : mutex(m) { if (mutex.try_lock()) locked = true; }
        ~try_locker() { if (locked) mutex.unlock(); }
        bool is_locked() const { return locked; }
    };

    class scoped_timer
    {
        std::string message;
        std::chrono::high_resolution_clock::time_point t0;
    public:
        scoped_timer(std::string message) : message{ std::move(message) }, t0{ std::chrono::high_resolution_clock::now() } {}
        ~scoped_timer()
        {
            std::cout << message << " completed in " << std::to_string((std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - t0).count() * 1000)) << " ms" << std::endl;
        }
    };

    class manual_timer
    {
        std::chrono::high_resolution_clock::time_point t0;
        double timestamp{ 0.f };
    public:
        void start() { t0 = std::chrono::high_resolution_clock::now(); }
        void stop() { timestamp = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - t0).count() * 1000; }
        const double & get() { return timestamp; }
    };

    #define AVL_SCOPED_TIMER(MESSAGE) scoped_timer scoped_timer ## __LINE__(MESSAGE)

    class UniformRandomGenerator
    {
        std::random_device rd;
        std::mt19937_64 gen;
        std::uniform_real_distribution<float> full { 0.f, 1.f };
        std::uniform_real_distribution<float> safe { 0.001f, 0.999f };
        std::uniform_real_distribution<float> two_pi { 0.f, float(6.2831853071795862) };
    public:
        UniformRandomGenerator() : rd(), gen(rd()) { }
        float random_float() { return full(gen); }
        float random_float(float max) { std::uniform_real_distribution<float> custom(0.f, max); return custom(gen); }
        float random_float(float min, float max) { std::uniform_real_distribution<float> custom(min, max); return custom(gen); }
        float random_float_sphere() { return two_pi(gen); }
        float random_float_safe() { return safe(gen); }
        uint32_t random_int(uint32_t max) { std::uniform_int_distribution<uint32_t> dInt(0, max); return dInt(gen); }
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
    
    class Noncopyable
    {
    protected:
        Noncopyable() = default;
        ~Noncopyable() = default;
        Noncopyable (const Noncopyable& r) = delete;
        Noncopyable & operator = (const Noncopyable& r) = delete;
    };
     
    template <typename T>
    class Singleton : public Noncopyable
    {
    private:
        Singleton(const Singleton<T> &);
        Singleton & operator = (const Singleton<T> &);
    protected:
        static T * single;
        Singleton() = default;
        ~Singleton() = default;
    public:
        static T * get_instance() { if (!single) single = new T(); return single; };
    };

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
    
    inline void flip_image(unsigned char * pixels, const uint32_t width, const uint32_t height, const uint32_t bytes_per_pixel)
    {
        const size_t stride = width * bytes_per_pixel;
        std::vector<unsigned char> row(stride);
        unsigned char * low = pixels;
        unsigned char * high = &pixels[(height - 1) * stride];
        
        for (; low < high; low += stride, high -= stride)
        {
            memcpy(row.data(), low, stride);
            memcpy(low, high, stride);
            memcpy(high, row.data(), stride);
        }
    }
}

#define ANVIL_ERROR(...) avl::pretty_print(__FILE__, __LINE__, avl::as_string() << __VA_ARGS__)
#define ANVIL_INFO(...) avl::pretty_print(__FILE__, __LINE__, avl::as_string() << __VA_ARGS__)

#endif
