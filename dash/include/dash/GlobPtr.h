#ifndef DASH__GLOB_PTR_H_
#define DASH__GLOB_PTR_H_

#include <dash/meta/ConditionalType.h>
#include <dash/profiling/traits.h>

namespace dash {

template <class T, class MemSpaceT>
class GlobPtrImpl;

template <class T, class MemSpaceT>
class TracedGlobPtr;

template <class T, class MemSpaceT>
using GlobPtr = typename conditional_type<dash::profiling_enabled::value, TracedGlobPtr<T, MemSpaceT>, GlobPtrImpl<T, MemSpaceT>>::type;

}

// #include <dash/GlobPtrImpl.hpp>

#endif  // DASH__GLOB_PTR_H_
