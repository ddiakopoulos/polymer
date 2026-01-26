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
