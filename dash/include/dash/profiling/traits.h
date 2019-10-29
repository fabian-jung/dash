#pragma once

namespace dash {

struct profiling_enabled {
// #ifdef PROFILING_ENABLED
 	constexpr static bool value = true;
// #else
// 	constexpr static bool value = false;
// #endif
};

}
