#pragma once

#ifndef polymer_bitmask_hpp
#define polymer_bitmask_hpp

namespace polymer
{
    /// Based on FlagSet.h from the Nimble Library
    /// Copyright (c) 2015 Dmitry Sovetov, MIT License (https://github.com/dmsovetov/Nimble)
    template<typename T>
    class bit_mask 
    {
        T maskField{ 0 };

    public:

        bit_mask() = default;
        bit_mask(T value) : maskField(value) {}

        bool operator == (const bit_mask & other) const;
        bool operator == (T mask) const;

        operator T(void) const;

        // Sets the bit mask
        void set(T mask, bool set);

        // Turns on the bit mask.
        void on(T mask);

        // Turns off the bit mask.
        void off(T mask);

        // Check the bit mask.
        bool is_set(T mask) const;

        // Returns true if the bit mask is not set.
        bool is_not_set(T mask) const;
    };
    
    template<typename T>
    bool bit_mask<T>::operator == (const bit_mask & other) const
    {
        return maskField == other.maskField;
    }

    template<typename T>
    bool bit_mask<T>::operator == (T mask) const
    {
        return maskField == mask;
    }

    template<typename T>
    bit_mask<T>::operator T () const
    {
        return maskField;
    }

    template<typename T>
    inline void bit_mask<T>::set(T mask, bool set) 
    {
        maskField = set ? (maskField | mask) : (maskField & ~mask);
    }

    template<typename T>
    inline void bit_mask<T>::on(T mask) 
    {
        maskField = maskField | mask;
    }

    template<typename T>
    inline void bit_mask<T>::off(T mask) 
    {
        maskField = maskField & ~mask;
    }

    template<typename T>
    inline bool bit_mask<T>::is_set(T mask) const
    {
        return (maskField & mask) ? true : false;
    }
    
    template<typename T>
    inline bool bit_mask<T>::is_not_set(T mask) const
    {
        return (maskField & mask) ? false : true;
    }

} // end util

#endif // end polymer_bitmask_hpp
