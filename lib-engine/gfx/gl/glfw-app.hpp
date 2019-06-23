#pragma once

#ifndef glfw_app_h
#define glfw_app_h

#include "util.hpp"
#include "math-core.hpp"

#include <thread>
#include <chrono>
#include <codecvt>
#include <string>

#if defined(POLYMER_PLATFORM_WINDOWS)
    #define GL_GLEXT_PROTOTYPES
    #include <glad/glad.h>
    #pragma warning(push)
    #pragma warning(disable : 4800)
#endif

#define GLFW_INCLUDE_GLU
#include <GLFW/glfw3.h>

namespace polymer
{

    // fixme - move to events file
    struct app_update_event
    {
        double elapsed_s;
        float timestep_ms;
        float framesPerSecond;
        uint64_t elapsedFrames;
    };

    // fixme - move to events file
    struct app_input_event
    {
        enum Type { CURSOR, MOUSE, KEY, CHAR, SCROLL };

        GLFWwindow * window;
        int2 windowSize;

        Type type;
        int action;
        int mods;

        float2 cursor;
        bool drag = false;

        int2 value; // button, key, codepoint, scrollX, scrollY

        bool is_down() const { return action != GLFW_RELEASE; }
        bool is_up() const { return action == GLFW_RELEASE; }

        bool using_shift_key() const { return mods & GLFW_MOD_SHIFT; };
        bool using_control_key() const { return mods & GLFW_MOD_CONTROL; };
        bool using_alt_key() const { return mods & GLFW_MOD_ALT; };
        bool using_super_key() const { return mods & GLFW_MOD_SUPER; };
    };

    struct gl_context
    {
        GLFWwindow * hidden_window;
        gl_context();
        ~gl_context();
    };

    class glfw_window
    {
        uint32_t drag_count { 0 };
        void preprocess_input(app_input_event & event);
        void consume_character(uint32_t codepoint);
        void consume_key(int key, int action);
        void consume_mousebtn(int button, int action);
        void consume_cursor(double xpos, double ypos);
        void consume_scroll(double xoffset, double yoffset);

    protected:

        GLFWwindow * window;
        gl_context * gl_ctx;

    public:

        glfw_window(gl_context * context, int w, int h, const std::string title, int samples = 1);
        virtual ~glfw_window();

        int get_mods() const;
        virtual void on_update(const app_update_event & e) { }
        virtual void on_draw() { }
        virtual void on_window_focus(bool focused) { }
        virtual void on_window_resize(int2 size) { }
        virtual void on_window_close() { }
        virtual void on_input(const app_input_event & event) { }
        virtual void on_drop(std::vector<std::string> names) { }
        gl_context * get_shared_gl_context() const { return gl_ctx; }
        GLFWwindow * get_window() const { return window; }
    };
        
    class polymer_app : public glfw_window
    {
        uint64_t elapsedFrames{ 0 };
        double fps{ 0 };
        double fpsTime{ 0 };
        bool fullscreenState{ false };

        void screenshot_impl();
        static void enter_fullscreen(GLFWwindow * window, int2 & windowedSize, int2 & windowedPos);
        static void exit_fullscreen(GLFWwindow * window, const int2 & windowedSize, const int2 & windowedPos);
        int2 windowedSize, windowedPos;
        std::string screenshotPath;

    public:

        polymer_app(int w, int h, const std::string windowTitle, int glfwSamples = 1);
        ~polymer_app();

        void main_loop();
        void exit();
        void set_fullscreen(bool state);
        bool get_fullscreen();
        void request_screenshot(const std::string & filename);
    };
        
    extern int Main(int argc, char * argv[]);
        
} // end namespace polymer

#endif

#pragma warning(pop)

#define IMPLEMENT_MAIN(...) namespace polymer { int main(int argc, char * argv[]); } int main(int argc, char * argv[]) { return polymer::Main(argc, argv); } int polymer::Main(__VA_ARGS__)
