#pragma once

#ifndef polymer_engine_hpp
#define polymer_engine_hpp

#include "profiling.hpp"
#include "logging.hpp"
#include "asset-handle.hpp"
#include "asset-handle-utils.hpp"
#include "asset-resolver.hpp"
#include "material.hpp"
#include "material-library.hpp"
#include "shader.hpp"
#include "shader-library.hpp"

#include "renderer-standard.hpp"
#include "renderer-debug.hpp"

#include "openvr-hmd.hpp"
#include "openvr-camera.hpp"

#include "ecs/component-pool.hpp"
#include "ecs/core-ecs.hpp"
#include "ecs/core-events.hpp"
#include "ecs/system-name.hpp"
#include "ecs/system-transform.hpp"
#include "ecs/typeid.hpp"

#endif // end polymer_engine_hpp
