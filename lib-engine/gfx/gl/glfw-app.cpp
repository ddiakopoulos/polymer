#include "glfw-app.hpp"
#include "util.hpp"
#include "math-spatial.hpp"
#include "gl-api.hpp"
#include "stb/stb_image_write.h"
#include "human_time.hpp"

using namespace polymer;

static app_input_event generate_input_event(GLFWwindow * window, app_input_event::Type type, const float2 & cursor, int action)
{
    app_input_event e;
    e.window = window;
    e.type = type;
    e.cursor = cursor;
    e.action = action;
    e.mods = 0;

    glfwGetWindowSize(window, &e.windowSize.x, &e.windowSize.y);

    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) | glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT)) e.mods |= GLFW_MOD_SHIFT;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) | glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL)) e.mods |= GLFW_MOD_CONTROL;
    if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) | glfwGetKey(window, GLFW_KEY_RIGHT_ALT)) e.mods |= GLFW_MOD_ALT;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SUPER) | glfwGetKey(window, GLFW_KEY_RIGHT_SUPER)) e.mods |= GLFW_MOD_SUPER;

    return e;
}

float2 get_cursor_position(GLFWwindow * win)
{
    double xpos, ypos;
    glfwGetCursorPos(win, &xpos, &ypos);
    return { (float)xpos, (float)ypos };
}

////////////////////
//   gl_context   //
////////////////////

gl_context::gl_context()
{
    if (!glfwInit()) throw std::runtime_error("could not initialize glfw...");

    glfwWindowHint(GLFW_VISIBLE, 0);
    hidden_window = glfwCreateWindow(1, 1, "hidden-window", nullptr, nullptr);
    if (!hidden_window) throw std::runtime_error("glfwCreateWindow(...) failed");
    glfwMakeContextCurrent(hidden_window);

    POLYMER_INFO("GL_VERSION =  " << (char *)glGetString(GL_VERSION));
    POLYMER_INFO("GL_SHADING_LANGUAGE_VERSION =  " << (char *)glGetString(GL_SHADING_LANGUAGE_VERSION));
    POLYMER_INFO("GL_VENDOR =   " << (char *)glGetString(GL_VENDOR));
    POLYMER_INFO("GL_RENDERER = " << (char *)glGetString(GL_RENDERER));
    POLYMER_INFO("GLFW_VERSION = " << glfwGetVersionString());

#if defined(POLYMER_PLATFORM_WINDOWS)
    glewExperimental = GL_TRUE;
    if (GLenum err = glewInit())
        throw std::runtime_error(std::string("glewInit() failed - ") + (const char *)glewGetErrorString(err));
    POLYMER_INFO("GLEW_VERSION = " << (char *)glewGetString(GLEW_VERSION));
#endif

    std::vector<std::pair<std::string, bool>> extensions{
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

gl_context::~gl_context()
{
    if (hidden_window) glfwDestroyWindow(hidden_window);
    glfwTerminate();
}

/////////////////////
//   glfw_window   //
/////////////////////

static glfw_window & get(GLFWwindow * window) { return *reinterpret_cast<glfw_window *>(glfwGetWindowUserPointer(window)); }

glfw_window::glfw_window(gl_context * context, int w, int h, const std::string title, int samples)
{
    gl_ctx = context;

    glfwWindowHint(GLFW_VISIBLE, 1);
    glfwWindowHint(GLFW_SAMPLES, samples);
    glfwWindowHint(GLFW_SRGB_CAPABLE, true);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_FALSE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glfwSetErrorCallback([](int err, const char* desc) {
        printf("glfw error - %i, desc: %s\n", err, desc);
    });

    window = glfwCreateWindow(w, h, title.c_str(), nullptr, gl_ctx->hidden_window);
    if (!window) throw std::runtime_error("failed to open glfw window: " + title);

    glfwMakeContextCurrent(window);

#ifdef _DEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
    glDebugMessageCallback(&gl_debug_callback, nullptr);
#endif

    glfwSetWindowUserPointer(window, this);
    glfwSetWindowFocusCallback(window, [](GLFWwindow * window, int focused) { get(window).on_window_focus(!!focused); });
    glfwSetWindowSizeCallback(window, [](GLFWwindow * window, int width, int height) {get(window).on_window_resize({ width, height }); });
    glfwSetCharCallback(window, [](GLFWwindow * window, unsigned int codepoint) { get(window).consume_character(codepoint);  });
    glfwSetKeyCallback(window, [](GLFWwindow * window, int key, int, int action, int) { get(window).consume_key(key, action); });
    glfwSetMouseButtonCallback(window, [](GLFWwindow * window, int button, int action, int) { get(window).consume_mousebtn(button, action);  });
    glfwSetCursorPosCallback(window, [](GLFWwindow * window, double xpos, double ypos) { get(window).consume_cursor(xpos, ypos); });
    glfwSetScrollCallback(window, [](GLFWwindow * window, double deltaX, double deltaY) { get(window).consume_scroll(deltaX, deltaY); });
    glfwSetDropCallback(window, [](GLFWwindow * window, int count, const char * names[]) { get(window).on_drop({ names, names + count }); });
    glfwSetWindowCloseCallback(window, [](GLFWwindow * window) { get(window).on_window_close(); });
    
}

glfw_window::~glfw_window()
{
    if (window) glfwDestroyWindow(window);
}

void glfw_window::preprocess_input(app_input_event & event)
{
    if (event.type == app_input_event::MOUSE)
    {
        if (event.is_down()) isDragging = true;
        else if (event.is_up()) isDragging = false;
    }
    event.drag = isDragging;
    on_input(event);
}

void glfw_window::consume_character(uint32_t codepoint)
{
    auto e = generate_input_event(window, app_input_event::CHAR, get_cursor_position(window), 0);
    e.value[0] = codepoint;
    preprocess_input(e);
}

void glfw_window::consume_key(int key, int action)
{
    auto e = generate_input_event(window, app_input_event::KEY, get_cursor_position(window), action);
    e.value[0] = key;
    preprocess_input(e);
}

void glfw_window::consume_mousebtn(int button, int action)
{
    auto e = generate_input_event(window, app_input_event::MOUSE, get_cursor_position(window), action);
    e.value[0] = button;
    preprocess_input(e);
}

void glfw_window::consume_cursor(double xpos, double ypos)
{
    auto e = generate_input_event(window, app_input_event::CURSOR, { (float)xpos, (float)ypos }, 0);
    preprocess_input(e);
}

void glfw_window::consume_scroll(double deltaX, double deltaY)
{
    auto e = generate_input_event(window, app_input_event::SCROLL, get_cursor_position(window), 0);
    e.value[0] = (float)deltaX;
    e.value[1] = (float)deltaY;
    preprocess_input(e);
}

int glfw_window::get_mods() const
{
    int mods = 0;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) || glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL)) mods |= GLFW_MOD_CONTROL;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT)) mods |= GLFW_MOD_SHIFT;
    if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) || glfwGetKey(window, GLFW_KEY_RIGHT_ALT)) mods |= GLFW_MOD_ALT;
    return mods;
}

/////////////////////
//   polymer_app   //
/////////////////////

// fixme -- technically gl_context is leaky
polymer_app::polymer_app(int w, int h, const std::string title, int samples) : glfw_window(new gl_context(), w, h, title, samples)
{

}

polymer_app::~polymer_app() 
{

}

void polymer_app::request_screenshot(const std::string & filename)
{
    screenshotPath = filename;
}

void polymer_app::screenshot_impl()
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    int2 size(width, height);
    HumanTime t;
    auto timestamp = t.make_timestamp();
    std::vector<uint8_t> screenShot(size.x * size.y * 4);
    glReadPixels(0, 0, size.x, size.y, GL_RGBA, GL_UNSIGNED_BYTE, screenShot.data());
    auto flipped = screenShot;
    for (int y = 0; y < size.y; ++y) memcpy(flipped.data() + y * size.x * 4, screenShot.data() + (size.y - y - 1)*size.x * 4, size.x * 4);
    stbi_write_png(std::string(screenshotPath + "-" + timestamp + ".png").c_str(), size.x, size.y, 4, flipped.data(), 4 * size.x);
    screenshotPath.clear();
}

void polymer_app::main_loop() 
{
    auto t0 = std::chrono::high_resolution_clock::now();
    
    while (!glfwWindowShouldClose(window)) 
    {
        try
        {
            glfwPollEvents();

            auto t1 = std::chrono::high_resolution_clock::now();
            auto timestep = std::chrono::duration<float>(t1 - t0).count();
            t0 = t1;
            
            elapsedFrames++;
            fpsTime += timestep;

            if (fpsTime > 0.5f)
            {
                fps = elapsedFrames / fpsTime;
                elapsedFrames = 0;
                fpsTime = 0;
            }

            app_update_event e;
            e.elapsed_s = glfwGetTime();
            e.timestep_ms = timestep;
            e.framesPerSecond = fps;
            e.elapsedFrames = elapsedFrames;

            on_update(e);
            on_draw();

            if (screenshotPath.size() > 0) screenshot_impl();
        }
        catch(...)
        {
            // ...
        }
    }
}

void polymer_app::exit()
{
    glfwMakeContextCurrent(window);
    glfwSetWindowShouldClose(window, 1);
}

namespace polymer
{
    int main(int argc, char * argv[]) 
    {
        int returnCode = EXIT_FAILURE;
        try { returnCode = main(argc, argv); }
        catch (const std::exception & e) { POLYMER_ERROR("[Fatal] Caught exception: \n" << e.what()); }
        return returnCode;
    }
}

int get_current_monitor(GLFWwindow * window)
{
    int numberOfMonitors;
    GLFWmonitor** monitors = glfwGetMonitors(&numberOfMonitors);
    
    int xW, yW;
    glfwGetWindowPos(window, &xW, &yW);
    
    for (int iC = 0; iC < numberOfMonitors; iC++)
    {
        int xM, yM;
        glfwGetMonitorPos(monitors[iC], &xM, &yM);
        const GLFWvidmode * desktopMode = glfwGetVideoMode(monitors[iC]);
        aabb_2d monitorRect {(float) xM, (float) yM, (float)xM + desktopMode->width, (float)yM + desktopMode->height};
        if (monitorRect.contains((float) xW, (float) yW))
            return iC;
    }
    return 0;
}

int2 get_screen_size(GLFWwindow * window)
{
    int numMonitors;
    GLFWmonitor** monitors = glfwGetMonitors(&numMonitors);
    if (numMonitors > 0)
    {
        int currentMonitor = get_current_monitor(window);
        const GLFWvidmode * desktopMode = glfwGetVideoMode(monitors[currentMonitor]);
        if (desktopMode)
            return {desktopMode->width, desktopMode->height};
        else
            return {0, 0};
    }
    return {0, 0};
}

#if defined (POLYMER_PLATFORM_WINDOWS)

#undef APIENTRY
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#include <GLFW/glfw3native.h>
#include <shellapi.h>

void polymer_app::enter_fullscreen(GLFWwindow * window, int2 & windowedSize, int2 & windowedPos)
{
    glfwGetWindowSize(window, &windowedSize.x, &windowedSize.y);
    glfwGetWindowPos(window, &windowedPos.x, &windowedPos.y);

    HWND hwnd = glfwGetWin32Window(window);
    SetWindowLong(hwnd, GWL_EXSTYLE, 0);
    SetWindowLong(hwnd, GWL_STYLE, WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    int32_t newScreenWidth = get_screen_size(window).x;
    int32_t newScreenHeight = get_screen_size(window).y;

    int monitorCount;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    int currentMonitor = get_current_monitor(window);
    int xpos, ypos;
    glfwGetMonitorPos(monitors[currentMonitor], &xpos, &ypos);

    SetWindowPos(hwnd, HWND_TOPMOST, xpos, ypos, (int) newScreenWidth, (int) newScreenHeight, SWP_SHOWWINDOW);
}

void polymer_app::exit_fullscreen(GLFWwindow * window, const int2 & windowedSize, const int2 & windowedPos)
{
    HWND hwnd = glfwGetWin32Window(window);
    DWORD EX_STYLE = WS_EX_OVERLAPPEDWINDOW;
    DWORD STYLE = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_SIZEBOX;
        
    ChangeDisplaySettings(0, 0);
    SetWindowLong(hwnd, GWL_EXSTYLE, EX_STYLE);
    SetWindowLong(hwnd, GWL_STYLE, STYLE);
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    // glfw/os bug: window shrinks by 4 pixels in x and y
    glfwSetWindowPos(window, windowedPos.x - 2, windowedPos.y - 2);
    glfwSetWindowSize(window, windowedSize.x + 4, windowedSize.y + 4);
}

#endif // end platform specific