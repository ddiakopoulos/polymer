#include "gl-particle-system.hpp"

using namespace polymer;

gl_particle_system::gl_particle_system()
{
    const float2 triangle_coords[] = { { 0,0 },{ 1,0 },{ 0,1 },{ 0,1 }, {1, 0}, {1, 1} };
    glNamedBufferDataEXT(vertexBuffer, sizeof(triangle_coords), triangle_coords, GL_STATIC_DRAW);
}

void gl_particle_system::set_trail_count(const size_t trail_count)
{
    trail = trail_count;
}

size_t gl_particle_system::get_trail_count() const
{
    return trail;
}

void gl_particle_system::set_particle_texture(gl_texture_2d && tex)
{
    particle_tex = std::move(tex);
    glTextureParameteriEXT(particle_tex, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTextureParameteriEXT(particle_tex, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
}

void gl_particle_system::add_modifier(std::shared_ptr<particle_modifier> modifier)
{
    particleModifiers.push_back(modifier);
}

void gl_particle_system::add(const float3 & position, const float3 & velocity, const float size, const float lifeMs)
{
    particle p;
    p.position = position;
    p.velocity = velocity;
    p.size = size;
    p.lifeMs = lifeMs;
    particles.emplace_back(p);
}

void gl_particle_system::update(const float dt, const float3 & gravityVec)
{
    if (particles.size() == 0) return;

    elapsed_time_ms += dt;

    // Update simulation
    for (auto & p : particles)
    {
        p.position += p.velocity * dt;
        p.lifeMs -= dt;
        p.isDead = p.lifeMs <= 0.f;
    }

    // Apply modifiers
    for (auto & modifier : particleModifiers)
    {
        modifier->update(particles, dt);
    }

    // Dead particle culling
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
        float3 & position = p.position;
        float & sz = p.size;

        // Create instance particles, with an optional trail
        for (int i = 0; i < (trail + 1); ++i)
        {
            position -= p.velocity * 0.001f;
            sz *= 0.97f;

            instance_data instance;
            instance.position_size = float4(position, sz);
            instance.color = float4(p.color);
            instances.emplace_back(instance);
        }
    }

    glNamedBufferDataEXT(instanceBuffer, instances.size() * sizeof(instance_data), instances.data(), GL_DYNAMIC_DRAW);
}

void gl_particle_system::draw(
    const float4x4 & viewMat, 
    const float4x4 & projMat, 
    gl_shader & shader) const
{
    if (instances.size() == 0) return;

    shader.bind();

    const GLboolean wasBlendingEnabled = glIsEnabled(GL_BLEND);

    // GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA // Traditional transparency
    // GL_ONE, GL_ONE_MINUS_SRC_ALPHA       // Premultiplied transparency
    // GL_ONE, GL_ONE                       // Additive
    // GL_ONE_MINUS_DST_COLOR, GL_ONE       // Soft additive
    // GL_DST_COLOR, GL_ZERO                // Multiplicative
    // GL_DST_COLOR, GL_SRC_COLOR           // 2x Multiplicative

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
    glDepthMask(GL_FALSE);

    shader.uniform("u_modelMatrix", Identity4x4);
    shader.uniform("u_inverseViewMatrix", inverse(viewMat));
    shader.uniform("u_viewProjMat", projMat * viewMat);
    shader.uniform("u_time", elapsed_time_ms);
    shader.texture("s_particleTex", 0, particle_tex, GL_TEXTURE_2D);

    glBindVertexArray(vao);

    // Instance buffer contains position (xyz) and size/radius (w)
    // An attribute is referred to as instanced if its GL_VERTEX_ATTRIB_ARRAY_DIVISOR value is non-zero. 
    glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(instance_data), (GLvoid*)offsetof(instance_data, position_size));
    glVertexAttribDivisor(0, 1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(instance_data), (GLvoid*)offsetof(instance_data, color));
    glVertexAttribDivisor(1, 1); 

    // Draw quad with texcoords
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(float2), nullptr);
    glVertexAttribDivisor(2, 0); // If divisor is zero, the attribute at slot index advances once per vertex

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (GLsizei)instances.size());
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    if (wasBlendingEnabled) glEnable(GL_BLEND);
    glDepthMask(GL_TRUE);

    shader.unbind();

    gl_check_error(__FILE__, __LINE__);
}