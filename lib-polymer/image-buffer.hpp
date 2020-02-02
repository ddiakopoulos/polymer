﻿#pragma once

#ifndef polymer_image_buffer_hpp
#define polymer_image_buffer_hpp

#include "math-core.hpp"
#include "util.hpp"
#include <memory>

namespace polymer
{
	struct camera_intrinsics
	{
		int width{ 0 };    // width of the image in pixels
		int height{ 0 };   // height of the image in pixels
		float ppx{ 0.f };  // horizontal coordinate of the principal point of the image, as a pixel offset from the left edge
		float ppy{ 0.f };  // vertical coordinate of the principal point of the image, as a pixel offset from the top edge
		float fx{ 0.f };   // focal length of the image plane, as a multiple of pixel width
		float fy{ 0.f };   // focal length of the image plane, as a multiple of pixel height
	};

    template <typename T, int C>
    class image_buffer
    {
        int2 dims{ 0, 0 };
        T * alias;
        struct delete_array { void operator()(T * p) { delete[] p; } };
        std::unique_ptr<T, decltype(image_buffer::delete_array())> buffer;

    public:

        image_buffer() {};
        image_buffer(const int2 size) : dims(size), buffer(new T[dims.x * dims.y * C], delete_array()) { alias = buffer.get(); }

        image_buffer(const image_buffer<T, C> & r) : dims(r.dims), buffer(new T[dims.x * dims.y * C], delete_array())
        {
            alias = buffer.get();
            if (r.alias) std::memcpy(alias, r.alias, dims.x * dims.y * C * sizeof(T));
        }

        image_buffer & operator = (const image_buffer<T, C> & r)
        {
            buffer = { new T[dims.x * dims.y * C], delete_array() };
            alias = buffer.get();
            dims = r.dims;
            if (r.alias) std::memcpy(alias, r.alias, dims.x * dims.y * C * sizeof(T));
            return *this;
        }

        int2 size() const { return dims; }
        uint32_t size_bytes() const { return C * dims.x * dims.y * sizeof(T); }
        uint32_t num_pixels() const { return dims.x * dims.y; }
        uint32_t num_channels() const { return C; }
        T * data() { return alias; }
        const T * data() const { return alias; }
        T & operator()(int y, int x) { return alias[y * dims.x + x]; }
        T & operator()(int y, int x, int channel) { return alias[C * (y * dims.x + x) + channel]; }
        const T operator()(int y, int x) const { return alias[y * dims.x + x]; }
        const T operator()(int y, int x, int channel) const { return alias[C * (y * dims.x + x) + channel]; }
        T sample_nearest(const int y, const int x) const;
        T sample_bilinear(const int y, const int x) const;
    };

    template<typename T, int C>
    T image_buffer<T, C>::sample_nearest(const int y, const int x) const
    {
        const int nX = static_cast<int>(std::floor(x + 0.5f));
        const int nY = static_cast<int>(std::floor(y + 0.5f));
        return operator()(nY, nX);
    }

    template<typename T, int C>
    T image_buffer<T, C>::sample_bilinear(const int y, const int x) const
    {
        const int x0 = static_cast<int>(std::floor(x));
        const int y0 = static_cast<int>(std::floor(y));
        const int x1 = x0 + 1;
        const int y1 = y0 + 1;
        const float c0 = clamp(x - x0, 0.0f, 1.0f);
        const float c1 = clamp(y - y0, 0.0f, 1.0f);

        return (1.0f - c1) * ((1.0f - c0) * operator()(y0, x0) + c0 * operator()(y1, x0)) +
            c1 * ((1.0f - c0) * operator()(y0, x1) + c0 * operator()(y1, x1));
    }

    // todo - create image_buffer_view

} // end namespace polymer

#endif // end polymer_image_buffer_hpp