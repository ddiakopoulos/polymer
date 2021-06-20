/*
 * Based on: https://github.com/google/lullaby/blob/master/lullaby/util/unordered_vector_map.h
 * Apache 2.0 License. Copyright 2017 Google Inc. All Rights Reserved.
 * See LICENSE file for full attribution information.
 */

#pragma once

#ifndef polymer_component_pool_hpp
#define polymer_component_pool_hpp

#include <assert.h>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "polymer-engine/ecs/core-ecs.hpp"
#include "polymer-engine/ecs/typeid.hpp"

namespace polymer
{
    /// A map-like container of [Key] to [Object].
    ///
    /// Objects are stored in a vector of arrays to ensure good locality of reference
    /// when iterating over them. Efficient iteration can be done by calling for_each()
    /// or by using the provided iterators. An unordered_map is used to provide O(1)
    /// access to individual objects.
    ///
    /// New objects are always inserted at the "end" of the vector of arrays.
    /// Objects are removed by first swapping the "target" object with the "end"
    /// object, then popping the last object of the end. 
    ///
    /// This container does not provide any order guarantees. Objects stored in
    /// the containers will be shuffled around during removal operations. Any
    /// pointers to objects in the container should be used with care. This
    /// container is also not thread safe. The for_each() function is not
    /// re-entrant: do not insert/remove objects from the container during iteration.

    template <typename Key, typename T, typename KeyFunctionT, typename LookupHashT = std::hash<Key>>
    class unordered_vector_map
    {
        template <bool IsConst> class Iterator;

    public:

        using iterator = Iterator<false>;
        using const_iterator = Iterator<true>;

        // The |page_size| specifies the number of elements to store in contiguous
        // memory before allocating a new "page" for more elements.
        explicit unordered_vector_map(size_t page_size) : page_size(page_size) {}

        unordered_vector_map(const unordered_vector_map & rhs) = delete;
        unordered_vector_map & operator = (const unordered_vector_map & rhs) = delete;

        // Emplaces an object at the end of the container's internal memory and
        // returns a pointer to it. Returns nullptr if there is already an Object in
        // the container that Hashes to the same key.
        // Note: The Object will be created in order to call the KeyFunctionT() function to
        // determine its key. If there is a collision, the newly created Object will
        // be immediately destroyed.
        template<typename... Args> T * emplace(Args &&... args)
        {
            // Grow the internal storage if necessary, either because this is the first
            // element being added or because the "back" array is full.
            if (objects.empty() || objects.back().size() == page_size)
            {
                objects.emplace_back(page_size);
            }

            // Add the element to the "end" of the ArrayVector.
            auto & back_page = objects.back();
            back_page.emplace_back(std::forward<Args>(args)...);
            T & obj = back_page.back();

            // Check to see if an object with this key is already being stored and, if
            // so, remove it. Otherwise, add this new one. we can only check the key after we have created the object.
            KeyFunctionT key_fn;
            const auto & key = key_fn(obj);
            const Index index(objects.size() - 1, back_page.size() - 1);

            if (lookup_table.emplace(key, index).second) return &obj;
            else
            {
                destroy(index);
                return nullptr;
            }
        }

        // destroys the Object associated with |key|. The Object being destroyed will
        // be swapped with the object at the end of the internal storage structure,
        // and then will be "popped" off the back.
        void destroy(const Key & key)
        {
            auto iter = lookup_table.find(key);
            if (iter == lookup_table.end()) return;
            destroy(iter->second);
            lookup_table.erase(iter);
        }

        // Returns true if an Object is associated with the |key|.
        bool contains(const Key & key) const
        {
            return lookup_table.count(key) > 0;
        }

        // Returns a pointer to the Object associated with |key|, or nullptr.
        T * get(const Key & key)
        {
            auto iter = lookup_table.find(key);
            if (iter == lookup_table.end()) return nullptr;

            const Index & index = iter->second;
            T & obj = objects[index.first][index.second];
            return &obj;
        }

        // Returns a pointer to the Object associated with |key|, or nullptr.
        const T * get(const Key & key) const
        {
            auto iter = lookup_table.find(key);
            if (iter == lookup_table.end())  return nullptr;

            const Index & index = iter->second;
            const T & obj = objects[index.first][index.second];
            return &obj;
        }

        // Iterates over all objects, passing them to the given function |Fn|.
        template<typename Fn> void for_each(Fn && func)
        {
            for (T & object : *this) func(object);
        }

        template<typename Fn> void for_each(Fn && func) const
        {
            for (T & object : *this) func(object);
        }

        size_t size() const
        {
            return lookup_table.size();
        }

        void clear()
        {
            objects.clear();
            lookup_table.clear();
        }

        iterator begin() { return iterator(objects.begin(), objects.end()); }
        const_iterator begin() const { return const_iterator(objects.begin(), objects.end()); }
        iterator end() { return iterator(objects.end()); }
        const_iterator end() const { return const_iterator(objects.end()); }

    private:

        using ArrayVector = std::vector<std::vector<T>>;                 // An array of an array of objects for cache-efficient iteration.
        using Index = std::pair<size_t, size_t>;                         // A pair of indices to the two arrays in the ArrayVector.
        using LookupTable = std::unordered_map<Key, Index, LookupHashT>; // Table that maps a Key to a specific element in the ArrayVector.

       // Destroys the Object at the specified |index|. Performs a swap-and-pop for objects not at the end of the ArrayVector.
        void destroy(const Index & index)
        {
            // The object to remove is in the "middle" of the ArrayVector, so swap it with the one at the very end.
            auto & back_page = objects.back();
            const bool is_in_last_page = (index.first == objects.size() - 1);
            const bool is_last_element = (index.second == back_page.size() - 1);

            if (!is_in_last_page || !is_last_element)
            {
                T & obj = objects[index.first][index.second];
                T & other = back_page[back_page.size() - 1];
                KeyFunctionT key_fn;
                lookup_table[key_fn(other)] = index;

                // Good thing to note: if the user has provided a copy assignment operator, it 
                // blocks the automatic implementation of a move constructor, and can be used in its place.
                std::swap(obj, other);
            }

            // The object we want to destroy is at the very back, so just pop it.
            objects.back().pop_back();

            // If the "back" array is empty, we can remove it.
            if (objects.back().size() == 0) objects.pop_back();
        }

        // The vector of arrays used to store Object instances.
        ArrayVector objects;

        // The map of key to Index in the ArrayVector for an Object.
        LookupTable lookup_table;

        // The maximum size of the internal array in the ArrayVector.
        size_t page_size;

        // An STL style iterator for accessing the elements of an unordered_vector_map.
        // This class provides both the const and non-const implementation. If the
        // container is modified at any point during the iteration then the iterator
        // will become invalid.
        template <bool IsConst>
        class Iterator
        {
            using OuterIterator = typename std::conditional<IsConst, typename ArrayVector::const_iterator, typename ArrayVector::iterator>::type;
            using InnerIterator = typename std::conditional<IsConst, typename std::vector<T>::const_iterator, typename std::vector<T>::iterator>::type;

        public:

            // STL iterator traits
            using iterator_category = std::forward_iterator_tag;
            using value_type = typename std::iterator_traits<InnerIterator>::value_type;
            using difference_type = typename std::iterator_traits<InnerIterator>::difference_type;
            using reference = typename std::iterator_traits<InnerIterator>::reference;
            using pointer = typename std::iterator_traits<InnerIterator>::pointer;

            // A default constructed iterator should first be assigned to a valid
            // iterator. Any operation before assignment other than destruction is undefined.
            Iterator() = default;

            // Allow conversion from the non-const iterator to the const
            // iterator. Relies on the fact that the internal iterators only support conversions from non-const to const.
            Iterator(const Iterator<false> & other) : outer_(other.outer_), outer_end_(other.outer_end_), inner_(other.inner_) { }

            reference operator*() const
            {
                assert(outer_ != outer_end_);
                return *inner_;
            }

            pointer operator->() const
            {
                assert(outer_ != outer_end_);
                return inner_;
            }

            Iterator & operator++()
            {
                assert(outer_ != outer_end_);
                ++inner_;
                find_next_element();
                return *this;
            }

            Iterator operator++(int)
            {
                Iterator temp(*this);
                operator++();
                return temp;
            }

            // Allow assignment from a non-const iterator to a const iterator. Relies on
            // the fact that the internal iterators only support assignment from
            // non-const to const.
            Iterator & operator=(const Iterator<false>& other)
            {
                outer_ = other.outer_;
                outer_end_ = other.outer_end_;
                inner_ = other.inner_;
                return *this;
            }

            // Allow equality comparison between const and non-const iterators.
            friend bool operator == (const Iterator& lhs, const Iterator& rhs)
            {
                return lhs.outer_ == rhs.outer_ && (lhs.outer_ == lhs.outer_end_ || lhs.inner_ == rhs.inner_);
            }

            // Allow inequality comparison between const and non-const iterators.
            friend bool operator != (const Iterator & lhs, const Iterator & rhs) { return !(lhs == rhs); }

        private:

            // Allow the container class access to the private constructors.
            friend class unordered_vector_map;

            // Allow the const iterator access to the private members of the non-const
            // iterator for the conversion constructor and assignment.
            friend class Iterator<true>;

            // Construct an end iterator.
            explicit Iterator(OuterIterator outer_end) : Iterator(outer_end, outer_end) {}

            // Construct an iterator with a potentially different beginning and end.
            Iterator(OuterIterator outer_begin, OuterIterator outer_end) : outer_(outer_begin), outer_end_(outer_end)
            {
                if (outer_ != outer_end_)
                {
                    inner_ = outer_->begin();
                    find_next_element();
                }
            }

            // If the inner iterator has reached the end of the current container then
            // find the next element, skipping over any empty inner containers.
            void find_next_element()
            {
                while (inner_ == outer_->end() && ++outer_ != outer_end_)
                {
                    inner_ = outer_->begin();
                }
            }

            OuterIterator outer_;
            OuterIterator outer_end_;
            InnerIterator inner_;
        };
    };

    template <typename T>
    using polymer_component_pool = unordered_vector_map<entity, T, component_hash, std::hash<entity>>;

} // end namespace polymer

#endif // polymer_component_pool_hpp
