#pragma once

#ifndef polymer_image_buffer_hpp
#define polymer_image_buffer_hpp

#include "polymer-core/math/math-core.hpp"
#include "polymer-core/util/util.hpp"

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

    template <typename T>
    class image_buffer
    {
        int2 dims{ 0, 0 };
        int channels {0};
        T * alias {nullptr};
        struct delete_array { void operator()(T * p) { delete[] p; } };
        std::unique_ptr<T, decltype(image_buffer::delete_array())> buffer;

    public:

        image_buffer() {};
        image_buffer(const int2 & size, const uint32_t channels) : dims(size), channels(channels),
            buffer(new T[dims.x * dims.y * channels], delete_array()) {
            alias = buffer.get();
        }

        image_buffer(const image_buffer<T> & r) : dims(r.dims), buffer(new T[dims.x * dims.y * channels], delete_array())
        {
            alias = buffer.get();
            if (r.alias) std::memcpy(alias, r.alias, dims.x * dims.y * channels * sizeof(T));
        }

        image_buffer & operator = (const image_buffer<T> & r)
        {
            buffer = { new T[dims.x * dims.y * channels], delete_array() };
            alias = buffer.get();
            dims = r.dims;
            if (r.alias) std::memcpy(alias, r.alias, dims.x * dims.y * channels * sizeof(T));
            return *this;
        }

        int2 size() const { return dims; }
        uint32_t size_bytes() const { return channels * dims.x * dims.y * sizeof(T); }
        uint32_t num_pixels() const { return dims.x * dims.y; }
        uint32_t num_channels() const { return channels; }
        T * data() { return alias; }
        const T * data() const { return alias; }
        T & operator()(int y, int x) { return alias[y * dims.x + x]; }
        T & operator()(int y, int x, int channel) { return alias[channels * (y * dims.x + x) + channel]; }
        const T operator()(int y, int x) const { return alias[y * dims.x + x]; }
        const T operator()(int y, int x, int channel) const { return alias[channels * (y * dims.x + x) + channel]; }
        T sample_nearest(const int y, const int x) const;
        T sample_bilinear(const int y, const int x) const;
    };

    template<typename T>
    T image_buffer<T>::sample_nearest(const int y, const int x) const
    {
        const int nX = static_cast<int>(std::floor(x + 0.5f));
        const int nY = static_cast<int>(std::floor(y + 0.5f));
        return operator()(nY, nX);
    }

    template<typename T>
    T image_buffer<T>::sample_bilinear(const int y, const int x) const
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

    // crop
    template<typename T>
    inline image_buffer<T> subrect(const image_buffer<T> & image, const int2 & origin, const int2 & size)
    {
        const int2 img_size = image.size();
        assert(x0 >= 0 && y0 >= 0 && x1 <= img_size.x && y1 <= img_size.y);
    
        image_buffer<T, C> result(size);

        const int x0 = origin.x;
        const int y0 = origin.y;
        const int x1 = x0 + size.x;
        const int y1 = y0 + size.y;

        T * in = image.data();
        T * out = result.data();

        const int stride = size.x;
        const int channels = image.num_channels();

        for (int y = y0; y < y1; ++y)
        {
            std::memcpy(out, in + (y * stride + x0) * channels, stride * channels);
            out += stride * channels;
        }
    
        return result;
    }

    // template <typename T>
    // inline std::vector<std::vector<T>> tile_image(const std::vector<T> & image, const int imgWidth, const int imgHeight, const int rowDivisor, const int colDivisor)
    // {
    //     std::vector<std::vector<T>> blocks(rowDivisor * colDivisor);
    // 
    //     const int tileSizeX = (imgWidth / rowDivisor);
    //     const int tileSizeY = (imgHeight / colDivisor);
    // 
    //     for (int r = 0; r < blocks.size(); r++)
    //     {
    //         blocks[r].resize(tileSizeX * tileSizeY);
    //     }
    // 
    //     // Does it fit?
    //     if ((imgHeight % colDivisor == 0) && (imgWidth % rowDivisor == 0))
    //     {
    //         int blockIdx_y = 0;
    //         for (int y = 0; y < imgHeight; y += imgHeight / colDivisor)
    //         {
    //             int blockIdx_x = 0;
    //             for (int x = 0; x < imgWidth; x += imgWidth / rowDivisor)
    //             {
    //                 auto & block = blocks[blockIdx_y * rowDivisor + blockIdx_x];
    //                 block = crop<T>(image, imgWidth, imgHeight, x, y, tileSizeX, tileSizeY);
    //                 blockIdx_x++;
    //             }
    //             blockIdx_y++;
    //         }
    //     }
    //     else if (imgHeight % colDivisor != 0 || imgWidth % rowDivisor != 0) throw std::runtime_error("image divisor doesn't fit");
    // 
    //     return blocks;
    // }

    // @todo - create image_buffer_view

} // end namespace polymer

#endif // end polymer_image_buffer_hpp
