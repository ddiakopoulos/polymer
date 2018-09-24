// This header file define the `polymer::any` type for holding a type-safe value
// of any type. The 'polymer::any` type is useful for providing a way to hold
// something that is, as yet, unspecified. Such unspecified types  traditionally
// are passed between API boundaries until they are later cast to their "destination"
// types. To cast to such a destination type, use `polymer::any_cast()`. Note that 
// when casting an `polymer::any`, you must cast it  to an explicit type; 
// implicit conversions will throw.

// This file was modified from the original absl::any. It removes the use of the absl
// namespace, and includes all dependent utilities from other parts of the Abseil codebase,
// making this a single-header, batteries-included drop-in for std::any. 

// Original License
// 
// Copyright 2017 The Abseil Authors.
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

#ifndef polymer_any_hpp
#define polymer_any_hpp

#ifdef __has_include
    #if __has_include(<any>) && __cplusplus >= 201703L
        #define POLYMER_HAS_STD_ANY 1
    #endif
#endif

#ifdef POLYMER_HAS_STD_ANY

#include <any>
namespace polymer
{
    using std::any;
    using std::any_cast;
    using std::bad_any_cast;
    using std::make_any;
} // namespace polymer

#else  // POLYMER_HAS_STD_ANY

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <typeinfo>
#include <utility>

#include <cstdlib>

namespace polymer
{
    ////////////////////////////////
    //   polymer::any utilities   //
    ////////////////////////////////

    template <bool B, typename T = void>
    using enable_if_t = typename std::enable_if<B, T>::type;

    template <typename T>
    using decay_t = typename std::decay<T>::type;

    struct in_place_t {};
    constexpr in_place_t in_place = {};  // via ABSL_INTERNAL_INLINE_CONSTEXPR

    template <typename T>
    struct in_place_type_t {};

    /// conjunction
    ///
    /// Performs a compile-time logical AND operation on the passed types (which
    /// must have  `::value` members convertible to `bool`. Short-circuits if it
    /// encounters any `false` members (and does not compare the `::value` members
    /// of any remaining arguments).
    ///
    /// This metafunction is designed to be a drop-in replacement for the C++17
    /// `std::conjunction` metafunction.
    template <typename... Ts>
    struct conjunction;

    template <typename T, typename... Ts>
    struct conjunction<T, Ts...> : std::conditional<T::value, conjunction<Ts...>, T>::type {};

    template <typename T>
    struct conjunction<T> : T {};

    template <>
    struct conjunction<> : std::true_type {};

    /// disjunction
    ///
    /// Performs a compile-time logical OR operation on the passed types (which
    /// must have  `::value` members convertible to `bool`. Short-circuits if it
    /// encounters any `true` members (and does not compare the `::value` members
    /// of any remaining arguments).
    ///
    /// This metafunction is designed to be a drop-in replacement for the C++17
    /// `std::disjunction` metafunction.
    template <typename... Ts>
    struct disjunction;

    template <typename T, typename... Ts>
    struct disjunction<T, Ts...> : std::conditional<T::value, T, disjunction<Ts...>>::type {};

    template <typename T>
    struct disjunction<T> : T {};

    template <>
    struct disjunction<> : std::false_type {};

    /// negation
    ///
    /// Performs a compile-time logical NOT operation on the passed type (which
    /// must have  `::value` members convertible to `bool`.
    ///
    /// This metafunction is designed to be a drop-in replacement for the C++17
    /// `std::negation` metafunction.
    template <typename T>
    struct negation : std::integral_constant<bool, !T::value> {};

    //////////////////////
    //   bad_any_cast   //
    //////////////////////

    struct bad_any_cast final : public std::bad_cast
    {
        ~bad_any_cast() override = default;
        const char * what() const noexcept override { return "bad any_cast"; }
    };

    namespace any_internal
    {
        [[noreturn]] void ThrowBadAnyCast() { throw bad_any_cast(); }
    }

}  // namespace polymer

namespace polymer
{
    namespace any_internal
    {
        template <typename Type>
        struct TypeTag
        {
            constexpr static char dummy_var = 0;
        };

        template <typename Type>
        constexpr char TypeTag<Type>::dummy_var;

        /// FastTypeId<Type>() evaluates at compile/link-time to a unique pointer for the
        /// passed in type. These are meant to be good match for keys into maps or
        /// straight up comparisons.
        template <typename Type>
        constexpr inline const void * FastTypeId() { return &TypeTag<Type>::dummy_var; }

    }  // namespace any_internal

    class any;

    /// Swaps two `polymer::any` values. Equivalent to `x.swap(y) where `x` and `y` are `polymer::any` types.
    void swap(any & x, any & y) noexcept;

    /// Constructs an `polymer::any` of type `T` with the given arguments.
    template <typename T, typename... Args>
    any make_any(Args &&... args);

    /// Overload of `polymer::make_any()` for constructing an `polymer::any` type from an  initializer list.
    template <typename T, typename U, typename... Args>
    any make_any(std::initializer_list<U> il, Args &&... args);

    /// Statically casts the value of a `const polymer::any` type to the given type.
    /// This function will throw `polymer::bad_any_cast` if the stored value type of the
    /// `polymer::any` does not match the cast.
    ///
    /// `any_cast()` can also be used to get a reference to the internal storage iff
    /// a reference type is passed as its `ValueType`:
    template <typename ValueType>
    ValueType any_cast(const any & operand);

    /// Overload of `any_cast()` to statically cast the value of a non-const
    /// `polymer::any` type to the given type. This function will throw
    /// `polymer::bad_any_cast` if the stored value type of the `polymer::any` does not
    /// match the cast.
    template <typename ValueType>
    ValueType any_cast(any & operand);  // NOLINT(runtime/references)

    /// Overload of `any_cast()` to statically cast the rvalue of an `polymer::any`
    /// type. This function will throw `polymer::bad_any_cast` if the stored value type
    /// of the `polymer::any` does not match the cast.
    template <typename ValueType>
    ValueType any_cast(any && operand);

    /// Overload of `any_cast()` to statically cast the value of a const pointer
    /// `polymer::any` type to the given pointer type, or `nullptr` if the stored value
    /// type of the `polymer::any` does not match the cast.
    template <typename ValueType>
    const ValueType * any_cast(const any * operand) noexcept;

    /// Overload of `any_cast()` to statically cast the value of a pointer
    /// `polymer::any` type to the given pointer type, or `nullptr` if the stored value
    /// type of the `polymer::any` does not match the cast.
    template <typename ValueType>
    ValueType * any_cast(any * operand) noexcept;

    //////////////////////
    //   polymer::any   //
    //////////////////////

    /// An `polymer::any` object provides the facility to either store an instance of a
    /// type, known as the "contained object", or no value. An `polymer::any` is used to
    /// store values of types that are unknown at compile time. The `polymer::any`
    /// object, when containing a value, must contain a value type; storing a
    /// reference type is neither desired nor supported.
    ///
    /// An `polymer::any` can only store a type that is copy-constructable; move-only
    /// types are not allowed within an `any` object.
    ///
    /// Note that `polymer::any` makes use of decayed types (`polymer::decay_t` in this
    /// context) to remove const-volatile qualifiers (known as "cv qualifiers"),
    /// decay functions to function pointers, etc. We essentially "decay" a given
    /// type into its essential type.
    ///
    /// `polymer::any` makes use of decayed types when determining the basic type `T` of
    /// the value to store in the any's contained object. In the documentation below,
    /// we explicitly denote this by using the phrase "a decayed type of `T`".
    ///
    /// `polymer::any` is a C++11 compatible version of the C++17 `std::any` abstraction
    /// and is designed to be a drop-in replacement for code compliant with C++17.
    class any
    {
    private:

        template <typename T>
        struct IsInPlaceType;

    public:

        // Constructors

        /// Constructs an empty `polymer::any` object (`any::has_value()` will return `false`).
        constexpr any() noexcept;

        /// Copy constructs an `polymer::any` object with a "contained object" of the
        /// passed type of `other` (or an empty `polymer::any` if `other.has_value()` is `false`.
        any(const any & other) :
            obj_(other.has_value() ? other.obj_->Clone() : std::unique_ptr<ObjInterface>()) {}

        /// Move constructs an `polymer::any` object with a "contained object" of the
        /// passed type of `other` (or an empty `polymer::any` if `other.has_value()` is `false`).
        any(any && other) noexcept = default;

        /// Constructs an `polymer::any` object with a "contained object" of the decayed
        /// type of `T`, which is initialized via `std::forward<T>(value)`.
        ///
        /// This constructor will not participate in overload resolution if the
        /// decayed type of `T` is not copy-constructible.
        template <
            typename T, typename VT = polymer::decay_t<T>,
            polymer::enable_if_t<!polymer::disjunction<
                std::is_same<any, VT>, IsInPlaceType<VT>,
                polymer::negation<std::is_copy_constructible<VT>>>::value> * = nullptr>
        any(T && value) :
            obj_(new Obj<VT>(in_place, std::forward<T>(value))) {}

        /// Constructs an `polymer::any` object with a "contained object" of the decayed
        /// type of `T`, which is initialized via `std::forward<T>(value)`.
        template <typename T, typename... Args, typename VT = polymer::decay_t<T>,
                  polymer::enable_if_t<polymer::conjunction<
                      std::is_copy_constructible<VT>,
                      std::is_constructible<VT, Args...>>::value> * = nullptr>
        explicit any(in_place_type_t<T> /*tag*/, Args &&... args) :
            obj_(new Obj<VT>(in_place, std::forward<Args>(args)...)) {}

        /// Constructs an `polymer::any` object with a "contained object" of the passed
        /// type `VT` as a decayed type of `T`. `VT` is initialized as if
        /// direct-non-list-initializing an object of type `VT` with the arguments
        /// `initializer_list, std::forward<Args>(args)...`.
        template <
            typename T, typename U, typename... Args, typename VT = polymer::decay_t<T>,
            polymer::enable_if_t<
                polymer::conjunction<std::is_copy_constructible<VT>,
                                     std::is_constructible<VT, std::initializer_list<U> &,
                                                           Args...>>::value> * = nullptr>
        explicit any(in_place_type_t<T> /*tag*/, std::initializer_list<U> ilist,
                     Args &&... args) :
            obj_(new Obj<VT>(in_place, ilist, std::forward<Args>(args)...)) {}

        // Assignment operators

        /// Copy assigns an `polymer::any` object with a "contained object" of the passed type.
        any & operator=(const any & rhs)
        {
            any(rhs).swap(*this);
            return *this;
        }

        /// Move assigns an `polymer::any` object with a "contained object" of the
        /// passed type. `rhs` is left in a valid but otherwise unspecified state.
        any & operator=(any && rhs) noexcept
        {
            any(std::move(rhs)).swap(*this);
            return *this;
        }

        /// Assigns an `polymer::any` object with a "contained object" of the passed type.
        template <typename T, typename VT = polymer::decay_t<T>,
                  polymer::enable_if_t<polymer::conjunction<
                      polymer::negation<std::is_same<VT, any>>,
                      std::is_copy_constructible<VT>>::value> * = nullptr>
        any & operator=(T && rhs)
        {
            any tmp(in_place_type_t<VT>(), std::forward<T>(rhs));
            tmp.swap(*this);
            return *this;
        }

        // Modifiers

        /// Emplaces a value within an `polymer::any` object by calling `any::reset()`,
        /// initializing the contained value as if direct-non-list-initializing an
        /// object of type `VT` with the arguments `std::forward<Args>(args)...`, and
        /// returning a reference to the new contained value.
        ///
        /// Note: If an exception is thrown during the call to `VT`'s constructor,
        /// `*this` does not contain a value, and any previously contained value has
        /// been destroyed.
        template <
            typename T, typename... Args, typename VT = polymer::decay_t<T>,
            polymer::enable_if_t<std::is_copy_constructible<VT>::value && std::is_constructible<VT, Args...>::value> * = nullptr>
        VT & emplace(Args &&... args)
        {
            reset();  // NOTE: reset() is required here even in the world of exceptions.
            Obj<VT> * const object_ptr = new Obj<VT>(in_place, std::forward<Args>(args)...);
            obj_ = std::unique_ptr<ObjInterface>(object_ptr);
            return object_ptr->value;
        }

        /// Overload of `any::emplace()` to emplace a value within an `polymer::any`
        /// object by calling `any::reset()`, initializing the contained value as if
        /// direct-non-list-initializing an object of type `VT` with the arguments
        /// `initializer_list, std::forward<Args>(args)...`, and returning a reference
        /// to the new contained value.
        ///
        /// Note: If an exception is thrown during the call to `VT`'s constructor,
        /// `*this` does not contain a value, and any previously contained value has
        /// been destroyed. The function shall not participate in overload resolution
        /// unless `is_copy_constructible_v<VT>` is `true` and
        /// `is_constructible_v<VT, initializer_list<U>&, Args...>` is `true`.
        template <
            typename T, typename U, typename... Args, typename VT = polymer::decay_t<T>,
            polymer::enable_if_t<std::is_copy_constructible<VT>::value && std::is_constructible<VT, std::initializer_list<U> &, Args...>::value> * = nullptr>
        VT & emplace(std::initializer_list<U> ilist, Args &&... args)
        {
            reset();  // NOTE: reset() is required here even in the world of exceptions.
            Obj<VT> * const object_ptr = new Obj<VT>(in_place, ilist, std::forward<Args>(args)...);
            obj_ = std::unique_ptr<ObjInterface>(object_ptr);
            return object_ptr->value;
        }

        /// Resets the state of the `polymer::any` object, destroying the contained object if present.
        void reset() noexcept { obj_ = nullptr; }

        /// Swaps the passed value and the value of this `polymer::any` object.
        void swap(any & other) noexcept { obj_.swap(other.obj_); }

        // Observers

        /// Returns `true` if the `any` object has a contained value, otherwise returns `false`.
        bool has_value() const noexcept { return obj_ != nullptr; }

        /// Returns: typeid(T) if *this has a contained object of type T, otherwise typeid(void).
        const std::type_info & type() const noexcept
        {
            if (has_value()) return obj_->Type();
            return typeid(void);
        }

    private:

        // Tagged type-erased abstraction for holding a cloneable object.
        struct ObjInterface
        {
            virtual ~ObjInterface() = default;
            virtual std::unique_ptr<ObjInterface> Clone() const = 0;
            virtual const void * ObjTypeId() const noexcept = 0;
            virtual const std::type_info & Type() const noexcept = 0;
        };

        // Hold a value of some queryable type, with an ability to Clone it.
        template <typename T>
        struct Obj : public ObjInterface
        {
            template <typename... Args>
            explicit Obj(in_place_t /*tag*/, Args &&... args) : value(std::forward<Args>(args)...) {}
            std::unique_ptr<ObjInterface> Clone() const final { return std::unique_ptr<ObjInterface>(new Obj(in_place, value)); }
            const void * ObjTypeId() const noexcept final { return IdForType<T>(); }
            const std::type_info & Type() const noexcept final { return typeid(T); }
            T value;
        };

        std::unique_ptr<ObjInterface> CloneObj() const
        {
            if (!obj_) return nullptr;
            return obj_->Clone();
        }

        template <typename T>
        constexpr static const void * IdForType()
        {
            // Note: This type dance is to make the behavior consistent with typeid.
            using NormalizedType = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
            return any_internal::FastTypeId<NormalizedType>();
        }

        const void * GetObjTypeId() const
        {
            return obj_ ? obj_->ObjTypeId() : any_internal::FastTypeId<void>();
        }

        // `polymer::any` nonmember functions

        template <typename ValueType>
        friend ValueType any_cast(const any & operand);

        template <typename ValueType>
        friend ValueType any_cast(any & operand);

        template <typename T>
        friend const T * any_cast(const any * operand) noexcept;

        template <typename T>
        friend T * any_cast(any * operand) noexcept;

        std::unique_ptr<ObjInterface> obj_;
    };

    ////////////////////////////////
    //   Implementation Details   //
    ////////////////////////////////

    constexpr any::any() noexcept = default;

    template <typename T>
    struct any::IsInPlaceType : std::false_type {};

    template <typename T>
    struct any::IsInPlaceType<in_place_type_t<T>> : std::true_type {};

    inline void swap(any & x, any & y) noexcept { x.swap(y); }

    template <typename T, typename... Args>
    any make_any(Args &&... args)
    {
        return any(in_place_type_t<T>(), std::forward<Args>(args)...);
    }

    template <typename T, typename U, typename... Args>
    any make_any(std::initializer_list<U> il, Args &&... args)
    {
        return any(in_place_type_t<T>(), il, std::forward<Args>(args)...);
    }

    template <typename ValueType>
    ValueType any_cast(const any & operand)
    {
        using U = typename std::remove_cv<
            typename std::remove_reference<ValueType>::type>::type;
        static_assert(std::is_constructible<ValueType, const U &>::value,
                      "Invalid ValueType");
        auto * const result = (any_cast<U>) (&operand);
        if (result == nullptr)
        {
            any_internal::ThrowBadAnyCast();
        }
        return static_cast<ValueType>(*result);
    }

    template <typename ValueType>
    ValueType any_cast(any & operand)
    {  
        using U = typename std::remove_cv<typename std::remove_reference<ValueType>::type>::type;
        static_assert(std::is_constructible<ValueType, U &>::value, "Invalid ValueType");

        auto * result = (any_cast<U>) (&operand);
        if (result == nullptr)
        {
            any_internal::ThrowBadAnyCast();
        }

        return static_cast<ValueType>(*result);
    }

    template <typename ValueType>
    ValueType any_cast(any && operand)
    {
        using U = typename std::remove_cv<typename std::remove_reference<ValueType>::type>::type;
        static_assert(std::is_constructible<ValueType, U>::value, "Invalid ValueType");
        return static_cast<ValueType>(std::move((any_cast<U &>) (operand)));
    }

    template <typename T>
    const T * any_cast(const any * operand) noexcept
    {
        return operand && operand->GetObjTypeId() == 
            any::IdForType<T>() ? std::addressof(static_cast<const any::Obj<T> *>(operand->obj_.get())->value) : nullptr;
    }

    template <typename T>
    T * any_cast(any * operand) noexcept
    {
        return operand && operand->GetObjTypeId() == 
            any::IdForType<T>() ? std::addressof(static_cast<any::Obj<T> *>(operand->obj_.get())->value) : nullptr;
    }

}  // end namespace polymer

#endif  // POLYMER_HAS_STD_ANY

#endif  // polymer_any_hpp
