#ifndef DASH__GlobRefImpl_H_
#define DASH__GlobRefImpl_H_

#include <dash/meta/ConditionalType.h>
#include <dash/profiling/traits.h>

namespace dash {

// forward declaration
template<typename T>
class GlobAsyncRef;

template<typename T>
class GlobRefImpl;

template <class T>
class TracedGlobRef;

template <class T>
using GlobRef = typename dash::conditional_type<dash::profiling_enabled::value, TracedGlobRef<T>, GlobRefImpl<T>>::type;

} // namespace dash

#endif // DASH__GlobRefImpl_H_
