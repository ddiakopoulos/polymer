#include "gl-particle-system.hpp"

using namespace polymer;

template<typename Fn>
void particle_parallel_for(int n, Fn function, int target_concurrency = 0)
{
	const int hint = (target_concurrency == 0) ? std::thread::hardware_concurrency() : target_concurrency;
	const int n_threads = std::min(n, (hint == 0) ? 4 : hint);
	const int n_max_tasks_per_thread = (n / n_threads) + (n % n_threads == 0 ? 0 : 1);
	const int n_lacking_tasks = n_max_tasks_per_thread * n_threads - n;

	auto inner_loop = [&](const int thread_index)
	{
		const int n_lacking_tasks_so_far = std::max(thread_index - n_threads + n_lacking_tasks, 0);
		const int inclusive_start_index = thread_index * n_max_tasks_per_thread - n_lacking_tasks_so_far;
		const int exclusive_end_index = inclusive_start_index + n_max_tasks_per_thread - (thread_index - n_threads + n_lacking_tasks >= 0 ? 1 : 0);

		for (int k = inclusive_start_index; k < exclusive_end_index; ++k)
		{
			function(k);
		}
	};
	std::vector<std::thread> threads;
	for (int j = 0; j < n_threads; ++j) { threads.push_back(std::thread(inner_loop, j)); }
	for (auto & t : threads) { t.join(); }
}

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
    use_alpha_mask_texture = true;
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

void gl_particle_system::add(const float3 & position, const float4 & color, const float size)
{
	particle p;
	p.position = position;
	p.color = color;
	p.size = size;
	particles.emplace_back(p);
}

void gl_particle_system::clear()
{
	particles.clear();
}

void gl_particle_system::update(const float dt)
{
	if (particles.size() == 0) return;

	//scoped_timer t("gl_particle_system::overall");

    // put into better place
	if (!instances.size())
	{
		instanceBuffers.reset(new ping_pong_buffer<gl_buffer>(16384)); // max?
	}

	{
		//scoped_timer t("gl_particle_system::simulate");

        // Simulate 
        for (int i = 0; i < particles.size(); ++i)
        {
            particles[i].position += particles[i].velocity * dt;
            particles[i].lifeMs -= dt;
            particles[i].isDead = particles[i].lifeMs <= 0.f;
        }

        // [pointcloud] apply modifiers
        for (auto& modifier : particleModifiers)
        {
            modifier->update(particles, dt);
        }

        // [pointcloud] cull
        if (!particles.empty())
        {
            auto it = std::remove_if(std::begin(particles), std::end(particles), [](const particle& p)
            {
                return p.isDead;
            });
            particles.erase(it, std::end(particles));
        }

        if (particles.size() > instances.size())
        {
            instances.resize(particles.size());
        }        

		for (int i = 0; i < particles.size(); ++i)
		{
			// Create instance particles, with an optional trail
			for (int trail_idx = 0; trail_idx < (trail + 1); ++trail_idx)
			{
				// Apply trail modifiers
				if (trail_idx >= 1)
				{
					particles[trail_idx].position -= particles[trail_idx].velocity * 0.001f;
					particles[trail_idx].size *= 0.97f;
				}
			
				// @fixme - need to account for trail
				instances[i].position_size = float4(particles[i].position, particles[i].size);
				instances[i].color = float4(particles[i].color);
			}
		};
	};

	{
		//scoped_timer t("gl_particle_system::upload");
        // int buffer, int offset, int size bytes, data)
		glNamedBufferSubDataEXT(instanceBuffers->current(), 0, instances.size() * sizeof(instance_data), instances.data());
        instanceBuffers->swap();
	}
}

void gl_particle_system::draw(
    const float4x4 & viewMat, 
    const float4x4 & projMat, 
    gl_shader & shader,
    const bool should_swap) const
{
    if (instances.size() == 0) return;

    shader.bind();

    const GLboolean wasBlendingEnabled = glIsEnabled(GL_BLEND);

    int current_vao {0};
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &current_vao);

    // GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA // Traditional transparency
    // GL_ONE, GL_ONE_MINUS_SRC_ALPHA       // Premultiplied transparency
    // GL_ONE, GL_ONE                       // Additive
    // GL_ONE_MINUS_DST_COLOR, GL_ONE       // Soft additive
    // GL_DST_COLOR, GL_ZERO                // Multiplicative
    // GL_DST_COLOR, GL_SRC_COLOR           // 2x Multiplicative

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
    glDepthMask(GL_FALSE);

    shader.uniform("u_inverseViewMatrix", inverse(viewMat));
    shader.uniform("u_viewProjMat", projMat * viewMat);
    shader.uniform("u_time", elapsed_time_ms);

    if (use_alpha_mask_texture)
    {
        shader.uniform("u_use_alpha_mask", 1.f);
        shader.texture("s_particleTex", 0, particle_tex, GL_TEXTURE_2D);
    }
    else shader.uniform("u_use_alpha_mask", 0.f);

    glBindVertexArray(vao);

    // Instance buffer contains position (xyz) and size/radius (w)
    // An attribute is referred to as instanced if its GL_VERTEX_ATTRIB_ARRAY_DIVISOR value is non-zero. 
    glBindBuffer(GL_ARRAY_BUFFER, instanceBuffers->previous());
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
    glBindVertexArray(current_vao);

    if (wasBlendingEnabled) glEnable(GL_BLEND);
    glDepthMask(GL_TRUE);

    shader.unbind();

    if (should_swap)
    {
        // ... 
    }

    gl_check_error(__FILE__, __LINE__);
}