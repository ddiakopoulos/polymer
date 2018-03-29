#pragma once

#ifndef bloom_pass_hpp
#define bloom_pass_hpp

#include "util.hpp"
#include "math-common.hpp"
#include "gl-api.hpp"
#include "gl-async-pbo.hpp"
#include "gl-imgui.hpp"
#include "file_io.hpp"
#include "procedural_mesh.hpp"

using namespace polymer;

struct BloomPass
{
    union
    {
        GLuint pipelines[1];
        struct { GLuint downsample_pipeline; };
    };


    GlShader hdr_post;
    GlShader hdr_lumShader;
    GlShader hdr_avgLumShader;
    GlShader hdr_blurShader;
    GlShader hdr_brightShader;

    GlShaderHandle hdr_tonemapShader = { "post-tonemap" };

    GlFramebuffer brightFramebuffer, blurFramebuffer, outputFramebuffer;
    GlFramebuffer luminance[5];

    GlTexture2D brightTex, blurPasses[2], outputTex; 
    GlTexture2D luminanceTex[5];

    GlMesh fsQuad;

    float2 perEyeSize;

    int blurPixelsPerSide = 2;
    float blurSigma = 4.0f;
    float middleGrey = 1.0f;
    float whitePoint = 1.5f;
    float threshold = 0.66f;
    float exposure = 0.5f;

    float blurDownsampleFactor = 2.0f;

    AsyncRead1 avgLuminance;

    BloomPass(float2 size) : perEyeSize(size)
    {
        fsQuad = make_fullscreen_quad();

        luminanceTex[0].setup(128, 128, GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        luminanceTex[1].setup(64, 64, GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        luminanceTex[2].setup(16, 16, GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        luminanceTex[3].setup(4, 4, GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        luminanceTex[4].setup(1, 1, GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        brightTex.setup(perEyeSize.x / 2, perEyeSize.y / 2, GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        blurPasses[0].setup(perEyeSize.x / blurDownsampleFactor, perEyeSize.y / blurDownsampleFactor, GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        blurPasses[1].setup(perEyeSize.x / blurDownsampleFactor, perEyeSize.y / blurDownsampleFactor, GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        outputTex.setup(perEyeSize.x, perEyeSize.y, GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);

        glNamedFramebufferTexture2DEXT(luminance[0], GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, luminanceTex[0], 0);
        glNamedFramebufferTexture2DEXT(luminance[1], GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, luminanceTex[1], 0);
        glNamedFramebufferTexture2DEXT(luminance[2], GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, luminanceTex[2], 0);
        glNamedFramebufferTexture2DEXT(luminance[3], GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, luminanceTex[3], 0);
        glNamedFramebufferTexture2DEXT(luminance[4], GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, luminanceTex[4], 0);
        glNamedFramebufferTexture2DEXT(brightFramebuffer, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brightTex, 0);
        glNamedFramebufferTexture2DEXT(blurFramebuffer, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, blurPasses[0], 0);
        glNamedFramebufferTexture2DEXT(blurFramebuffer, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, blurPasses[1], 0);
        glNamedFramebufferTexture2DEXT(outputFramebuffer, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outputTex, 0);

        for (auto & l : luminance) l.check_complete();
        brightFramebuffer.check_complete();
        blurFramebuffer.check_complete();
        outputFramebuffer.check_complete();

        hdr_post = GlShader(GL_VERTEX_SHADER, read_file_text("../assets/shaders/renderer/post_vert.glsl"));
        hdr_avgLumShader = GlShader(GL_FRAGMENT_SHADER, read_file_text("../assets/shaders/renderer/post_lumavg_frag.glsl"));

        hdr_lumShader = GlShader(read_file_text("../assets/shaders/renderer/post_vert.glsl"), read_file_text("../assets/shaders/renderer/post_lum_frag.glsl"));
        hdr_blurShader = GlShader(read_file_text("../assets/shaders/renderer/gaussian_blur_vert.glsl"), read_file_text("../assets/shaders/renderer/gaussian_blur_frag.glsl"));
        hdr_brightShader = GlShader(read_file_text("../assets/shaders/renderer/post_vert.glsl"), read_file_text("../assets/shaders/renderer/post_bright_frag.glsl"));

        glCreateProgramPipelines(GLsizei(1), pipelines);
        glBindProgramPipeline(downsample_pipeline);
        glUseProgramStages(downsample_pipeline, GL_VERTEX_SHADER_BIT, hdr_post.handle());
        glUseProgramStages(downsample_pipeline, GL_FRAGMENT_SHADER_BIT, hdr_avgLumShader.handle());

        gl_check_error(__FILE__, __LINE__);
    }

    ~BloomPass()
    {
        glDeleteProgramPipelines(GLsizei(1), pipelines);
    }

    void execute(GlTexture2D & sceneColorTex)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, luminance[0]); // 128x128 surface area - calculate luminance 
        glViewport(0, 0, 128, 128);
        hdr_lumShader.bind();
        hdr_lumShader.texture("s_texColor", 0, sceneColorTex, GL_TEXTURE_2D);
        hdr_lumShader.uniform("u_modelViewProj", Identity4x4);
        fsQuad.draw_elements();

        {
            glBindProgramPipeline(downsample_pipeline);

            std::vector<float3> downsampleTargets = { { 0, 64, 64 },{ 1, 16, 16 },{ 2, 4, 4 },{ 3, 1, 1 } };

            auto downsample = [&](const int idx, const float2 targetSize)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, luminance[idx + 1]);
                glViewport(0, 0, targetSize.x, targetSize.y);
                hdr_avgLumShader.texture("s_texColor", 0, luminance[idx], GL_TEXTURE_2D); // not bound?
                fsQuad.draw_elements();
            };

            for (auto target : downsampleTargets) downsample(target.x, float2(target.y, target.z));

            glBindProgramPipeline(0);

            hdr_avgLumShader.unbind();
        }

        hdr_lumShader.unbind();

        // Readback luminance value
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, luminance[4]);
        float4 lumValue = avgLuminance.download();
        glBindTexture(GL_TEXTURE_2D, 0);

        float4 tonemap = { middleGrey, whitePoint * whitePoint, threshold, 0.0f };

        /*
        const float lumTarget = 0.4f;
        const float exposureTarget = lumValue.x += 0.1 * (lumTarget - lumValue.x);
        float exposureCtrl = 0.86f;
        exposureCtrl = exposureCtrl * 0.1 + exposureTarget * 0.9;
        const float exposure = std::exp(exposureCtrl*exposureCtrl) - 1.0f;
        ImGui::Text("Exposure %f", exposure);
        ImGui::Text("Luminance %f", lumValue.x);
        */

        glBindFramebuffer(GL_FRAMEBUFFER, brightFramebuffer);
        glViewport(0, 0, perEyeSize.x / 2, perEyeSize.y / 2);
        hdr_brightShader.bind();
        hdr_brightShader.texture("s_texColor", 0, sceneColorTex, GL_TEXTURE_2D);
        hdr_brightShader.uniform("u_exposure", exposure);
        hdr_brightShader.uniform("u_tonemap", tonemap);
        hdr_brightShader.uniform("u_modelViewProj", Identity4x4);
        fsQuad.draw_elements();
        hdr_brightShader.unbind();

        static int dx = 0;

        {
            glBindFramebuffer(GL_FRAMEBUFFER, blurFramebuffer);

            if (dx == 0)
            {
                glDrawBuffer(GL_COLOR_ATTACHMENT1);
                glReadBuffer(GL_COLOR_ATTACHMENT0);
            }
            else
            {
                glDrawBuffer(GL_COLOR_ATTACHMENT0);
                glReadBuffer(GL_COLOR_ATTACHMENT1);
            }

            glViewport(0, 0, perEyeSize.x / blurDownsampleFactor, perEyeSize.y / blurDownsampleFactor);

            hdr_blurShader.bind();

            hdr_blurShader.uniform("u_modelViewProj", Identity4x4);
            hdr_blurShader.uniform("sigma", blurSigma);
            hdr_blurShader.uniform("numBlurPixelsPerSide", float(blurPixelsPerSide));

            // Horizontal pass
            hdr_blurShader.uniform("blurSize", 1.f / (perEyeSize.x / blurDownsampleFactor));
            hdr_blurShader.uniform("blurMultiplyVec", float2(1.0f, 0.0f));
            hdr_blurShader.texture("s_blurTexure", 0, brightTex, GL_TEXTURE_2D);
            fsQuad.draw_elements();

            // Vertical pass
            hdr_blurShader.uniform("blurSize", 1.f / (perEyeSize.y / blurDownsampleFactor));
            hdr_blurShader.uniform("blurMultiplyVec", float2(0.0f, 1.0f));
            hdr_blurShader.texture("s_blurTexure", 0, blurPasses[1 - dx], GL_TEXTURE_2D);
            fsQuad.draw_elements();

            dx = 1 - dx; // swap

            hdr_blurShader.unbind();
        }

        glBindFramebuffer(GL_FRAMEBUFFER, outputFramebuffer);
        glViewport(0, 0, perEyeSize.x, perEyeSize.y);
        auto & tonemapProgram = hdr_tonemapShader.get();
        tonemapProgram.bind();
        tonemapProgram.texture("s_texColor", 0, sceneColorTex, GL_TEXTURE_2D);
        tonemapProgram.texture("s_bloom", 1, blurPasses[dx], GL_TEXTURE_2D);
        fsQuad.draw_elements();
        tonemapProgram.unbind();
    }

    GLuint get_output_framebuffer() const { return outputFramebuffer.id(); }

    GLuint get_luminance_texture() const { return luminanceTex[0].id(); }

    GLuint get_bright_tex() const { return brightTex.id(); }

    GLuint get_blur_tex() const { return blurPasses[0].id(); }
};

template<class F> void visit_fields(BloomPass & o, F f)
{
    f("blur_radius", o.blurPixelsPerSide, range_metadata<int>{ 2, 6 });
    f("blur_sigma", o.blurSigma, range_metadata<float>{ 0.1f, 8.f});
    f("middle_grey", o.middleGrey, range_metadata<float>{ 0.1f, 1.0});
    f("whitepoint", o.whitePoint, range_metadata<float>{ 0.1f, 2.f});
    f("threshold", o.threshold, range_metadata<float>{ 0.f, 2.f});
    f("exposure", o.exposure, range_metadata<float>{ 0.f, 2.f});
}

#endif // end bloom_pass_hpp
