// This header file contains the declaration and definition of an "inlined
// vector" which behaves in an equivalent fashion to a `std::vector`, except
// that storage for small sequences of the vector are provided inline without
// requiring any heap allocation.
//
// An `polymer::inlined_vector<T, N>` specifies the default capacity `N` as one of
// its template parameters. Instances where `size() <= N` hold contained
// elements in inline space. Typically `N` is very small so that sequences that
// are expected to be short do not require allocations.
//
// An `polymer::inlined_vector` does not usually require a specific allocator. If
// the inlined vector grows beyond its initial constraints, it will need to
// allocate (as any normal `std::vector` would). This is usually performed with
// the default allocator (defined as `std::allocator<T>`). Optionally, a custom
// allocator type may be specified as `A` in `polymer::inlined_vector<T, N, A>`.

// This file was modified from the original polymer::inlined_vector. It removes the use of the absl
// namespace, and includes most dependent code from other parts of the Abseil codebase.
// It currently requires type definitions from polymer::any and the use of the nonstd::span class.

// Original License
//
// Copyright 2019 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#ifndef polymer_inlined_vector_hpp
#define polymer_inlined_vector_hpp

#include "any.hpp"
#include "nonstd/span.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

namespace polymer
{
    namespace inlined_vector_internal
    {
        template <typename Iterator>
        using IsAtLeastForwardIterator =
            std::is_convertible<typename std::iterator_traits<Iterator>::iterator_category, std::forward_iterator_tag>;

        template <typename AllocatorType, typename ValueType = typename std::allocator_traits<AllocatorType>::value_type>
        using IsMemcpyOk =
            polymer::conjunction<std::is_same<AllocatorType, std::allocator<ValueType>>, std::is_trivially_copy_constructible<ValueType>,
                                 std::is_trivially_copy_assignable<ValueType>, std::is_trivially_destructible<ValueType>>;

        template <typename AllocatorType, typename Pointer, typename SizeType>
        void DestroyElements(AllocatorType * alloc_ptr, Pointer destroy_first, SizeType destroy_size)
        {
            using AllocatorTraits = std::allocator_traits<AllocatorType>;

            if (destroy_first != nullptr)
            {
                for (auto i = destroy_size; i != 0;)
                {
                    --i;
                    AllocatorTraits::destroy(*alloc_ptr, destroy_first + i);
                }

#if !defined(NDEBUG)
                {
                    using ValueType = typename AllocatorTraits::value_type;

                    // Overwrite unused memory with `0xab` so we can catch uninitialized
                    // usage.
                    //
                    // Cast to `void*` to tell the compiler that we don't care that we might
                    // be scribbling on a vtable pointer.
                    void * memory_ptr = destroy_first;
                    auto memory_size  = destroy_size * sizeof(ValueType);
                    std::memset(memory_ptr, 0xab, memory_size);
                }
#endif  // !defined(NDEBUG)
            }
        }

        template <typename AllocatorType, typename Pointer, typename ValueAdapter, typename SizeType>
        void ConstructElements(AllocatorType * alloc_ptr, Pointer construct_first, ValueAdapter * values_ptr, SizeType construct_size)
        {
            for (SizeType i = 0; i < construct_size; ++i)
            {
                try
                {
                    values_ptr->ConstructNext(alloc_ptr, construct_first + i);
                }
                catch (...)
                {
                    inlined_vector_internal::DestroyElements(alloc_ptr, construct_first, i);
                    throw;
                }
            }
        }

        template <typename Pointer, typename ValueAdapter, typename SizeType>
        void AssignElements(Pointer assign_first, ValueAdapter * values_ptr, SizeType assign_size)
        {
            for (SizeType i = 0; i < assign_size; ++i) { values_ptr->AssignNext(assign_first + i); }
        }

        template <typename AllocatorType>
        struct StorageView
        {
            using AllocatorTraits = std::allocator_traits<AllocatorType>;
            using Pointer         = typename AllocatorTraits::pointer;
            using SizeType        = typename AllocatorTraits::size_type;

            Pointer data;
            SizeType size;
            SizeType capacity;
        };

        template <typename AllocatorType, typename Iterator>
        class IteratorValueAdapter
        {
            using AllocatorTraits = std::allocator_traits<AllocatorType>;
            using Pointer         = typename AllocatorTraits::pointer;

        public:
            explicit IteratorValueAdapter(const Iterator & it) : it_(it) {}

            void ConstructNext(AllocatorType * alloc_ptr, Pointer construct_at)
            {
                AllocatorTraits::construct(*alloc_ptr, construct_at, *it_);
                ++it_;
            }

            void AssignNext(Pointer assign_at)
            {
                *assign_at = *it_;
                ++it_;
            }

        private:
            Iterator it_;
        };

        template <typename AllocatorType>
        class CopyValueAdapter
        {
            using AllocatorTraits = std::allocator_traits<AllocatorType>;
            using ValueType       = typename AllocatorTraits::value_type;
            using Pointer         = typename AllocatorTraits::pointer;
            using ConstPointer    = typename AllocatorTraits::const_pointer;

            ConstPointer ptr_;

        public:
            explicit CopyValueAdapter(const ValueType & v) : ptr_(std::addressof(v)) {}

            void ConstructNext(AllocatorType * alloc_ptr, Pointer construct_at)
            {
                AllocatorTraits::construct(*alloc_ptr, construct_at, *ptr_);
            }

            void AssignNext(Pointer assign_at) { *assign_at = *ptr_; }
        };

        template <typename AllocatorType>
        class DefaultValueAdapter
        {
            using AllocatorTraits = std::allocator_traits<AllocatorType>;
            using ValueType       = typename AllocatorTraits::value_type;
            using Pointer         = typename AllocatorTraits::pointer;

        public:
            explicit DefaultValueAdapter() {}

            void ConstructNext(AllocatorType * alloc_ptr, Pointer construct_at) { AllocatorTraits::construct(*alloc_ptr, construct_at); }

            void AssignNext(Pointer assign_at) { *assign_at = ValueType(); }
        };

        template <typename AllocatorType>
        class AllocationTransaction
        {
            using AllocatorTraits = std::allocator_traits<AllocatorType>;
            using Pointer         = typename AllocatorTraits::pointer;
            using SizeType        = typename AllocatorTraits::size_type;

            std::tuple<AllocatorType, Pointer> alloc_data_;
            SizeType capacity_ = 0;

        public:
            explicit AllocationTransaction(AllocatorType * alloc_ptr) : alloc_data_(*alloc_ptr, nullptr) {}

            ~AllocationTransaction()
            {
                if (DidAllocate()) { AllocatorTraits::deallocate(GetAllocator(), GetData(), GetCapacity()); }
            }

            AllocationTransaction(const AllocationTransaction &) = delete;
            void operator=(const AllocationTransaction &) = delete;

            AllocatorType & GetAllocator() { return std::get<0>(alloc_data_); }
            Pointer & GetData() { return std::get<1>(alloc_data_); }
            SizeType & GetCapacity() { return capacity_; }

            bool DidAllocate() { return GetData() != nullptr; }
            Pointer Allocate(SizeType capacity)
            {
                GetData()     = AllocatorTraits::allocate(GetAllocator(), capacity);
                GetCapacity() = capacity;
                return GetData();
            }

            void Reset()
            {
                GetData()     = nullptr;
                GetCapacity() = 0;
            }
        };

        template <typename AllocatorType>
        class ConstructionTransaction
        {
            using AllocatorTraits = std::allocator_traits<AllocatorType>;
            using Pointer         = typename AllocatorTraits::pointer;
            using SizeType        = typename AllocatorTraits::size_type;

        public:
            explicit ConstructionTransaction(AllocatorType * alloc_ptr) : alloc_data_(*alloc_ptr, nullptr) {}

            ~ConstructionTransaction()
            {
                if (DidConstruct()) { inlined_vector_internal::DestroyElements(std::addressof(GetAllocator()), GetData(), GetSize()); }
            }

            ConstructionTransaction(const ConstructionTransaction &) = delete;
            void operator=(const ConstructionTransaction &) = delete;

            AllocatorType & GetAllocator() { return std::get<0>(alloc_data_); }
            Pointer & GetData() { return std::get<1>(alloc_data_); }
            SizeType & GetSize() { return size_; }

            bool DidConstruct() { return GetData() != nullptr; }
            template <typename ValueAdapter>
            void Construct(Pointer data, ValueAdapter * values_ptr, SizeType size)
            {
                inlined_vector_internal::ConstructElements(std::addressof(GetAllocator()), data, values_ptr, size);
                GetData() = data;
                GetSize() = size;
            }

            void Commit()
            {
                GetData() = nullptr;
                GetSize() = 0;
            }

        private:
            std::tuple<AllocatorType, Pointer> alloc_data_;
            SizeType size_ = 0;
        };

        template <typename T, size_t N, typename A>
        class Storage
        {
        public:
            using AllocatorTraits        = std::allocator_traits<A>;
            using allocator_type         = typename AllocatorTraits::allocator_type;
            using value_type             = typename AllocatorTraits::value_type;
            using pointer                = typename AllocatorTraits::pointer;
            using const_pointer          = typename AllocatorTraits::const_pointer;
            using size_type              = typename AllocatorTraits::size_type;
            using difference_type        = typename AllocatorTraits::difference_type;
            using reference              = value_type &;
            using const_reference        = const value_type &;
            using RValueReference        = value_type &&;
            using iterator               = pointer;
            using const_iterator         = const_pointer;
            using reverse_iterator       = std::reverse_iterator<iterator>;
            using const_reverse_iterator = std::reverse_iterator<const_iterator>;
            using MoveIterator           = std::move_iterator<iterator>;
            using IsMemcpyOk             = inlined_vector_internal::IsMemcpyOk<allocator_type>;

            using StorageView = inlined_vector_internal::StorageView<allocator_type>;

            template <typename Iterator>
            using IteratorValueAdapter    = inlined_vector_internal::IteratorValueAdapter<allocator_type, Iterator>;

            using CopyValueAdapter        = inlined_vector_internal::CopyValueAdapter<allocator_type>;
            using DefaultValueAdapter     = inlined_vector_internal::DefaultValueAdapter<allocator_type>;
            using AllocationTransaction   = inlined_vector_internal::AllocationTransaction<allocator_type>;
            using ConstructionTransaction = inlined_vector_internal::ConstructionTransaction<allocator_type>;

            static size_type NextCapacity(size_type current_capacity) { return current_capacity * 2; }

            static size_type ComputeCapacity(size_type current_capacity, size_type requested_capacity)
            {
                return (std::max)(NextCapacity(current_capacity), requested_capacity);
            }

            // ---------------------------------------------------------------------------
            // Storage Constructors and Destructor
            // ---------------------------------------------------------------------------

            Storage() : metadata_() {}
            explicit Storage(const allocator_type & alloc) : metadata_(alloc, {}) {}

            ~Storage()
            {
                pointer data = GetIsAllocated() ? GetAllocatedData() : GetInlinedData();
                inlined_vector_internal::DestroyElements(GetAllocPtr(), data, GetSize());
                DeallocateIfAllocated();
            }

            // ---------------------------------------------------------------------------
            // Storage Member Accessors
            // ---------------------------------------------------------------------------

            size_type & GetSizeAndIsAllocated() { return static_cast<size_type>(std::get<1>(metadata_)); }

            const size_type & GetSizeAndIsAllocated() const { return static_cast<size_type>(std::get<1>(metadata_)); }

            size_type GetSize() const { return GetSizeAndIsAllocated() >> 1; }

            bool GetIsAllocated() const { return GetSizeAndIsAllocated() & 1; }

            pointer GetAllocatedData() { return data_.allocated.allocated_data; }

            const_pointer GetAllocatedData() const { return data_.allocated.allocated_data; }

            pointer GetInlinedData() { return reinterpret_cast<pointer>(std::addressof(data_.inlined.inlined_data[0])); }

            const_pointer GetInlinedData() const { return reinterpret_cast<const_pointer>(std::addressof(data_.inlined.inlined_data[0])); }

            size_type GetAllocatedCapacity() const { return data_.allocated.allocated_capacity; }

            size_type GetInlinedCapacity() const { return static_cast<size_type>(N); }

            StorageView MakeStorageView()
            {
                return GetIsAllocated() ? StorageView {GetAllocatedData(), GetSize(), GetAllocatedCapacity()} :
                                          StorageView {GetInlinedData(), GetSize(), GetInlinedCapacity()};
            }

            allocator_type * GetAllocPtr() { return std::addressof(std::get<0>(metadata_)); }

            const allocator_type * GetAllocPtr() const { return std::addressof(std::get<0>(metadata_)); }

            // ---------------------------------------------------------------------------
            // Storage Member Mutators
            // ---------------------------------------------------------------------------

            template <typename ValueAdapter>
            void Initialize(ValueAdapter values, size_type new_size);

            template <typename ValueAdapter>
            void Assign(ValueAdapter values, size_type new_size);

            template <typename ValueAdapter>
            void Resize(ValueAdapter values, size_type new_size);

            template <typename ValueAdapter>
            iterator Insert(const_iterator pos, ValueAdapter values, size_type insert_count);

            template <typename... Args>
            reference EmplaceBack(Args &&... args);

            iterator Erase(const_iterator from, const_iterator to);

            void Reserve(size_type requested_capacity);

            void ShrinkToFit();

            void Swap(Storage * other_storage_ptr);

            void SetIsAllocated() { GetSizeAndIsAllocated() |= static_cast<size_type>(1); }

            void UnsetIsAllocated() { GetSizeAndIsAllocated() &= ((std::numeric_limits<size_type>::max)() - 1); }

            void SetSize(size_type size) { GetSizeAndIsAllocated() = (size << 1) | static_cast<size_type>(GetIsAllocated()); }

            void SetAllocatedSize(size_type size) { GetSizeAndIsAllocated() = (size << 1) | static_cast<size_type>(1); }

            void SetInlinedSize(size_type size) { GetSizeAndIsAllocated() = size << static_cast<size_type>(1); }

            void AddSize(size_type count) { GetSizeAndIsAllocated() += count << static_cast<size_type>(1); }

            void SubtractSize(size_type count)
            {
                assert(count <= GetSize());

                GetSizeAndIsAllocated() -= count << static_cast<size_type>(1);
            }

            void SetAllocatedData(pointer data, size_type capacity)
            {
                data_.allocated.allocated_data     = data;
                data_.allocated.allocated_capacity = capacity;
            }

            void AcquireAllocatedData(AllocationTransaction * allocation_tx_ptr)
            {
                SetAllocatedData(allocation_tx_ptr->GetData(), allocation_tx_ptr->GetCapacity());
                allocation_tx_ptr->Reset();
            }

            void MemcpyFrom(const Storage & other_storage)
            {
                assert(IsMemcpyOk::value || other_storage.GetIsAllocated());

                GetSizeAndIsAllocated() = other_storage.GetSizeAndIsAllocated();
                data_                   = other_storage.data_;
            }

            void DeallocateIfAllocated()
            {
                if (GetIsAllocated()) { AllocatorTraits::deallocate(*GetAllocPtr(), GetAllocatedData(), GetAllocatedCapacity()); }
            }

        private:
            using Metadata = std::tuple<allocator_type, size_type>;

            struct Allocated
            {
                pointer allocated_data;
                size_type allocated_capacity;
            };

            struct Inlined
            {
                alignas(value_type) char inlined_data[sizeof(value_type[N])];
            };

            union Data
            {
                Allocated allocated;
                Inlined inlined;
            };

            Metadata metadata_;
            Data data_;
        };

        template <typename T, size_t N, typename A>
        template <typename ValueAdapter>
        auto Storage<T, N, A>::Initialize(ValueAdapter values, size_type new_size) -> void
        {
            // Only callable from constructors!
            assert(!GetIsAllocated());
            assert(GetSize() == 0);

            pointer construct_data;
            if (new_size > GetInlinedCapacity())
            {
                // Because this is only called from the `inlined_vector` constructors, it's
                // safe to take on the allocation with size `0`. If `ConstructElements(...)`
                // throws, deallocation will be automatically handled by `~Storage()`.
                size_type new_capacity = ComputeCapacity(GetInlinedCapacity(), new_size);
                construct_data         = AllocatorTraits::allocate(*GetAllocPtr(), new_capacity);
                SetAllocatedData(construct_data, new_capacity);
                SetIsAllocated();
            }
            else
            {
                construct_data = GetInlinedData();
            }

            inlined_vector_internal::ConstructElements(GetAllocPtr(), construct_data, &values, new_size);

            // Since the initial size was guaranteed to be `0` and the allocated bit is
            // already correct for either case, *adding* `new_size` gives us the correct
            // result faster than setting it directly.
            AddSize(new_size);
        }

        template <typename T, size_t N, typename A>
        template <typename ValueAdapter>
        auto Storage<T, N, A>::Assign(ValueAdapter values, size_type new_size) -> void
        {
            StorageView storage_view = MakeStorageView();

            AllocationTransaction allocation_tx(GetAllocPtr());

            nonstd::span<value_type> assign_loop;
            nonstd::span<value_type> construct_loop;
            nonstd::span<value_type> destroy_loop;

            if (new_size > storage_view.capacity)
            {
                size_type new_capacity = ComputeCapacity(storage_view.capacity, new_size);
                construct_loop         = {allocation_tx.Allocate(new_capacity), new_size};
                destroy_loop           = {storage_view.data, storage_view.size};
            }
            else if (new_size > storage_view.size)
            {
                assign_loop    = {storage_view.data, storage_view.size};
                construct_loop = {storage_view.data + storage_view.size, new_size - storage_view.size};
            }
            else
            {
                assign_loop  = {storage_view.data, new_size};
                destroy_loop = {storage_view.data + new_size, storage_view.size - new_size};
            }

            inlined_vector_internal::AssignElements(assign_loop.data(), &values, assign_loop.size());
            inlined_vector_internal::ConstructElements(GetAllocPtr(), construct_loop.data(), &values, construct_loop.size());
            inlined_vector_internal::DestroyElements(GetAllocPtr(), destroy_loop.data(), destroy_loop.size());

            if (allocation_tx.DidAllocate())
            {
                DeallocateIfAllocated();
                AcquireAllocatedData(&allocation_tx);
                SetIsAllocated();
            }

            SetSize(new_size);
        }

        template <typename T, size_t N, typename A>
        template <typename ValueAdapter>
        auto Storage<T, N, A>::Resize(ValueAdapter values, size_type new_size) -> void
        {
            StorageView storage_view = MakeStorageView();

            IteratorValueAdapter<MoveIterator> move_values(MoveIterator(storage_view.data));

            AllocationTransaction allocation_tx(GetAllocPtr());
            ConstructionTransaction construction_tx(GetAllocPtr());

            nonstd::span<value_type> construct_loop;
            nonstd::span<value_type> move_construct_loop;
            nonstd::span<value_type> destroy_loop;

            if (new_size > storage_view.capacity)
            {
                size_type new_capacity = ComputeCapacity(storage_view.capacity, new_size);
                pointer new_data       = allocation_tx.Allocate(new_capacity);
                construct_loop         = {new_data + storage_view.size, new_size - storage_view.size};
                move_construct_loop    = {new_data, storage_view.size};
                destroy_loop           = {storage_view.data, storage_view.size};
            }
            else if (new_size > storage_view.size)
            {
                construct_loop = {storage_view.data + storage_view.size, new_size - storage_view.size};
            }
            else
            {
                destroy_loop = {storage_view.data + new_size, storage_view.size - new_size};
            }

            construction_tx.Construct(construct_loop.data(), &values, construct_loop.size());
            inlined_vector_internal::ConstructElements(GetAllocPtr(), move_construct_loop.data(), &move_values, move_construct_loop.size());
            inlined_vector_internal::DestroyElements(GetAllocPtr(), destroy_loop.data(), destroy_loop.size());

            construction_tx.Commit();
            if (allocation_tx.DidAllocate())
            {
                DeallocateIfAllocated();
                AcquireAllocatedData(&allocation_tx);
                SetIsAllocated();
            }

            SetSize(new_size);
        }

        template <typename T, size_t N, typename A>
        template <typename ValueAdapter>
        auto Storage<T, N, A>::Insert(const_iterator pos, ValueAdapter values, size_type insert_count) -> iterator
        {
            StorageView storage_view = MakeStorageView();

            size_type insert_index     = std::distance(const_iterator(storage_view.data), pos);
            size_type insert_end_index = insert_index + insert_count;
            size_type new_size         = storage_view.size + insert_count;

            if (new_size > storage_view.capacity)
            {
                AllocationTransaction allocation_tx(GetAllocPtr());
                ConstructionTransaction construction_tx(GetAllocPtr());
                ConstructionTransaction move_construciton_tx(GetAllocPtr());

                IteratorValueAdapter<MoveIterator> move_values(MoveIterator(storage_view.data));

                size_type new_capacity = ComputeCapacity(storage_view.capacity, new_size);
                pointer new_data       = allocation_tx.Allocate(new_capacity);

                construction_tx.Construct(new_data + insert_index, &values, insert_count);
                move_construciton_tx.Construct(new_data, &move_values, insert_index);
                inlined_vector_internal::ConstructElements(GetAllocPtr(), new_data + insert_end_index, &move_values,
                                                           storage_view.size - insert_index);
                inlined_vector_internal::DestroyElements(GetAllocPtr(), storage_view.data, storage_view.size);

                construction_tx.Commit();
                move_construciton_tx.Commit();
                DeallocateIfAllocated();
                AcquireAllocatedData(&allocation_tx);

                SetAllocatedSize(new_size);
                return iterator(new_data + insert_index);
            }
            else
            {
                size_type move_construction_destination_index = (std::max)(insert_end_index, storage_view.size);

                ConstructionTransaction move_construction_tx(GetAllocPtr());

                IteratorValueAdapter<MoveIterator> move_construction_values(
                    MoveIterator(storage_view.data + (move_construction_destination_index - insert_count)));
                nonstd::span<value_type> move_construction = {storage_view.data + move_construction_destination_index,
                                                              new_size - move_construction_destination_index};

                pointer move_assignment_values           = storage_view.data + insert_index;
                nonstd::span<value_type> move_assignment = {storage_view.data + insert_end_index,
                                                            move_construction_destination_index - insert_end_index};

                nonstd::span<value_type> insert_assignment = {move_assignment_values, move_construction.size()};

                nonstd::span<value_type> insert_construction = {insert_assignment.data() + insert_assignment.size(),
                                                                insert_count - insert_assignment.size()};

                move_construction_tx.Construct(move_construction.data(), &move_construction_values, move_construction.size());

                for (pointer destination = move_assignment.data() + move_assignment.size(), last_destination = move_assignment.data(),
                             source = move_assignment_values + move_assignment.size();
                     ;)
                {
                    --destination;
                    --source;
                    if (destination < last_destination) break;
                    *destination = std::move(*source);
                }

                inlined_vector_internal::AssignElements(insert_assignment.data(), &values, insert_assignment.size());
                inlined_vector_internal::ConstructElements(GetAllocPtr(), insert_construction.data(), &values, insert_construction.size());

                move_construction_tx.Commit();

                AddSize(insert_count);
                return iterator(storage_view.data + insert_index);
            }
        }

        template <typename T, size_t N, typename A>
        template <typename... Args>
        auto Storage<T, N, A>::EmplaceBack(Args &&... args)
            -> reference
        {
            StorageView storage_view = MakeStorageView();

            AllocationTransaction allocation_tx(GetAllocPtr());

            IteratorValueAdapter<MoveIterator> move_values(MoveIterator(storage_view.data));

            pointer construct_data;
            if (storage_view.size == storage_view.capacity)
            {
                size_type new_capacity = NextCapacity(storage_view.capacity);
                construct_data         = allocation_tx.Allocate(new_capacity);
            }
            else
            {
                construct_data = storage_view.data;
            }

            pointer last_ptr = construct_data + storage_view.size;

            AllocatorTraits::construct(*GetAllocPtr(), last_ptr, std::forward<Args>(args)...);

            if (allocation_tx.DidAllocate())
            {
                try
                {
                    inlined_vector_internal::ConstructElements(GetAllocPtr(), allocation_tx.GetData(), &move_values, storage_view.size);
                }
                catch (...)
                {
                    AllocatorTraits::destroy(*GetAllocPtr(), last_ptr);
                    throw;
                }

                inlined_vector_internal::DestroyElements(GetAllocPtr(), storage_view.data, storage_view.size);

                DeallocateIfAllocated();
                AcquireAllocatedData(&allocation_tx);
                SetIsAllocated();
            }

            AddSize(1);
            return *last_ptr;
        }

        template <typename T, size_t N, typename A>
        auto Storage<T, N, A>::Erase(const_iterator from, const_iterator to) -> iterator
        {
            StorageView storage_view = MakeStorageView();

            size_type erase_size      = std::distance(from, to);
            size_type erase_index     = std::distance(const_iterator(storage_view.data), from);
            size_type erase_end_index = erase_index + erase_size;

            IteratorValueAdapter<MoveIterator> move_values(MoveIterator(storage_view.data + erase_end_index));

            inlined_vector_internal::AssignElements(storage_view.data + erase_index, &move_values, storage_view.size - erase_end_index);

            inlined_vector_internal::DestroyElements(GetAllocPtr(), storage_view.data + (storage_view.size - erase_size), erase_size);

            SubtractSize(erase_size);
            return iterator(storage_view.data + erase_index);
        }

        template <typename T, size_t N, typename A>
        auto Storage<T, N, A>::Reserve(size_type requested_capacity) -> void
        {
            StorageView storage_view = MakeStorageView();

            if ((requested_capacity <= storage_view.capacity)) return;

            AllocationTransaction allocation_tx(GetAllocPtr());

            IteratorValueAdapter<MoveIterator> move_values(MoveIterator(storage_view.data));

            size_type new_capacity = ComputeCapacity(storage_view.capacity, requested_capacity);
            pointer new_data       = allocation_tx.Allocate(new_capacity);

            inlined_vector_internal::ConstructElements(GetAllocPtr(), new_data, &move_values, storage_view.size);

            inlined_vector_internal::DestroyElements(GetAllocPtr(), storage_view.data, storage_view.size);

            DeallocateIfAllocated();
            AcquireAllocatedData(&allocation_tx);
            SetIsAllocated();
        }

        template <typename T, size_t N, typename A>
        auto Storage<T, N, A>::ShrinkToFit() -> void
        {
            // May only be called on allocated instances!
            assert(GetIsAllocated());

            StorageView storage_view {GetAllocatedData(), GetSize(), GetAllocatedCapacity()};

            if ((storage_view.size == storage_view.capacity)) return;

            AllocationTransaction allocation_tx(GetAllocPtr());

            IteratorValueAdapter<MoveIterator> move_values(MoveIterator(storage_view.data));

            pointer construct_data;
            if (storage_view.size > GetInlinedCapacity())
            {
                size_type new_capacity = storage_view.size;
                construct_data         = allocation_tx.Allocate(new_capacity);
            }
            else
            {
                construct_data = GetInlinedData();
            }

            try
            {
                inlined_vector_internal::ConstructElements(GetAllocPtr(), construct_data, &move_values, storage_view.size);
            }
            catch (const std::exception & e)
            {
                SetAllocatedData(storage_view.data, storage_view.capacity);
                throw;
            }

            inlined_vector_internal::DestroyElements(GetAllocPtr(), storage_view.data, storage_view.size);
            AllocatorTraits::deallocate(*GetAllocPtr(), storage_view.data, storage_view.capacity);

            if (allocation_tx.DidAllocate()) AcquireAllocatedData(&allocation_tx);
            else
                UnsetIsAllocated();
        }

        template <typename T, size_t N, typename A>
        auto Storage<T, N, A>::Swap(Storage * other_storage_ptr) -> void
        {
            using std::swap;
            assert(this != other_storage_ptr);

            if (GetIsAllocated() && other_storage_ptr->GetIsAllocated()) { swap(data_.allocated, other_storage_ptr->data_.allocated); }
            else if (!GetIsAllocated() && !other_storage_ptr->GetIsAllocated())
            {
                Storage * small_ptr = this;
                Storage * large_ptr = other_storage_ptr;
                if (small_ptr->GetSize() > large_ptr->GetSize()) swap(small_ptr, large_ptr);

                for (size_type i = 0; i < small_ptr->GetSize(); ++i)
                { swap(small_ptr->GetInlinedData()[i], large_ptr->GetInlinedData()[i]); }

                IteratorValueAdapter<MoveIterator> move_values(MoveIterator(large_ptr->GetInlinedData() + small_ptr->GetSize()));

                inlined_vector_internal::ConstructElements(large_ptr->GetAllocPtr(), small_ptr->GetInlinedData() + small_ptr->GetSize(),
                                                           &move_values, large_ptr->GetSize() - small_ptr->GetSize());

                inlined_vector_internal::DestroyElements(large_ptr->GetAllocPtr(), large_ptr->GetInlinedData() + small_ptr->GetSize(),
                                                         large_ptr->GetSize() - small_ptr->GetSize());
            }
            else
            {
                Storage * allocated_ptr = this;
                Storage * inlined_ptr   = other_storage_ptr;
                if (!allocated_ptr->GetIsAllocated()) swap(allocated_ptr, inlined_ptr);

                StorageView allocated_storage_view {allocated_ptr->GetAllocatedData(), allocated_ptr->GetSize(),
                                                    allocated_ptr->GetAllocatedCapacity()};

                IteratorValueAdapter<MoveIterator> move_values(MoveIterator(inlined_ptr->GetInlinedData()));

                try
                {
                    inlined_vector_internal::ConstructElements(inlined_ptr->GetAllocPtr(), allocated_ptr->GetInlinedData(), &move_values,
                                                               inlined_ptr->GetSize());
                }
                catch (const std::exception & e)
                {
                    allocated_ptr->SetAllocatedData(allocated_storage_view.data, allocated_storage_view.capacity);
                    throw;
                }

                inlined_vector_internal::DestroyElements(inlined_ptr->GetAllocPtr(), inlined_ptr->GetInlinedData(), inlined_ptr->GetSize());

                inlined_ptr->SetAllocatedData(allocated_storage_view.data, allocated_storage_view.capacity);
            }

            swap(GetSizeAndIsAllocated(), other_storage_ptr->GetSizeAndIsAllocated());
            swap(*GetAllocPtr(), *other_storage_ptr->GetAllocPtr());
        }

    }  // namespace inlined_vector_internal

    //////////////////////////
    //    inlined_vector    //
    //////////////////////////

    template <typename T, size_t N, typename A = std::allocator<T>>
    class inlined_vector
    {
        static_assert(N > 0, "`polymer::inlined_vector` requires an inlined capacity.");

        using Storage = inlined_vector_internal::Storage<T, N, A>;

        using AllocatorTraits = typename Storage::AllocatorTraits;
        using RValueReference = typename Storage::RValueReference;
        using MoveIterator    = typename Storage::MoveIterator;
        using IsMemcpyOk      = typename Storage::IsMemcpyOk;

        template <typename Iterator>
        using IteratorValueAdapter = typename Storage::template IteratorValueAdapter<Iterator>;
        using CopyValueAdapter     = typename Storage::CopyValueAdapter;
        using DefaultValueAdapter  = typename Storage::DefaultValueAdapter;

        template <typename Iterator>
        using EnableIfAtLeastForwardIterator =
            std::enable_if_t<inlined_vector_internal::IsAtLeastForwardIterator<Iterator>::value>;

        template <typename Iterator>
        using DisableIfAtLeastForwardIterator =
            std::enable_if_t<!inlined_vector_internal::IsAtLeastForwardIterator<Iterator>::value>;

    public:
        using allocator_type         = typename Storage::allocator_type;
        using value_type             = typename Storage::value_type;
        using pointer                = typename Storage::pointer;
        using const_pointer          = typename Storage::const_pointer;
        using size_type              = typename Storage::size_type;
        using difference_type        = typename Storage::difference_type;
        using reference              = typename Storage::reference;
        using const_reference        = typename Storage::const_reference;
        using iterator               = typename Storage::iterator;
        using const_iterator         = typename Storage::const_iterator;
        using reverse_iterator       = typename Storage::reverse_iterator;
        using const_reverse_iterator = typename Storage::const_reverse_iterator;

        // ---------------------------------------------------------------------------
        // inlined_vector Constructors and Destructor
        // ---------------------------------------------------------------------------

        // Creates an empty inlined vector with a value-initialized allocator.
        inlined_vector() noexcept(noexcept(allocator_type())) : storage_() {}

        // Creates an empty inlined vector with a copy of `alloc`.
        explicit inlined_vector(const allocator_type & alloc) noexcept : storage_(alloc) {}

        // Creates an inlined vector with `n` copies of `value_type()`.
        explicit inlined_vector(size_type n, const allocator_type & alloc = allocator_type()) : storage_(alloc)
        {
            storage_.Initialize(DefaultValueAdapter(), n);
        }

        // Creates an inlined vector with `n` copies of `v`.
        inlined_vector(size_type n, const_reference v, const allocator_type & alloc = allocator_type()) : storage_(alloc)
        {
            storage_.Initialize(CopyValueAdapter(v), n);
        }

        // Creates an inlined vector with copies of the elements of `list`.
        inlined_vector(std::initializer_list<value_type> list, const allocator_type & alloc = allocator_type())
            : inlined_vector(list.begin(), list.end(), alloc)
        {
        }

        // Creates an inlined vector with elements constructed from the provided
        // forward iterator range [`first`, `last`).
        //
        // NOTE: the `enable_if` prevents ambiguous interpretation between a call to
        // this constructor with two integral arguments and a call to the above
        // `inlined_vector(size_type, const_reference)` constructor.
        template <typename ForwardIterator, EnableIfAtLeastForwardIterator<ForwardIterator> * = nullptr>
        inlined_vector(ForwardIterator first, ForwardIterator last, const allocator_type & alloc = allocator_type()) : storage_(alloc)
        {
            storage_.Initialize(IteratorValueAdapter<ForwardIterator>(first), std::distance(first, last));
        }

        // Creates an inlined vector with elements constructed from the provided input
        // iterator range [`first`, `last`).
        template <typename InputIterator, DisableIfAtLeastForwardIterator<InputIterator> * = nullptr>
        inlined_vector(InputIterator first, InputIterator last, const allocator_type & alloc = allocator_type()) : storage_(alloc)
        {
            std::copy(first, last, std::back_inserter(*this));
        }

        // Creates an inlined vector by copying the contents of `other` using
        // `other`'s allocator.
        inlined_vector(const inlined_vector & other) : inlined_vector(other, *other.storage_.GetAllocPtr()) {}

        // Creates an inlined vector by copying the contents of `other` using `alloc`.
        inlined_vector(const inlined_vector & other, const allocator_type & alloc) : storage_(alloc)
        {
            if (IsMemcpyOk::value && !other.storage_.GetIsAllocated()) { storage_.MemcpyFrom(other.storage_); }
            else
            {
                storage_.Initialize(IteratorValueAdapter<const_pointer>(other.data()), other.size());
            }
        }

        // Creates an inlined vector by moving in the contents of `other` without
        // allocating. If `other` contains allocated memory, the newly-created inlined
        // vector will take ownership of that memory. However, if `other` does not
        // contain allocated memory, the newly-created inlined vector will perform
        // element-wise move construction of the contents of `other`.
        //
        // NOTE: since no allocation is performed for the inlined vector in either
        // case, the `noexcept(...)` specification depends on whether moving the
        // underlying objects can throw. It is assumed assumed that...
        //  a) move constructors should only throw due to allocation failure.
        //  b) if `value_type`'s move constructor allocates, it uses the same
        //     allocation function as the inlined vector's allocator.
        // Thus, the move constructor is non-throwing if the allocator is non-throwing
        // or `value_type`'s move constructor is specified as `noexcept`.
        inlined_vector(inlined_vector && other) noexcept : storage_(*other.storage_.GetAllocPtr())
        {
            if (IsMemcpyOk::value)
            {
                storage_.MemcpyFrom(other.storage_);
                other.storage_.SetInlinedSize(0);
            }
            else if (other.storage_.GetIsAllocated())
            {
                storage_.SetAllocatedData(other.storage_.GetAllocatedData(), other.storage_.GetAllocatedCapacity());
                storage_.SetAllocatedSize(other.storage_.GetSize());
                other.storage_.SetInlinedSize(0);
            }
            else
            {
                IteratorValueAdapter<MoveIterator> other_values(MoveIterator(other.storage_.GetInlinedData()));
                inlined_vector_internal::ConstructElements(storage_.GetAllocPtr(), storage_.GetInlinedData(), &other_values,
                                                           other.storage_.GetSize());
                storage_.SetInlinedSize(other.storage_.GetSize());
            }
        }

        // Creates an inlined vector by moving in the contents of `other` with a copy
        // of `alloc`.
        //
        // NOTE: if `other`'s allocator is not equal to `alloc`, even if `other`
        // contains allocated memory, this move constructor will still allocate. Since
        // allocation is performed, this constructor can only be `noexcept` if the
        // specified allocator is also `noexcept`.
        inlined_vector(inlined_vector && other, const allocator_type & alloc) noexcept : storage_(alloc)
        {
            if (IsMemcpyOk::value)
            {
                storage_.MemcpyFrom(other.storage_);
                other.storage_.SetInlinedSize(0);
            }
            else if ((*storage_.GetAllocPtr() == *other.storage_.GetAllocPtr()) && other.storage_.GetIsAllocated())
            {
                storage_.SetAllocatedData(other.storage_.GetAllocatedData(), other.storage_.GetAllocatedCapacity());
                storage_.SetAllocatedSize(other.storage_.GetSize());
                other.storage_.SetInlinedSize(0);
            }
            else
            {
                storage_.Initialize(IteratorValueAdapter<MoveIterator>(MoveIterator(other.data())), other.size());
            }
        }

        ~inlined_vector() {}

        // ---------------------------------------------------------------------------
        // inlined_vector Member Accessors
        // ---------------------------------------------------------------------------

        // `inlined_vector::empty()`
        // Returns whether the inlined vector contains no elements.
        bool empty() const noexcept { return !size(); }

        // `inlined_vector::size()`
        // Returns the number of elements in the inlined vector.
        size_type size() const noexcept { return storage_.GetSize(); }

        // `inlined_vector::max_size()`
        // Returns the maximum number of elements the inlined vector can hold.
        size_type max_size() const noexcept
        {
            // One bit of the size storage is used to indicate whether the inlined
            // vector contains allocated memory. As a result, the maximum size that the
            // inlined vector can express is half of the max for `size_type`.
            return (std::numeric_limits<size_type>::max)() / 2;
        }

        // `inlined_vector::capacity()`
        //
        // Returns the number of elements that could be stored in the inlined vector
        // without requiring a reallocation.
        //
        // NOTE: for most inlined vectors, `capacity()` should be equal to the
        // template parameter `N`. For inlined vectors which exceed this capacity,
        // they will no longer be inlined and `capacity()` will equal the capactity of
        // the allocated memory.
        size_type capacity() const noexcept
        {
            return storage_.GetIsAllocated() ? storage_.GetAllocatedCapacity() : storage_.GetInlinedCapacity();
        }

        // `inlined_vector::data()`
        //
        // Returns a `pointer` to the elements of the inlined vector. This pointer
        // can be used to access and modify the contained elements.
        //
        // NOTE: only elements within [`data()`, `data() + size()`) are valid.
        pointer data() noexcept { return storage_.GetIsAllocated() ? storage_.GetAllocatedData() : storage_.GetInlinedData(); }

        // Overload of `inlined_vector::data()` that returns a `const_pointer` to the
        // elements of the inlined vector. This pointer can be used to access but not
        // modify the contained elements.
        //
        // NOTE: only elements within [`data()`, `data() + size()`) are valid.
        const_pointer data() const noexcept { return storage_.GetIsAllocated() ? storage_.GetAllocatedData() : storage_.GetInlinedData(); }

        // `inlined_vector::operator[](...)`
        //
        // Returns a `reference` to the `i`th element of the inlined vector.
        reference operator[](size_type i)
        {
            assert(i < size());

            return data()[i];
        }

        // Overload of `inlined_vector::operator[](...)` that returns a
        // `const_reference` to the `i`th element of the inlined vector.
        const_reference operator[](size_type i) const
        {
            assert(i < size());

            return data()[i];
        }

        // `inlined_vector::at(...)`
        //
        // Returns a `reference` to the `i`th element of the inlined vector.
        //
        // NOTE: if `i` is not within the required range of `inlined_vector::at(...)`,
        // in both debug and non-debug builds, `std::out_of_range` will be thrown.
        reference at(size_type i)
        {
            if ((i >= size())) { throw std::invalid_argument("`inlined_vector::at(size_type)` failed bounds check"); }

            return data()[i];
        }

        // Overload of `inlined_vector::at(...)` that returns a `const_reference` to
        // the `i`th element of the inlined vector.
        //
        // NOTE: if `i` is not within the required range of `inlined_vector::at(...)`,
        // in both debug and non-debug builds, `std::out_of_range` will be thrown.
        const_reference at(size_type i) const
        {
            if ((i >= size())) { throw std::invalid_argument("`inlined_vector::at(size_type) const` failed bounds check"); }

            return data()[i];
        }

        // `inlined_vector::front()`
        //
        // Returns a `reference` to the first element of the inlined vector.
        reference front()
        {
            assert(!empty());

            return at(0);
        }

        // Overload of `inlined_vector::front()` that returns a `const_reference` to
        // the first element of the inlined vector.
        const_reference front() const
        {
            assert(!empty());

            return at(0);
        }

        // `inlined_vector::back()`
        //
        // Returns a `reference` to the last element of the inlined vector.
        reference back()
        {
            assert(!empty());

            return at(size() - 1);
        }

        // Overload of `inlined_vector::back()` that returns a `const_reference` to the
        // last element of the inlined vector.
        const_reference back() const
        {
            assert(!empty());

            return at(size() - 1);
        }

        // `inlined_vector::begin()`
        //
        // Returns an `iterator` to the beginning of the inlined vector.
        iterator begin() noexcept { return data(); }

        // Overload of `inlined_vector::begin()` that returns a `const_iterator` to
        // the beginning of the inlined vector.
        const_iterator begin() const noexcept { return data(); }

        // `inlined_vector::end()`
        //
        // Returns an `iterator` to the end of the inlined vector.
        iterator end() noexcept { return data() + size(); }

        // Overload of `inlined_vector::end()` that returns a `const_iterator` to the
        // end of the inlined vector.
        const_iterator end() const noexcept { return data() + size(); }

        // `inlined_vector::cbegin()`
        //
        // Returns a `const_iterator` to the beginning of the inlined vector.
        const_iterator cbegin() const noexcept { return begin(); }

        // `inlined_vector::cend()`
        //
        // Returns a `const_iterator` to the end of the inlined vector.
        const_iterator cend() const noexcept { return end(); }

        // `inlined_vector::rbegin()`
        //
        // Returns a `reverse_iterator` from the end of the inlined vector.
        reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }

        // Overload of `inlined_vector::rbegin()` that returns a
        // `const_reverse_iterator` from the end of the inlined vector.
        const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }

        // `inlined_vector::rend()`
        //
        // Returns a `reverse_iterator` from the beginning of the inlined vector.
        reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

        // Overload of `inlined_vector::rend()` that returns a `const_reverse_iterator`
        // from the beginning of the inlined vector.
        const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

        // `inlined_vector::crbegin()`
        //
        // Returns a `const_reverse_iterator` from the end of the inlined vector.
        const_reverse_iterator crbegin() const noexcept { return rbegin(); }

        // `inlined_vector::crend()`
        //
        // Returns a `const_reverse_iterator` from the beginning of the inlined
        // vector.
        const_reverse_iterator crend() const noexcept { return rend(); }

        // `inlined_vector::get_allocator()`
        //
        // Returns a copy of the inlined vector's allocator.
        allocator_type get_allocator() const { return *storage_.GetAllocPtr(); }

        // ---------------------------------------------------------------------------
        // inlined_vector Member Mutators
        // ---------------------------------------------------------------------------

        // `inlined_vector::operator=(...)`
        //
        // Replaces the elements of the inlined vector with copies of the elements of
        // `list`.
        inlined_vector & operator=(std::initializer_list<value_type> list)
        {
            assign(list.begin(), list.end());
            return *this;
        }

        // Overload of `inlined_vector::operator=(...)` that replaces the elements of
        // the inlined vector with copies of the elements of `other`.
        inlined_vector & operator=(const inlined_vector & other)
        {
            if ((this != std::addressof(other)))
            {
                const_pointer other_data = other.data();
                assign(other_data, other_data + other.size());
            }

            return *this;
        }

        // Overload of `inlined_vector::operator=(...)` that moves the elements of
        // `other` into the inlined vector.
        //
        // NOTE: as a result of calling this overload, `other` is left in a valid but
        // unspecified state.
        inlined_vector & operator=(inlined_vector && other)
        {
            if ((this != std::addressof(other)))
            {
                if (IsMemcpyOk::value || other.storage_.GetIsAllocated())
                {
                    inlined_vector_internal::DestroyElements(storage_.GetAllocPtr(), data(), size());
                    storage_.DeallocateIfAllocated();
                    storage_.MemcpyFrom(other.storage_);
                    other.storage_.SetInlinedSize(0);
                }
                else
                {
                    storage_.Assign(IteratorValueAdapter<MoveIterator>(MoveIterator(other.storage_.GetInlinedData())), other.size());
                }
            }

            return *this;
        }

        // `inlined_vector::assign(...)`
        //
        // Replaces the contents of the inlined vector with `n` copies of `v`.
        void assign(size_type n, const_reference v) { storage_.Assign(CopyValueAdapter(v), n); }

        // Overload of `inlined_vector::assign(...)` that replaces the contents of the
        // inlined vector with copies of the elements of `list`.
        void assign(std::initializer_list<value_type> list) { assign(list.begin(), list.end()); }

        // Overload of `inlined_vector::assign(...)` to replace the contents of the
        // inlined vector with the range [`first`, `last`).
        //
        // NOTE: this overload is for iterators that are "forward" category or better.
        template <typename ForwardIterator, EnableIfAtLeastForwardIterator<ForwardIterator> * = nullptr>
        void assign(ForwardIterator first, ForwardIterator last)
        {
            storage_.Assign(IteratorValueAdapter<ForwardIterator>(first), std::distance(first, last));
        }

        // Overload of `inlined_vector::assign(...)` to replace the contents of the
        // inlined vector with the range [`first`, `last`).
        //
        // NOTE: this overload is for iterators that are "input" category.
        template <typename InputIterator, DisableIfAtLeastForwardIterator<InputIterator> * = nullptr>
        void assign(InputIterator first, InputIterator last)
        {
            size_type i = 0;
            for (; i < size() && first != last; ++i, static_cast<void>(++first)) { at(i) = *first; }

            erase(data() + i, data() + size());
            std::copy(first, last, std::back_inserter(*this));
        }

        // `inlined_vector::resize(...)`
        //
        // Resizes the inlined vector to contain `n` elements.
        //
        // NOTE: if `n` is smaller than `size()`, extra elements are destroyed. If `n`
        // is larger than `size()`, new elements are value-initialized.
        void resize(size_type n) { storage_.Resize(DefaultValueAdapter(), n); }

        // Overload of `inlined_vector::resize(...)` that resizes the inlined vector to
        // contain `n` elements.
        //
        // NOTE: if `n` is smaller than `size()`, extra elements are destroyed. If `n`
        // is larger than `size()`, new elements are copied-constructed from `v`.
        void resize(size_type n, const_reference v) { storage_.Resize(CopyValueAdapter(v), n); }

        // `inlined_vector::insert(...)`
        //
        // Inserts a copy of `v` at `pos`, returning an `iterator` to the newly
        // inserted element.
        iterator insert(const_iterator pos, const_reference v) { return emplace(pos, v); }

        // Overload of `inlined_vector::insert(...)` that inserts `v` at `pos` using
        // move semantics, returning an `iterator` to the newly inserted element.
        iterator insert(const_iterator pos, RValueReference v) { return emplace(pos, std::move(v)); }

        // Overload of `inlined_vector::insert(...)` that inserts `n` contiguous copies
        // of `v` starting at `pos`, returning an `iterator` pointing to the first of
        // the newly inserted elements.
        iterator insert(const_iterator pos, size_type n, const_reference v)
        {
            assert(pos >= begin());
            assert(pos <= end());

            if ((n != 0))
            {
                value_type dealias = v;
                return storage_.Insert(pos, CopyValueAdapter(dealias), n);
            }
            else
            {
                return const_cast<iterator>(pos);
            }
        }

        // Overload of `inlined_vector::insert(...)` that inserts copies of the
        // elements of `list` starting at `pos`, returning an `iterator` pointing to
        // the first of the newly inserted elements.
        iterator insert(const_iterator pos, std::initializer_list<value_type> list) { return insert(pos, list.begin(), list.end()); }

        // Overload of `inlined_vector::insert(...)` that inserts the range [`first`,
        // `last`) starting at `pos`, returning an `iterator` pointing to the first
        // of the newly inserted elements.
        //
        // NOTE: this overload is for iterators that are "forward" category or better.
        template <typename ForwardIterator, EnableIfAtLeastForwardIterator<ForwardIterator> * = nullptr>
        iterator insert(const_iterator pos, ForwardIterator first, ForwardIterator last)
        {
            assert(pos >= begin());
            assert(pos <= end());

            if ((first != last)) { return storage_.Insert(pos, IteratorValueAdapter<ForwardIterator>(first), std::distance(first, last)); }
            else
            {
                return const_cast<iterator>(pos);
            }
        }

        // Overload of `inlined_vector::insert(...)` that inserts the range [`first`,
        // `last`) starting at `pos`, returning an `iterator` pointing to the first
        // of the newly inserted elements.
        //
        // NOTE: this overload is for iterators that are "input" category.
        template <typename InputIterator, DisableIfAtLeastForwardIterator<InputIterator> * = nullptr>
        iterator insert(const_iterator pos, InputIterator first, InputIterator last)
        {
            assert(pos >= begin());
            assert(pos <= end());

            size_type index = std::distance(cbegin(), pos);
            for (size_type i = index; first != last; ++i, static_cast<void>(++first)) { insert(data() + i, *first); }

            return iterator(data() + index);
        }

        // `inlined_vector::emplace(...)`
        //
        // Constructs and inserts an element using `args...` in the inlined vector at
        // `pos`, returning an `iterator` pointing to the newly emplaced element.
        template <typename... Args>
        iterator emplace(const_iterator pos, Args &&... args)
        {
            assert(pos >= begin());
            assert(pos <= end());

            value_type dealias(std::forward<Args>(args)...);
            return storage_.Insert(pos, IteratorValueAdapter<MoveIterator>(MoveIterator(std::addressof(dealias))), 1);
        }

        // `inlined_vector::emplace_back(...)`
        //
        // Constructs and inserts an element using `args...` in the inlined vector at
        // `end()`, returning a `reference` to the newly emplaced element.
        template <typename... Args>
        reference emplace_back(Args &&... args)
        {
            return storage_.EmplaceBack(std::forward<Args>(args)...);
        }

        // `inlined_vector::push_back(...)`
        //
        // Inserts a copy of `v` in the inlined vector at `end()`.
        void push_back(const_reference v) { static_cast<void>(emplace_back(v)); }

        // Overload of `inlined_vector::push_back(...)` for inserting `v` at `end()`
        // using move semantics.
        void push_back(RValueReference v) { static_cast<void>(emplace_back(std::move(v))); }

        // `inlined_vector::pop_back()`
        //
        // Destroys the element at `back()`, reducing the size by `1`.
        void pop_back() noexcept
        {
            assert(!empty());

            AllocatorTraits::destroy(*storage_.GetAllocPtr(), data() + (size() - 1));
            storage_.SubtractSize(1);
        }

        // `inlined_vector::erase(...)`
        //
        // Erases the element at `pos`, returning an `iterator` pointing to where the
        // erased element was located.
        //
        // NOTE: may return `end()`, which is not dereferencable.
        iterator erase(const_iterator pos)
        {
            assert(pos >= begin());
            assert(pos < end());

            return storage_.Erase(pos, pos + 1);
        }

        // Overload of `inlined_vector::erase(...)` that erases every element in the
        // range [`from`, `to`), returning an `iterator` pointing to where the first
        // erased element was located.
        //
        // NOTE: may return `end()`, which is not dereferencable.
        iterator erase(const_iterator from, const_iterator to)
        {
            assert(from >= begin());
            assert(from <= to);
            assert(to <= end());

            if ((from != to)) { return storage_.Erase(from, to); }
            else
            {
                return const_cast<iterator>(from);
            }
        }

        // `inlined_vector::clear()`
        //
        // Destroys all elements in the inlined vector, setting the size to `0` and
        // deallocating any held memory.
        void clear() noexcept
        {
            inlined_vector_internal::DestroyElements(storage_.GetAllocPtr(), data(), size());
            storage_.DeallocateIfAllocated();

            storage_.SetInlinedSize(0);
        }

        // `inlined_vector::reserve(...)`
        //
        // Ensures that there is enough room for at least `n` elements.
        void reserve(size_type n) { storage_.Reserve(n); }

        // `inlined_vector::shrink_to_fit()`
        //
        // Reduces memory usage by freeing unused memory. After being called, calls to
        // `capacity()` will be equal to `max(N, size())`.
        //
        // If `size() <= N` and the inlined vector contains allocated memory, the
        // elements will all be moved to the inlined space and the allocated memory
        // will be deallocated.
        //
        // If `size() > N` and `size() < capacity()`, the elements will be moved to a
        // smaller allocation.
        void shrink_to_fit()
        {
            if (storage_.GetIsAllocated()) { storage_.ShrinkToFit(); }
        }

        // `inlined_vector::swap(...)`
        //
        // Swaps the contents of the inlined vector with `other`.
        void swap(inlined_vector & other)
        {
            if ((this != std::addressof(other))) { storage_.Swap(std::addressof(other.storage_)); }
        }

    private:
        template <typename H, typename TheT, size_t TheN, typename TheA>
        friend H PolyIVHashValue(H h, const polymer::inlined_vector<TheT, TheN, TheA> & a);
        Storage storage_;
    };

    // -----------------------------------------------------------------------------
    // inlined_vector Non-Member Functions
    // -----------------------------------------------------------------------------

    // `swap(...)`
    //
    // Swaps the contents of two inlined vectors.
    template <typename T, size_t N, typename A>
    void swap(polymer::inlined_vector<T, N, A> & a, polymer::inlined_vector<T, N, A> & b) noexcept(noexcept(a.swap(b)))
    {
        a.swap(b);
    }

    // `operator==(...)`
    //
    // Tests for value-equality of two inlined vectors.
    template <typename T, size_t N, typename A>
    bool operator==(const polymer::inlined_vector<T, N, A> & a, const polymer::inlined_vector<T, N, A> & b)
    {
        auto a_data = a.data();
        auto b_data = b.data();
        return polymer::equal(a_data, a_data + a.size(), b_data, b_data + b.size());
    }

    // `operator!=(...)`
    //
    // Tests for value-inequality of two inlined vectors.
    template <typename T, size_t N, typename A>
    bool operator!=(const polymer::inlined_vector<T, N, A> & a, const polymer::inlined_vector<T, N, A> & b)
    {
        return !(a == b);
    }

    // `operator<(...)`
    //
    // Tests whether the value of an inlined vector is less than the value of
    // another inlined vector using a lexicographical comparison algorithm.
    template <typename T, size_t N, typename A>
    bool operator<(const polymer::inlined_vector<T, N, A> & a, const polymer::inlined_vector<T, N, A> & b)
    {
        auto a_data = a.data();
        auto b_data = b.data();
        return std::lexicographical_compare(a_data, a_data + a.size(), b_data, b_data + b.size());
    }

    // `operator>(...)`
    //
    // Tests whether the value of an inlined vector is greater than the value of
    // another inlined vector using a lexicographical comparison algorithm.
    template <typename T, size_t N, typename A>
    bool operator>(const polymer::inlined_vector<T, N, A> & a, const polymer::inlined_vector<T, N, A> & b)
    {
        return b < a;
    }

    // `operator<=(...)`
    //
    // Tests whether the value of an inlined vector is less than or equal to the
    // value of another inlined vector using a lexicographical comparison algorithm.
    template <typename T, size_t N, typename A>
    bool operator<=(const polymer::inlined_vector<T, N, A> & a, const polymer::inlined_vector<T, N, A> & b)
    {
        return !(b < a);
    }

    // `operator>=(...)`
    //
    // Tests whether the value of an inlined vector is greater than or equal to the
    // value of another inlined vector using a lexicographical comparison algorithm.
    template <typename T, size_t N, typename A>
    bool operator>=(const polymer::inlined_vector<T, N, A> & a, const polymer::inlined_vector<T, N, A> & b)
    {
        return !(a < b);
    }

    // Provides `polymer::Hash` support for `polymer::inlined_vector`. It is uncommon to
    // call this directly.
    template <typename H, typename T, size_t N, typename A>
    H PolyIVHashValue(H h, const polymer::inlined_vector<T, N, A> & a)
    {
        auto size = a.size();
        return H::combine(H::combine_contiguous(std::move(h), a.data(), size), size);
    }

}  // namespace polymer

#endif  // polymer_inlined_vector_hpp
