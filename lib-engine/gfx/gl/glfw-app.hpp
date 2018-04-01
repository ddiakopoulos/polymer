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
    #define GLEW_STATIC
    #define GL_GLEXT_PROTOTYPES
    #include "glew.h"
    #pragma warning(push)
    #pragma warning(disable : 4800)
#endif

#define GLFW_INCLUDE_GLU
#include <GLFW/glfw3.h>

namespace polymer
{


    // fixme - move to events file
    struct UpdateEvent
    {
        double elapsed_s;
        float timestep_ms;
        float framesPerSecond;
        uint64_t elapsedFrames;
    };

    // fixme - move to events file
    struct InputEvent
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

        gl_context()
        {
            glfwDefaultWindowHints();
            glfwWindowHint(GLFW_VISIBLE, 0);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
            glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
            hidden_window = glfwCreateWindow(1, 1, "", nullptr, nullptr);
            if (!hidden_window) throw std::runtime_error("glfwCreateWindow(...) failed");
            glfwMakeContextCurrent(hidden_window);

            POLYMER_INFO("GL_VERSION =  " << (char *)glGetString(GL_VERSION));
            POLYMER_INFO("GL_SHADING_LANGUAGE_VERSION =  " << (char *)glGetString(GL_SHADING_LANGUAGE_VERSION));
            POLYMER_INFO("GL_VENDOR =   " << (char *)glGetString(GL_VENDOR));
            POLYMER_INFO("GL_RENDERER = " << (char *)glGetString(GL_RENDERER));

#if defined(POLYMER_PLATFORM_WINDOWS)
            glewExperimental = GL_TRUE;
            if (GLenum err = glewInit())
                throw std::runtime_error(std::string("glewInit() failed - ") + (const char *)glewGetErrorString(err));
            POLYMER_INFO("GLEW_VERSION = " << (char *)glewGetString(GLEW_VERSION));
#endif

            std::vector<std::pair<std::string, bool>> extensions {
                { "GL_EXT_direct_state_access", false },
                { "GL_KHR_debug", false },
                { "GL_EXT_blend_equation_separate", false },
                { "GL_EXT_framebuffer_sRGB", false },
                { "GL_EXT_pixel_buffer_object", false },
            };
            has_gl_extension(extensions);

            std::ostringstream ss;
            ss << "Unsupported extensions: ";

            bool anyUnsupported = false;
            for (auto & e : extensions)
            {
                if (!e.second)
                {
                    ss << ' ' << e.first;
                    anyUnsupported = true;
                }
            }
            if (anyUnsupported) { throw std::runtime_error(ss.str()); }
        }


        ~gl_context()
        {
            if (hidden_window) glfwDestroyWindow(hidden_window);
        }
    };

    class glfw_window
    {
        bool isDragging{ false };
        void consume_character(uint32_t codepoint);
        void consume_key(int key, int action);
        void consume_mousebtn(int button, int action);
        void consume_cursor(double xpos, double ypos);
        void consume_scroll(double xoffset, double yoffset);

        void preprocess_input(InputEvent & event)
        {
            if (event.type == InputEvent::MOUSE)
            {
                if (event.is_down()) isDragging = true;
                else if (event.is_up()) isDragging = false;
            }
            event.drag = isDragging;
            on_input(event);
        }

    protected:

        GLFWwindow * window;

    public:

        glfw_window(gl_context * context, int w, int h, const std::string title, int samples = 1)
        {
            if (!glfwInit()) std::exit(EXIT_FAILURE);

            glfwWindowHint(GLFW_SAMPLES, samples);
            glfwWindowHint(GLFW_SRGB_CAPABLE, true);

#ifdef _DEBUG
            glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif

            glfwSetErrorCallback(s_error_callback);

            window = glfwCreateWindow(w, h, title.c_str(), NULL, context->hidden_window);
            if (!window) throw std::runtime_error("failed to open glfw window...");

            glfwMakeContextCurrent(window);

#ifdef _DEBUG
            glEnable(GL_DEBUG_OUTPUT);
            glDebugMessageCallback(&gl_debug_callback, nullptr);
#endif

            glfwSetWindowUserPointer(window, this);
            glfwSetWindowFocusCallback(window, [](GLFWwindow * window, int focused) { auto win = (glfw_window *)(glfwGetWindowUserPointer(window)); win->on_window_focus(!!focused); });
            glfwSetWindowSizeCallback(window, [](GLFWwindow * window, int width, int height) { auto win = (glfw_window *)(glfwGetWindowUserPointer(window)); win->on_window_resize({ width, height }); });
            glfwSetCharCallback(window, [](GLFWwindow * window, unsigned int codepoint) { auto win = (glfw_window *)(glfwGetWindowUserPointer(window)); win->consume_character(codepoint);  });
            glfwSetKeyCallback(window, [](GLFWwindow * window, int key, int, int action, int) { auto win = (glfw_window *)(glfwGetWindowUserPointer(window)); win->consume_key(key, action); });
            glfwSetMouseButtonCallback(window, [](GLFWwindow * window, int button, int action, int) { auto win = (glfw_window *)(glfwGetWindowUserPointer(window)); win->consume_mousebtn(button, action);  });
            glfwSetCursorPosCallback(window, [](GLFWwindow * window, double xpos, double ypos) { auto win = (glfw_window *)(glfwGetWindowUserPointer(window)); win->consume_cursor(xpos, ypos); });
            glfwSetScrollCallback(window, [](GLFWwindow * window, double deltaX, double deltaY) { auto win = (glfw_window *)(glfwGetWindowUserPointer(window)); win->consume_scroll(deltaX, deltaY); });
            glfwSetDropCallback(window, [](GLFWwindow * window, int count, const char * names[]) { auto win = (glfw_window *)(glfwGetWindowUserPointer(window)); win->on_drop({ names, names + count }); });
        }

        ~glfw_window()
        {
            if (window) glfwDestroyWindow(window);
        }

        int get_mods() const;
        virtual void on_update(const UpdateEvent & e) { }
        virtual void on_draw() { }
        virtual void on_window_focus(bool focused) { }
        virtual void on_window_resize(int2 size) { }
        virtual void on_input(const InputEvent & event) { }
        virtual void on_drop(std::vector<std::string> names) { }
        virtual void on_uncaught_exception(std::exception_ptr e)
        {
            rethrow_exception(e);
        }
    };
        
    class polymer_app : public glfw_window
    {
        uint64_t elapsedFrames{ 0 };
        double fps{ 0 };
        double fpsTime{ 0 };
        bool fullscreenState{ false };

        void take_screenshot_impl();
        std::string screenshotPath;
        static void enter_fullscreen(GLFWwindow * window, int2 & windowedSize, int2 & windowedPos);
        static void exit_fullscreen(GLFWwindow * window, const int2 & windowedSize, const int2 & windowedPos);
        int2 windowedSize, windowedPos;

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
