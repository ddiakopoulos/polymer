#pragma once

#ifndef serialization_hpp
#define serialization_hpp

#include <string>
#include "math-core.hpp"
#include "json.hpp"

// Variadic unpacking of the metadata in the style of sgorsten. The tricky bit is the use of SFINAE with `enable_if_t.
// The compiler will keep bailing out until requested type matches the type in the parameter pack, by either of the two other templates below 
// This is loosely inspired by `findArg` found here (https://github.com/WyattTechnology/Wyatt-STM/blob/master/wstm/find_arg.h) (BSD-3),
// but using pointers instead of default-constructed objects.
template<class T, class A, class... O>
std::enable_if_t<!std::is_same<T, A>::value, const T *> unpack(const A & first, const O & ... others)
{
    // Recursively resolve piece of metadata until `others...` exhausted
    return unpack<T>(others...);
}

// Resolves the metadata when provided with a parameter pack. In the case of a single piece of metadata, this is the target.
template<class T, class... O>
const T * unpack(const T & meta, const O & ... others) { return &meta; }

// Base template to that is resolved when there's no metadata
template<class T>
const T * unpack() { return nullptr; }

template<class T> struct range_metadata { T min, max; };
struct editor_hidden {};
struct input_field {};
struct serializer_hidden {};
struct entity_ref {};
struct tagged_set { std::string tag; };

using json = nlohmann::json;

#endif // end serialization_hpp