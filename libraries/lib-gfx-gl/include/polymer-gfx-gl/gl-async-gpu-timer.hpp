#pragma once

#ifndef timer_gl_gpu_h
#define timer_gl_gpu_h

#include "polymer-gfx-gl/gl-api.hpp"
#include "polymer-core/util/util.hpp"

class gl_gpu_timer
{
    struct query_timer
    {
        GLuint query;
        bool active;
    };

    GLsync sync;
    size_t activeIdx;
    std::vector<query_timer> queries;

public:

    ~gl_gpu_timer()
    {
        for (size_t i = 0; i < queries.size(); ++i)
        {
            glDeleteQueries(1, &queries[i].query);
        }
    }

    void start()
    {
        activeIdx = queries.size();

        // Reuse inactive queries
        for (size_t i = 0; i < queries.size(); ++i)
        {
            if (queries[i].active == false)
            {
                activeIdx = i;
                queries[i].active = true;
                break;
            }
        }
        
        // Generate new query
        if (activeIdx == queries.size())
        {
            query_timer qt;
            glCreateQueries(GL_TIME_ELAPSED, 1, &qt.query);
            qt.active = true;
            queries.push_back(qt);
        }

        glBeginQuery(GL_TIME_ELAPSED, queries[activeIdx].query);
    }

    void stop()
    {
        glEndQuery(GL_TIME_ELAPSED);
    }

    double elapsed_ms()
    {
        double timer_elapsed = 0;
        for (size_t i = 0; i < queries.size(); ++i)
        {
            if (queries[i].active == true)
            {
                uint64_t elapsed = 0;
                glGetQueryObjectui64v(queries[i].query, GL_QUERY_RESULT_NO_WAIT, &elapsed);
                if (!elapsed)
                    continue;

                timer_elapsed = elapsed * 1E-6f; // convert into milliseconds
                queries[i].active = false;
            }
        }

        return timer_elapsed;
    }

};

#endif // end timer_gl_gpu_h
