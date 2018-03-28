// See COPYING file for attribution information

#ifndef bitmask_h
#define bitmask_h

namespace avl
{
    
    template<typename T>
    class BitMask 
    {
        T maskField;
    public:

        BitMask() : maskField(0) {}
        BitMask(T value) : maskField(value) {}

        bool operator == (const BitMask& other) const;
        bool operator == (T mask) const;

        operator T( void ) const;

        // Sets the bit mask
        void set(T mask, bool set);

        // Turns on the bit mask.
        void on(T mask);

        // Turns off the bit mask.
        void off(T mask);

        // Check the bit mask.
        bool isSet(T mask) const;

        // Returns true if the bit mask is not set.
        bool isNotSet(T mask) const;
    };
    
    template<typename T>
    bool BitMask<T>::operator == (const BitMask& other) const
    {
        return maskField == other.maskField;
    }

    template<typename T>
    bool BitMask<T>::operator == (T mask) const
    {
        return maskField == mask;
    }

    template<typename T>
    BitMask<T>::operator T () const
    {
        return maskField;
    }

    template<typename T>
    inline void BitMask<T>::set(T mask, bool set) 
    {
        maskField = set ? (maskField | mask) : (maskField & ~mask);
    }

    template<typename T>
    inline void BitMask<T>::on(T mask) 
    {
        maskField = maskField | mask;
    }

    template<typename T>
    inline void BitMask<T>::off(T mask) 
    {
        maskField = maskField & ~mask;
    }

    template<typename T>
    inline bool BitMask<T>::isSet(T mask) const
    {
        return (maskField & mask) ? true : false;
    }
    
    template<typename T>
    inline bool BitMask<T>::isNotSet(T mask) const
    {
        return (maskField & mask) ? false : true;
    }

} // end util

#endif // end bitmask_h
