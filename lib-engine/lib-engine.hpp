#pragma once

#ifndef polymer_engine_hpp
#define polymer_engine_hpp

#include "profiling.hpp"
#include "logging.hpp"

#include "asset-handle.hpp"
#include "asset-handle-utils.hpp"
#include "asset-resolver.hpp"

#include "shader.hpp"
#include "material.hpp"
#include "shader-library.hpp"
#include "material-library.hpp"

#include "openvr-hmd.hpp"
#include "openvr-camera.hpp"
#include "xr-interaction.hpp"

#include "ecs/core-ecs.hpp"
#include "ecs/core-events.hpp"
#include "ecs/component-pool.hpp"
#include "ecs/typeid.hpp"

#include "system-identifier.hpp"
#include "system-transform.hpp"
#include "system-collision.hpp"
#include "system-render.hpp"

#include "renderer-pbr.hpp"
#include "renderer-debug.hpp"
#include "renderer-util.hpp"

#include "gl-api.hpp"
#include "gl-async-gpu-timer.hpp"
#include "gl-camera-utils.hpp"
#include "gl-gizmo.hpp"
#include "gl-imgui.hpp"
#include "gl-loaders.hpp"
#include "gl-mesh-util.hpp"
#include "gl-nvg.hpp"
#include "gl-procedural-mesh.hpp"
#include "gl-procedural-sky.hpp"
#include "gl-renderable-grid.hpp"
#include "gl-renderable-meshline.hpp"
#include "gl-texture-view.hpp"

#include "camera-controllers.hpp"
#include "glfw-app.hpp"

#endif // end polymer_engine_hpp
