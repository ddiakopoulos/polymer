#if defined(POLYMER_PLATFORM_WINDOWS)
#pragma warning(push)
#pragma warning(disable : 4244)
#endif

#include <functional>
#include <memory>
#include "gl-imgui.hpp"
#include "imgui/imgui_internal.h"
#include "gl-api.hpp"
#include "glfw-app.hpp"

using namespace polymer;

namespace gui
{
    ////////////////////////////////
    //   Wrapper Implementation   //
    ////////////////////////////////

    imgui_instance::imgui_instance(GLFWwindow * win, bool use_default_font)
    {
        data.window = win;
        data.context = ImGui::CreateContext();
        
        ImGui::SetCurrentContext(data.context);

        ImGuiIO & io = ImGui::GetIO();

        io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
        io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
        io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
        io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
        io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
        io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
        io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
        io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
        io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
        io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
        io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
        io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
        io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
        io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
        io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
        io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
        io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
        io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
        io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;
        
        if (use_default_font)
        {
            io.Fonts->AddFontDefault();
        }
    }

    std::vector<uint8_t> fontBuffer;
    void imgui_instance::add_font(const std::vector<uint8_t> & buffer)
    {
        ImGuiIO & io = ImGui::GetIO();
        fontBuffer = buffer;
        ImFontConfig config;
        config.FontDataOwnedByAtlas = false;
        auto font = io.Fonts->AddFontFromMemoryTTF((void *)fontBuffer.data(), (int) fontBuffer.size(), 15.f, &config);
        IM_ASSERT(font != NULL);
    }

    std::vector<uint8_t> iconFontBuffer;
    void imgui_instance::append_icon_font(const std::vector<uint8_t> & buffer)
    {
        ImGuiIO & io = ImGui::GetIO();
        iconFontBuffer = buffer;
        static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
        ImFontConfig icons_config; 
        icons_config.MergeMode = true; 
        icons_config.PixelSnapH = true;
        icons_config.FontDataOwnedByAtlas = false;
        auto font = io.Fonts->AddFontFromMemoryTTF((void *)iconFontBuffer.data(), (int)iconFontBuffer.size(), 15.f, &icons_config, icons_ranges);
        IM_ASSERT(font != NULL);
    }

    imgui_instance::~imgui_instance()
    {
        ImGui::SetCurrentContext(data.context);
        destroy_render_objects();
        ImGui::Shutdown(data.context);
    }
    
    void imgui_instance::update_input(const polymer::app_input_event & e)
    {
        ImGui::SetCurrentContext(data.context);

        ImGuiIO & io = ImGui::GetIO();
        
        if (e.type == app_input_event::Type::MOUSE)
        {
            if (e.action == GLFW_PRESS)
            {
                int button = clamp<int>(e.value[0], 0, 3);
                data.MousePressed[button] = true;
            }
            io.MousePos = ImVec2(e.cursor.x, e.cursor.y);
        }

        if (e.type == app_input_event::Type::CURSOR)
        {
            io.MousePos = ImVec2(e.cursor.x, e.cursor.y);
        }

        if (e.type == app_input_event::Type::SCROLL)
        {
            data.MouseWheel += (float)e.value[1]; // Use fractional mouse wheel, 1.0 unit 5 lines.
        }
        
        if (e.type == app_input_event::Type::KEY)
        {
            io.KeysDown[e.value[0]] = (e.action == GLFW_PRESS);
            io.KeyCtrl = (e.mods & GLFW_MOD_CONTROL) != 0;
            io.KeyShift = (e.mods & GLFW_MOD_SHIFT) != 0;
            io.KeyAlt = (e.mods & GLFW_MOD_ALT) != 0;
        }
        
        if (e.type == app_input_event::Type::CHAR)
        {
            if (e.value[0] > 0 && e.value[0] < 0x10000)
            {
                io.AddInputCharacter((unsigned short) e.value[0]);
            }
        }

    }

    bool imgui_instance::create_fonts_texture()
    {
        ImGui::SetCurrentContext(data.context);

        // Build texture atlas
        ImGuiIO & io = ImGui::GetIO();
        unsigned char* pixels;
        int width, height;

        // Load as RGBA 32-bits for OpenGL3 demo because it is more likely to be compatible with user's existing shader.
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        
        // Upload texture to graphics system
        GLint last_texture;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
        glGenTextures(1, &data.FontTexture);
        glBindTexture(GL_TEXTURE_2D, data.FontTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        
        // Store our identifier
        io.Fonts->TexID = (void *)(intptr_t)data.FontTexture;
        
        // Restore state
        glBindTexture(GL_TEXTURE_2D, last_texture);
        
        return true;
    }
    
    bool imgui_instance::create_render_objects()
    {
        ImGui::SetCurrentContext(data.context);

        // Backup GL state
        GLint last_texture, last_array_buffer, last_vertex_array;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);
        
        const GLchar *vertex_shader =
        "#version 330\n"
        "uniform mat4 ProjMtx;\n"
        "in vec2 Position;\n"
        "in vec2 UV;\n"
        "in vec4 Color;\n"
        "out vec2 Frag_UV;\n"
        "out vec4 Frag_Color;\n"
        "void main()\n"
        "{\n"
        "	Frag_UV = UV;\n"
        "	Frag_Color = Color;\n"
        "	gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
        "}\n";
        
        const GLchar* fragment_shader =
        "#version 330\n"
        "uniform sampler2D Texture;\n"
        "in vec2 Frag_UV;\n"
        "in vec4 Frag_Color;\n"
        "out vec4 Out_Color;\n"
        "void main()\n"
        "{\n"
        "	Out_Color = Frag_Color * texture( Texture, Frag_UV.st);\n"
        "}\n";
        
        data.ShaderHandle = glCreateProgram();
        data.VertHandle = glCreateShader(GL_VERTEX_SHADER);
        data.FragHandle = glCreateShader(GL_FRAGMENT_SHADER);
        
        glShaderSource(data.VertHandle, 1, &vertex_shader, 0);
        glShaderSource(data.FragHandle, 1, &fragment_shader, 0);
        glCompileShader(data.VertHandle);
        glCompileShader(data.FragHandle);
        glAttachShader(data.ShaderHandle, data.VertHandle);
        glAttachShader(data.ShaderHandle, data.FragHandle);
        glLinkProgram(data.ShaderHandle);
        
        data.AttribLocationTex = glGetUniformLocation(data.ShaderHandle, "Texture");
        data.AttribLocationProjMtx = glGetUniformLocation(data.ShaderHandle, "ProjMtx");
        data.AttribLocationPosition = glGetAttribLocation(data.ShaderHandle, "Position");
        data.AttribLocationUV = glGetAttribLocation(data.ShaderHandle, "UV");
        data.AttribLocationColor = glGetAttribLocation(data.ShaderHandle, "Color");
        
        glGenBuffers(1, &data.VboHandle);
        glGenBuffers(1, &data.ElementsHandle);
        
        glGenVertexArrays(1, &data.VaoHandle);
        glBindVertexArray(data.VaoHandle);
        glBindBuffer(GL_ARRAY_BUFFER, data.VboHandle);
        glEnableVertexAttribArray(data.AttribLocationPosition);
        glEnableVertexAttribArray(data.AttribLocationUV);
        glEnableVertexAttribArray(data.AttribLocationColor);
        
        #define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))
        glVertexAttribPointer(data.AttribLocationPosition, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, pos));
        glVertexAttribPointer(data.AttribLocationUV, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, uv));
        glVertexAttribPointer(data.AttribLocationColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, col));
        #undef OFFSETOF
        
        create_fonts_texture();
        
        // Restore modified GL state
        glBindTexture(GL_TEXTURE_2D, last_texture);
        glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
        glBindVertexArray(last_vertex_array);
        
        return true;
    }
    
    void imgui_instance::destroy_render_objects()
    {
        ImGui::SetCurrentContext(data.context);

        if (data.VaoHandle) glDeleteVertexArrays(1, &data.VaoHandle);
        if (data.VboHandle) glDeleteBuffers(1, &data.VboHandle);
        if (data.ElementsHandle) glDeleteBuffers(1, &data.ElementsHandle);
        data.VaoHandle = data.VboHandle = data.ElementsHandle = 0;
        
        if (data.ShaderHandle && data.VertHandle) glDetachShader(data.ShaderHandle, data.VertHandle);
        if (data.VertHandle) glDeleteShader(data.VertHandle);
        data.VertHandle = 0;
        
        if (data.ShaderHandle && data.VertHandle) glDetachShader(data.ShaderHandle, data.FragHandle);
        if (data.FragHandle) glDeleteShader(data.FragHandle);
        data.FragHandle = 0;
        
        glDeleteProgram(data.ShaderHandle);
        data.ShaderHandle = 0;

        if (data.FontTexture)
        {
            glDeleteTextures(1, &data.FontTexture);
            ImGui::GetIO().Fonts->TexID = 0;
            data.FontTexture = 0;
        }
        gl_check_error(__FILE__, __LINE__);
    }
    
    void imgui_instance::begin_frame(const uint32_t width, const uint32_t height)
    {
        ImGui::SetCurrentContext(data.context);
        if (!data.FontTexture) create_render_objects();

        ImGuiIO & io = ImGui::GetIO();

        // Setup time step
        double current_time = glfwGetTime();
        io.DeltaTime = data.Time > 0.0 ? (float)(current_time - data.Time) : (float)(1.0f / 60.0f);
        data.Time = current_time;

        for (int i = 0; i < 3; i++)
        {
            // If a mouse press event came, always pass it as "mouse held this frame",
            // so we don't miss click-release events that are shorter than 1 frame
            io.MouseDown[i] = data.MousePressed[i] || glfwGetMouseButton(data.window, i) != 0;
            data.MousePressed[i] = false;
        }

        io.MouseWheel = data.MouseWheel;
        data.MouseWheel = 0.0f;

        // Hide OS mouse cursor if ImGui is drawing it
        glfwSetInputMode(data.window, GLFW_CURSOR, io.MouseDrawCursor ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL);

        // Don't muck with the state if minimized
        if (!glfwGetWindowAttrib(data.window, GLFW_ICONIFIED))
        {
            // Setup display size (every frame to accommodate for window resizing)
            int w = width;
            int h = height;
            int display_w = width;
            int display_h = height;

            // Only use glfw window size if we have not passed in a texture size that
            // we are rendering to 
            if (w == 0 || h == 0)
            {
                glfwGetWindowSize(data.window, &w, &h);
                glfwGetFramebufferSize(data.window, &display_w, &display_h);
            }

            io.DisplaySize = ImVec2((float)w, (float)h);
            io.DisplayFramebufferScale = ImVec2(w > 0 ? ((float)display_w / w) : 0, h > 0 ? ((float)display_h / h) : 0);
        }

        // Start the frame
        ImGui::NewFrame();
    }
    
    void imgui_instance::end_frame()
    {
        ImGui::SetCurrentContext(data.context);
        ImGui::Render();
        ImDrawData * drawData = ImGui::GetDrawData();

        // Backup GL state
        GLint last_program; glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
        GLint last_texture; glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
        GLint last_array_buffer; glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
        GLint last_element_array_buffer; glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_element_array_buffer);
        GLint last_vertex_array; glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);
        GLint last_blend_src; glGetIntegerv(GL_BLEND_SRC, &last_blend_src);
        GLint last_blend_dst; glGetIntegerv(GL_BLEND_DST, &last_blend_dst);
        GLint last_blend_equation_rgb; glGetIntegerv(GL_BLEND_EQUATION_RGB, &last_blend_equation_rgb);
        GLint last_blend_equation_alpha; glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &last_blend_equation_alpha);
        GLint last_viewport[4]; glGetIntegerv(GL_VIEWPORT, last_viewport);
        GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
        GLboolean last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
        GLboolean last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
        GLboolean last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);

        // Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_SCISSOR_TEST);
        glActiveTexture(GL_TEXTURE0);

        // Handle cases of screen coordinates != from framebuffer coordinates (e.g. retina displays)
        ImGuiIO & io = ImGui::GetIO();
        int fb_width = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
        int fb_height = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
        if (fb_width == 0 || fb_height == 0) return;
        drawData->ScaleClipRects(io.DisplayFramebufferScale);

        // Setup viewport, orthographic projection matrix
        glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height);
        const float ortho_projection[4][4] = {
            { 2.0f / io.DisplaySize.x, 0.0f,                        0.0f, 0.0f },
            { 0.0f,                    2.0f / -io.DisplaySize.y,    0.0f, 0.0f },
            { 0.0f,                    0.0f,                       -1.0f, 0.0f },
            {-1.0f,                    1.0f,                        0.0f, 1.0f },
        };

        glUseProgram(data.ShaderHandle);
        glUniform1i(data.AttribLocationTex, 0);
        glUniformMatrix4fv(data.AttribLocationProjMtx, 1, GL_FALSE, &ortho_projection[0][0]);
        glBindVertexArray(data.VaoHandle);

        for (int n = 0; n < drawData->CmdListsCount; n++)
        {
            const ImDrawList* cmd_list = drawData->CmdLists[n];
            const ImDrawIdx* idx_buffer_offset = 0;

            glBindBuffer(GL_ARRAY_BUFFER, data.VboHandle);
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)cmd_list->VtxBuffer.size() * sizeof(ImDrawVert), (GLvoid*)&cmd_list->VtxBuffer.front(), GL_STREAM_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ElementsHandle);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)cmd_list->IdxBuffer.size() * sizeof(ImDrawIdx), (GLvoid*)&cmd_list->IdxBuffer.front(), GL_STREAM_DRAW);

            for (const ImDrawCmd* pcmd = cmd_list->CmdBuffer.begin(); pcmd != cmd_list->CmdBuffer.end(); pcmd++)
            {
                if (pcmd->UserCallback)
                {
                    pcmd->UserCallback(cmd_list, pcmd);
                }
                else
                {
                    glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->TextureId);
                    glScissor((int)pcmd->ClipRect.x, (int)(fb_height - pcmd->ClipRect.w), (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
                    glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, idx_buffer_offset);
                }
                idx_buffer_offset += pcmd->ElemCount;
            }
        }

        // Restore modified GL state
        glUseProgram(last_program);
        glBindTexture(GL_TEXTURE_2D, last_texture);
        glBindVertexArray(last_vertex_array);
        glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_element_array_buffer);
        glBlendEquationSeparate(last_blend_equation_rgb, last_blend_equation_alpha);
        glBlendFunc(last_blend_src, last_blend_dst);
        if (last_enable_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        if (last_enable_cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
        if (last_enable_depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        if (last_enable_scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
        glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
    }


    ///////////////////////
    //   imgui_surface   //
    ///////////////////////

    imgui_surface::imgui_surface(const uint2 size, GLFWwindow * window) : framebufferSize(size)
    {
        imgui.reset(new gui::imgui_instance(window));
        renderTexture.setup(size.x, size.y, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glNamedFramebufferTexture2DEXT(renderFramebuffer, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderTexture, 0);
        renderFramebuffer.check_complete();
    }

    uint2 imgui_surface::get_size() const 
    { 
        return framebufferSize;
    }

    gui::imgui_instance * imgui_surface::get_instance() 
    {
        return imgui.get(); 
    }

    uint32_t imgui_surface::get_render_texture() const 
    {
        return renderTexture; 
    }

    void imgui_surface::begin_frame()
    {
        imgui->begin_frame(framebufferSize.x, framebufferSize.y);
    }

    void imgui_surface::end_frame()
    {
        renderFramebuffer.check_complete();

        // Save framebuffer state
        GLint last_viewport[4]; glGetIntegerv(GL_VIEWPORT, last_viewport);
        GLint drawFramebuffer = 0, readFramebuffer = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);


        glBindFramebuffer(GL_FRAMEBUFFER, renderFramebuffer);
        glViewport(0, 0, (GLsizei)framebufferSize.x, (GLsizei)framebufferSize.y);
        glClear(GL_COLOR_BUFFER_BIT);

        imgui->end_frame();

        // Restore framebuffer state
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebuffer);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
        glViewport((GLsizei)last_viewport[0], (GLsizei)last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
    }

    //////////////////////////////
    //   Helper Functionality   //
    //////////////////////////////

    void Texture(const int & texture, const char * label, const ImVec2 & size, const ImVec2 & uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col)
    {
        ImGui::Image((void *)(intptr_t)texture, size, uv0, uv1, tint_col, border_col);
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text(label);
            ImGui::EndTooltip();
        }
    }

    bool ImageButton(const int & texture, const ImVec2 & size, const ImVec2 & uv0, const ImVec2 & uv1, int frame_padding, const ImVec4 & bg_col, const ImVec4 & tint_col)
    {
        return ImGui::ImageButton((void *)(intptr_t) texture, size, uv0, uv1, frame_padding, bg_col, tint_col);
    }

    bool ListBox(const char* label, int * current_item, const std::vector<std::string> & items, int height_in_items)
    {
        std::vector<const char*> names;
        for (auto item : items) 
        {
            char *cname = new char[item.size() + 1];
            std::strcpy(cname, item.c_str());
            names.push_back(cname);
        }
        bool result = ImGui::ListBox(label, current_item, names.data(), (int) names.size(), height_in_items);
        for (auto &name : names) delete [] name;
        return result;
    }

    bool InputText(const char* label, std::string * buf, ImGuiInputTextFlags flags, ImGuiTextEditCallback callback, void * user_data)
    {
        char *buffer = new char[buf->size()+128];
        std::strcpy(buffer, buf->c_str());
        bool result = ImGui::InputText(label, buffer, buf->size()+128, flags, callback, user_data);
        if (result) *buf = std::string(buffer);
        delete [] buffer;
        return result;
    }

    bool InputTextMultiline(const char* label, std::string* buf, const ImVec2 & size, ImGuiInputTextFlags flags, ImGuiTextEditCallback callback, void * user_data)
    {
        char *buffer = new char[buf->size()+128];
        std::strcpy(buffer, buf->c_str());
        bool result = ImGui::InputTextMultiline(label, buffer, buf->size()+128, size, flags, callback, user_data);
        if (result) *buf = std::string(buffer);
        delete [] buffer;
        return result;
    }

    bool Combo(const char* label, int * current_item, const std::vector<std::string> & items, int height_in_items)
    {
        std::string itemsNames;
        for (auto item : items) itemsNames += item + '\0';
        itemsNames += '\0';
        std::vector<char> charArray(itemsNames.begin(), itemsNames.end());
        bool result = ImGui::Combo(label, current_item, (const char*) &charArray[0], height_in_items);
        return result;
    }

    ////////////////////
    //   Menu Stack   //
    ////////////////////

    imgui_menu_stack::imgui_menu_stack(const polymer_app & app, bool * keys) : current_mods(app.get_mods()), keys(keys) { }

    void imgui_menu_stack::app_menu_begin()
    {
        assert(open.empty());
        open.push_back(ImGui::BeginMainMenuBar());
    }

    void imgui_menu_stack::begin(const char * label, bool enabled)
    {
        open.push_back(open.back() ? ImGui::BeginMenu(label, true) : false);
    }

    bool imgui_menu_stack::item(const char * label, int mods, int key, bool enabled)
    {
        bool invoked = (key && mods == current_mods && keys[key]);
        if (open.back())
        {
            std::string shortcut;
            if (key)
            {
                if (mods & GLFW_MOD_CONTROL) shortcut += "Ctrl+";
                if (mods & GLFW_MOD_SHIFT) shortcut += "Shift+";
                if (mods & GLFW_MOD_ALT) shortcut += "Alt+";
                if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) shortcut += static_cast<char>('A' + (key - GLFW_KEY_A));
                else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) shortcut += static_cast<char>('0' + (key - GLFW_KEY_0));
                else if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F25) shortcut += 'F' + std::to_string(1 + (key - GLFW_KEY_F1));
                else if (key == GLFW_KEY_SPACE) shortcut += "Space";
                else if (key == GLFW_KEY_APOSTROPHE) shortcut += '\'';
                else if (key == GLFW_KEY_COMMA) shortcut += ',';
                else if (key == GLFW_KEY_MINUS) shortcut += '-';
                else if (key == GLFW_KEY_PERIOD) shortcut += '.';
                else if (key == GLFW_KEY_SLASH) shortcut += '/';
                else if (key == GLFW_KEY_SEMICOLON) shortcut += ';';
                else if (key == GLFW_KEY_EQUAL) shortcut += '=';
                else if (key == GLFW_KEY_LEFT_BRACKET) shortcut += '[';
                else if (key == GLFW_KEY_BACKSLASH) shortcut += '\\';
                else if (key == GLFW_KEY_RIGHT_BRACKET) shortcut += ']';
                else if (key == GLFW_KEY_GRAVE_ACCENT) shortcut += '`';
                else if (key == GLFW_KEY_ESCAPE) shortcut += "Escape";
                else if (key == GLFW_KEY_ENTER) shortcut += "Enter";
                else if (key == GLFW_KEY_TAB) shortcut += "Tab";
                else if (key == GLFW_KEY_BACKSPACE) shortcut += "Backspace";
                else if (key == GLFW_KEY_INSERT) shortcut += "Insert";
                else if (key == GLFW_KEY_DELETE) shortcut += "Delete";
                else if (key == GLFW_KEY_RIGHT) shortcut += "Right Arrow";
                else if (key == GLFW_KEY_LEFT) shortcut += "Left Arrow";
                else if (key == GLFW_KEY_DOWN) shortcut += "Down Arrow";
                else if (key == GLFW_KEY_UP) shortcut += "Up Arrow";
                else if (key == GLFW_KEY_PAGE_UP) shortcut += "Page Up";
                else if (key == GLFW_KEY_PAGE_DOWN) shortcut += "Page Down";
                else if (key == GLFW_KEY_HOME) shortcut += "Home";
                else if (key == GLFW_KEY_END) shortcut += "End";
                else if (key == GLFW_KEY_CAPS_LOCK) shortcut += "Caps Lock";
                else if (key == GLFW_KEY_SCROLL_LOCK) shortcut += "Scroll Lock";
                else if (key == GLFW_KEY_NUM_LOCK) shortcut += "Num Lock";
                else if (key == GLFW_KEY_PRINT_SCREEN) shortcut += "Print Screen";
                else if (key == GLFW_KEY_PAUSE) shortcut += "Pause";
                else assert(false && "bad shortcut key");
            }
            invoked |= ImGui::MenuItem(label, shortcut.c_str(), false, enabled);
        }
        return invoked;
    }

    void imgui_menu_stack::end()
    {
        if (open.back()) ImGui::EndMenu();
        open.pop_back();
    }

    void imgui_menu_stack::app_menu_end()
    {
        if (open.back()) ImGui::EndMainMenuBar();
        open.pop_back();
        assert(open.empty());
    }

} // end namespace gui
