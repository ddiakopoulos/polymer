#pragma once

#ifndef polymer_guid_hpp
#define polymer_guid_hpp

// Loosely based on https://github.com/graeme-hill/crossguid

#include <array>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

namespace polymer
{
    class poly_guid
    {
        std::array<uint8_t, 16> byte_array{ {0} };

        friend std::ostream & operator << (std::ostream & s, const poly_guid & guid)
        {
            std::ios_base::fmtflags f(s.flags());

            s << std::hex << std::setfill('0')
              << std::setw(2) << (uint32_t)guid.byte_array[0]
              << std::setw(2) << (uint32_t)guid.byte_array[1]
              << std::setw(2) << (uint32_t)guid.byte_array[2]
              << std::setw(2) << (uint32_t)guid.byte_array[3]
              << "-"
              << std::setw(2) << (uint32_t)guid.byte_array[4]
              << std::setw(2) << (uint32_t)guid.byte_array[5]
              << "-" 
              << std::setw(2) << (uint32_t)guid.byte_array[6]
              << std::setw(2) << (uint32_t)guid.byte_array[7]
              << "-"
              << std::setw(2) << (uint32_t)guid.byte_array[8]
              << std::setw(2) << (uint32_t)guid.byte_array[9]
              << "-" 
              << std::setw(2) << (uint32_t)guid.byte_array[10]
              << std::setw(2) << (uint32_t)guid.byte_array[11]
              << std::setw(2) << (uint32_t)guid.byte_array[12]
              << std::setw(2) << (uint32_t)guid.byte_array[13]
              << std::setw(2) << (uint32_t)guid.byte_array[14]
              << std::setw(2) << (uint32_t)guid.byte_array[15];

            s.flags(f);

            return s;
        }

        friend bool operator < (const poly_guid & lhs, const poly_guid & rhs) { return lhs.bytes() < rhs.bytes(); }

    public:

        poly_guid(const std::array<uint8_t, 16> & bytes) : byte_array(bytes) {}
        poly_guid(const uint8_t * bytes) { std::copy(bytes, bytes + 16, std::begin(byte_array)); }

        poly_guid(const std::string & guid_as_string)
        {
            auto hex_digit_to_char = [](const char ch) -> uint8_t
            {
                if (ch > 47 && ch < 58)  return ch - 48;
                if (ch > 96 && ch < 103) return ch - 87;
                if (ch > 64 && ch < 71)  return ch - 55;
                return 0;
            };

            auto valid_hex_character = [](const char ch) -> bool
            {
                if (ch > 47 && ch < 58)  return true;
                if (ch > 96 && ch < 103) return true;
                if (ch > 64 && ch < 71)  return true;
                return false;
            };

             auto hex_pair_to_character = [hex_digit_to_char](const char a, const char b) -> uint8_t
             {
                return hex_digit_to_char(a) * 16 + hex_digit_to_char(b);
             };

            char charOne = '\0';
            char charTwo = '\0';
            bool lookingForFirstChar = true;
            unsigned nextByte = 0;

            for (const char &ch : guid_as_string)
            {
                if (ch == '-') continue;

                if (nextByte >= 16 || !valid_hex_character(ch))
                {
                    std::fill(byte_array.begin(), byte_array.end(), static_cast<uint8_t>(0));
                    return;
                }

                if (lookingForFirstChar)
                {
                    charOne = ch;
                    lookingForFirstChar = false;
                }
                else
                {
                    charTwo = ch;
                    auto byte = hex_pair_to_character(charOne, charTwo);
                    byte_array[nextByte++] = byte;
                    lookingForFirstChar = true;
                }
            }

            if (nextByte < 16)
            {
                std::fill(byte_array.begin(), byte_array.end(), static_cast<unsigned char>(0));
                return;
            }
        }

        poly_guid() {};
        poly_guid(const poly_guid & other) : byte_array(other.byte_array) {}
        poly_guid & operator = (const poly_guid & other) { poly_guid(other).swap(*this); return *this; }
        bool operator == (const poly_guid & other) const { return byte_array == other.byte_array; }
        bool operator != (const poly_guid & other) const { return !((*this) == other); }
        operator std::string() const { return as_string(); }
        const std::array<uint8_t, 16> & bytes() const { return byte_array; }
        void swap(poly_guid & other) { byte_array.swap(other.byte_array); }
        bool valid() const { return *this != poly_guid(); }

        std::string as_string() const
        {
            char one[10], two[6], three[6], four[6], five[14];
            snprintf(one,   10, "%02x%02x%02x%02x", byte_array[0], byte_array[1], byte_array[2], byte_array[3]);
            snprintf(two,   6,  "%02x%02x", byte_array[4], byte_array[5]);
            snprintf(three, 6,  "%02x%02x", byte_array[6], byte_array[7]);
            snprintf(four,  6,  "%02x%02x", byte_array[8], byte_array[9]);
            snprintf(five,  14, "%02x%02x%02x%02x%02x%02x", byte_array[10], byte_array[11], byte_array[12], byte_array[13], byte_array[14], byte_array[15]);
            const std::string sep("-");
            return std::string(one + sep + two + sep + three + sep + four + sep + five);
        }
    };

    inline poly_guid make_guid()
    {
        std::mt19937_64 rnd;
        std::uniform_int_distribution<uint32_t> dist(0, 255);

        std::chrono::high_resolution_clock::time_point epoch;
        std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
        const auto duration = now - epoch;
        rnd.seed(static_cast<uint64_t>(duration.count()));

        auto random_byte = [&]() -> uint8_t
        {
            return static_cast<uint8_t>(dist(rnd));
        };

        std::array<uint8_t, 16> arr = { {
            random_byte(), random_byte(), random_byte(), random_byte(),
            random_byte(), random_byte(), random_byte(), random_byte(),
            random_byte(), random_byte(), random_byte(), random_byte(),
            random_byte(), random_byte(), random_byte(), random_byte()
        } };

        return arr;
    }

} // end namespace polymer

namespace std
{
    template <> inline void swap(polymer::poly_guid & lhs, polymer::poly_guid & rhs) noexcept { lhs.swap(rhs); }
    template <> struct hash<polymer::poly_guid>
    {
        typedef polymer::poly_guid argument_type;
        typedef std::size_t result_type;
        result_type operator()(argument_type const & guid) const
        {
            std::hash<std::string> h;
            return static_cast<result_type>(h(guid.as_string()));
        }
    };

} // end namespace std

#endif // polymer_guid_hpp
