// This file is part of meshoptimizer library; see meshoptimizer.h for version/license details
#include "meshoptimizer.h"

#include <assert.h>
#include <string.h>

size_t meshopt_optimizeVertexFetchRemap(unsigned int* destination, const unsigned int* indices, size_t index_count, size_t vertex_count)
{
	assert(index_count % 3 == 0);

	memset(destination, -1, vertex_count * sizeof(unsigned int));

	unsigned int next_vertex = 0;

	for (size_t i = 0; i < index_count; ++i)
	{
		unsigned int index = indices[i];
		assert(index < vertex_count);

		if (destination[index] == ~0u)
		{
			destination[index] = next_vertex++;
		}
	}

	assert(next_vertex <= vertex_count);

	return next_vertex;
}

size_t meshopt_optimizeVertexFetch(void* destination, unsigned int* indices, size_t index_count, const void* vertices, size_t vertex_count, size_t vertex_size)
{
	assert(index_count % 3 == 0);
	assert(vertex_size > 0 && vertex_size <= 256);

	// support in-place optimization
	meshopt_Buffer<char> vertices_copy;

	if (destination == vertices)
	{
		vertices_copy.allocate(vertex_count * vertex_size);
		memcpy(vertices_copy.data, vertices, vertex_count * vertex_size);
		vertices = vertices_copy.data;
	}

	// build vertex remap table
	meshopt_Buffer<unsigned int> vertex_remap(vertex_count);
	memset(vertex_remap.data, -1, vertex_remap.size * sizeof(unsigned int));

	unsigned int next_vertex = 0;

	for (size_t i = 0; i < index_count; ++i)
	{
		unsigned int index = indices[i];
		assert(index < vertex_count);

		unsigned int& remap = vertex_remap[index];

		if (remap == ~0u) // vertex was not added to destination VB
		{
			// add vertex
			memcpy(static_cast<char*>(destination) + next_vertex * vertex_size, static_cast<const char*>(vertices) + index * vertex_size, vertex_size);

			remap = next_vertex++;
		}

		// modify indices in place
		indices[i] = remap;
	}

	assert(next_vertex <= vertex_count);

	return next_vertex;
}
