#pragma once

#ifndef polymer_engine_hpp
#define polymer_engine_hpp

#include "polymer-engine/asset/asset-handle.hpp"
#include "polymer-engine/asset/asset-handle-utils.hpp"
#include "polymer-engine/asset/asset-resolver.hpp"

#include "polymer-engine/ecs/core-ecs.hpp"
#include "polymer-engine/ecs/core-events.hpp"
#include "polymer-engine/ecs/component-pool.hpp"
#include "polymer-engine/ecs/typeid.hpp"

#include "polymer-engine/system/system-identifier.hpp"
#include "polymer-engine/system/system-transform.hpp"
#include "polymer-engine/system/system-collision.hpp"
#include "polymer-engine/system/system-render.hpp"

#include "polymer-engine/renderer/renderer-pbr.hpp"
#include "polymer-engine/renderer/renderer-debug.hpp"
#include "polymer-engine/renderer/renderer-util.hpp"

#include "profiling.hpp"
#include "logging.hpp"
#include "shader.hpp"
#include "material.hpp"
#include "shader-library.hpp"
#include "material-library.hpp"

#endif // end polymer_engine_hpp
