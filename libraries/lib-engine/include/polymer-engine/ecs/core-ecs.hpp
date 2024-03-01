/*
 * Based on: https://github.com/google/lullaby/tree/master/lullaby/modules/ecs
 * Apache 2.0 License. Copyright 2017 Google Inc. All Rights Reserved.
 * See LICENSE file for full attribution information.
 */

#pragma once

#ifndef polymer_base_ecs_hpp
#define polymer_base_ecs_hpp

#include "polymer-engine/ecs/typeid.hpp"
#include "polymer-core/util/guid.hpp"

namespace polymer
{
    ////////////////
    //   entity   //
    ////////////////

    using entity = guid;
    const entity kInvalidEntity = guid(std::array<uint8_t, 16>{0000000000000000});

} // end namespace polymer

#endif // polymer_base_ecs_hpp
