#pragma once

#ifndef async_pbo_hpp
#define async_pbo_hpp

#include "gl-api.hpp"
#include "math-common.hpp"

struct AsyncRead1
{
    GLuint pbo[2];
    int idx{ 0 };

    AsyncRead1()
    {
        glGenBuffers(2, pbo);

        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[0]);
        glBufferData(GL_PIXEL_PACK_BUFFER, 16, NULL, GL_STREAM_DRAW); // 16 bytes

        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[1]);
        glBufferData(GL_PIXEL_PACK_BUFFER, 16, NULL, GL_STREAM_DRAW); // 16 bytes

        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    }

    ~AsyncRead1()
    {
        glDeleteBuffers(1, &pbo[0]);
        glDeleteBuffers(1, &pbo[1]);
    }

    float4 download()
    {
        int current = idx, next = 1 - idx;

        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[current]);
        glReadPixels(0, 0, 1, 1, GL_BGRA, GL_FLOAT, NULL);

        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[next]);
        GLubyte * v = (GLubyte*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);

        float4 result;

        if (v != NULL)
        {
            std::memcpy(&result.x, v, 16);
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        }

        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

        idx = !idx;

        return result;
    }
};

#endif // end async_pbo_hpp