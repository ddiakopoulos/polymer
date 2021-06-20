#pragma once

#ifndef lib_polymer_hpp
#define lib_polymer_hpp

#include "polymer-core/math/math-core.hpp"

#include "polymer-core/util/util.hpp"
#include "polymer-core/util/string-utils.hpp"
#include "polymer-core/util/simple-timer.hpp"
#include "polymer-core/util/procedural-mesh.hpp"
#include "polymer-core/util/memory-pool.hpp"
#include "polymer-core/util/image-buffer.hpp"
#include "polymer-core/util/geometry.hpp"
#include "polymer-core/util/file-io.hpp"
#include "polymer-core/util/camera.hpp"
#include "polymer-core/util/bit-mask.hpp"
#include "polymer-core/util/arcball.hpp"
#include "polymer-core/util/property.hpp"
#include "polymer-core/util/thread-pool.hpp"
#include "polymer-core/util/guid.hpp"
#include "polymer-core/util/timestamp.hpp"

#include "polymer-core/tools/simple-animator.hpp"
#include "polymer-core/tools/trajectory.hpp"
#include "polymer-core/tools/splines.hpp"
#include "polymer-core/tools/simplex-noise.hpp"
#include "polymer-core/tools/radix-sort.hpp"
#include "polymer-core/tools/quick-hull.hpp"
#include "polymer-core/tools/poisson-disk.hpp"
#include "polymer-core/tools/parallel-transport-frames.hpp"
#include "polymer-core/tools/parabolic-pointer.hpp"
#include "polymer-core/tools/one-euro.hpp"
#include "polymer-core/tools/octree.hpp"
#include "polymer-core/tools/movement-tracker.hpp"
#include "polymer-core/tools/algo-misc.hpp"
#include "polymer-core/tools/bvh.hpp"
#include "polymer-core/tools/oriented-bounding-box.hpp"
#include "polymer-core/tools/polynomial-solvers.hpp"
#include "polymer-core/tools/colormap.hpp"

#include "polymer-core/queues/queue-spsc-bounded.hpp"
#include "polymer-core/queues/queue-spsc.hpp"
#include "polymer-core/queues/queue-mpsc-bounded.hpp"
#include "polymer-core/queues/queue-mpsc.hpp"
#include "polymer-core/queues/queue-mpmc-bounded.hpp"
#include "polymer-core/queues/queue-mpmc-blocking.hpp"
#include "polymer-core/queues/queue-circular.hpp"

#endif // end lib_polymer_hpp
