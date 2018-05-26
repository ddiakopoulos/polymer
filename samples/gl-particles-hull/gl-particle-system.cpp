#include "gl-particle-system.hpp"

using namespace polymer;

gl_particle_system::gl_particle_system(size_t trail_count) : trail(trail_count)
{
    const float2 quadCoords[] = { { 0,0 },{ 1,0 },{ 1,1 },{ 0,1 } };
    glNamedBufferDataEXT(vertexBuffer, sizeof(quadCoords), quadCoords, GL_STATIC_DRAW);
}

void gl_particle_system::add_modifier(std::unique_ptr<particle_modifier> modifier)
{
    particleModifiers.push_back(std::move(modifier));
}

void gl_particle_system::add(const float3 position, const float3 velocity, const float size, const float lifeMs)
{
    particle p;
    p.position = position;
    p.velocity = velocity;
    p.size = size;
    p.lifeMs = lifeMs;
    particles.push_back(std::move(p));
}

void gl_particle_system::update(const float dt, const float3 gravityVec)
{
    if (particles.size() == 0) return;

    for (auto & p : particles)
    {
        p.position += p.velocity * dt;
        p.lifeMs -= dt;
        p.isDead = p.lifeMs <= 0.f;
    }

    for (auto & modifier : particleModifiers)
    {
        modifier->update(particles, dt);
    }

    if (!particles.empty())
    {
        auto it = std::remove_if(std::begin(particles), std::end(particles), [](const particle & p)
        {
            return p.isDead;
        });
        particles.erase(it, std::end(particles));
    }

    instances.clear();

    for (auto & p : particles)
    {
        float3 position = p.position;
        float sz = p.size;

        // create a trail using instancing
        for (int i = 0; i < (trail + 1); ++i)
        {
            instances.push_back({ position, sz });
            position -= p.velocity * 0.001f;
            sz *= 0.9f;
        }
    }
    glNamedBufferDataEXT(instanceBuffer, instances.size() * sizeof(float4), instances.data(), GL_DYNAMIC_DRAW);
}

void gl_particle_system::draw(
    const float4x4 & viewMat, 
    const float4x4 & projMat, 
    gl_shader & shader, 
    gl_texture_2d & outerTex, 
    gl_texture_2d & innerTex, 
    const float time)
{
    if (instances.size() == 0) return;

    shader.bind();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // one-one, additive
    glDepthMask(GL_FALSE);

    shader.uniform("u_modelMatrix", Identity4x4);
    shader.uniform("u_viewMat", viewMat);
    shader.uniform("u_viewProjMat", mul(projMat, viewMat));
    shader.uniform("u_time", time);
    shader.texture("s_outerTex", 0, outerTex, GL_TEXTURE_2D);
    shader.texture("s_innerTex", 1, innerTex, GL_TEXTURE_2D);

    // Instance buffer contains position and size
    glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(float4), nullptr);
    glVertexAttribDivisor(0, 1);

    // Quad
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float2), nullptr);
    glVertexAttribDivisor(1, 0);

    glDrawArraysInstanced(GL_QUADS, 0, 4, (GLsizei)instances.size());

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    shader.unbind();
}