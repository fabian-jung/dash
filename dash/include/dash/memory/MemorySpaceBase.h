#ifndef DASH__MEMORY__MEMORY_SPACE_BASE_H__INCLUDED
#define DASH__MEMORY__MEMORY_SPACE_BASE_H__INCLUDED

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <memory>

#include <cpp17/polymorphic_allocator.h>


#include <dash/Meta.h>
#include <dash/Types.h>

#include <dash/dart/if/dart_globmem.h>
#include <dash/memory/internal/MemorySpaceRegistry.h>

namespace dash {

/// Forward declarations

template <typename ElementType, class MemorySpace>
class GlobPtr;

/**
 * The \c MemorySpace concept follows the STL \c std::pmr::memory_resource.
 *
 * Unlinke shared memory systems where process memory and virtual address
 * translation is realized as system functions and hardware components,
 * PGAS memory resources are also responsible for maintaining and propagating
 * the size and structure of their underlying local memory ranges.
 *
 * Consequently, the memory resource concept is extended by methods and
 * type definitions that are required to maintain global pointer arithmetics
 * and named Memory Space in DASH.
 *
 * STL Reference of \c std::pmr::memory_resource:
 * http://en.cppreference.com/w/cpp/memory/memory_resource
 *
 * \note
 *
 * Defining \c MemorySpace as class template seems to contradict the
 * intention to use it as polymorphic base class as subclass types of
 * \c MemorySpace<T> and \c MemorySpace<U> have incompatible polymorphic
 * base classes.
 * This looks like the kind of unintentional type incompatibility that lead
 * to the introduction of polymorphic allocators.
 * However, the template parameter specifies the memory space domain and
 * two types of \c MemorySpace are in fact incompatible if they do not refer
 * to the same domain, like global and local address space.
 *
 * \todo
 *
 * Clarify:
 * The STL memory_resource concept assumes that any type can be
 * allocated with its size specified in number of bytes so untyped
 * allocation with void pointers is practicable.
 * However, the DART allocation routines are typed for correctness
 * considerations and to optimize communication.
 * If the memory space concept complies to the STL memory_resource
 * interface, only DART_TYPE_BYTE instead of the actual value type
 * is available. This would therefore harm stability and performance.
 */

struct memory_domain_global {
};
struct memory_domain_local {
};

struct memory_space_host_tag {
};
struct memory_space_hbw_tag {
};
struct memory_space_cuda_tag {
};
struct memory_space_pmem_tag {
};

/// Allocation Policy

struct allocation_static {
  // Participating units allocate on construction. Acquired memory is never
  // reclaimed upon destruction
  //
  // Methods:
  //  - allocate
  //  - deallocate
};

struct allocation_dynamic {
  // Participating units allocate local segments independent and subsequently
  // attach it to global memory.
  //
  // Methods:
  //  - allocate_local
  //  - deallocate_local
  //  - attach
  //  - detach
};

struct memory_space_noncontiguous {
};

struct memory_space_contiguous: public memory_space_noncontiguous {
};

/// Synchronization Policy

struct synchronization_collective {
  // All allocations in memory are collective...
  //
  // => Requires that global memory allocation is always collective
};

struct synchronization_independent {
  // Units allocate global memory independent from each other. Requires a
  // synchronization mechanism to agree on a global memory state.
  // See GlobHeapMem as an example.
  //
  // => Should support both point-to-point and collective synchronization
  // within the team
};

struct synchronization_single {
  // Only a single unit participates in global memory memory allocation. It
  // may then broadcast the global pointer to other units in the team.
  //
  // => See dash::Shared as an example.
};

///////////////////////////////////////////////////////////////////////////////
// MEMORY SPACE TRAITS
///////////////////////////////////////////////////////////////////////////////
//

namespace details {

template <typename _D>
using is_local_memory_space = std::
    integral_constant<bool, std::is_same<_D, memory_domain_local>::value>;

template <typename _D>
using is_global_memory_space = std::
    integral_constant<bool, std::is_same<_D, memory_domain_global>::value>;

template <class _Ms>
using memspace_traits_is_global =
    is_global_memory_space<typename _Ms::memory_space_domain_category>;

template <class _Ms>
using memspace_traits_is_local =
    is_local_memory_space<typename _Ms::memory_space_domain_category>;

DASH__META__DEFINE_TRAIT__HAS_TYPE(void_pointer)
DASH__META__DEFINE_TRAIT__HAS_TYPE(const_void_pointer)
DASH__META__DEFINE_TRAIT__HAS_TYPE(memory_space_layout_tag)

template <class _Ms, bool = has_type_void_pointer<_Ms>::value>
struct memspace_traits_void_pointer_type {
  typedef typename _Ms::void_pointer type;
};

template <class _Ms>
struct memspace_traits_void_pointer_type<_Ms, false> {
  typedef typename std::conditional<
      memspace_traits_is_global<_Ms>::value,
      dash::GlobPtr<void, _Ms>,
      void*>::type type;
};

template <class _Ms, bool = has_type_const_void_pointer<_Ms>::value>
struct memspace_traits_const_void_pointer_type {
  typedef typename _Ms::const_void_pointer type;
};

template <class _Ms>
struct memspace_traits_const_void_pointer_type<_Ms, false> {
  typedef typename std::conditional<
      memspace_traits_is_global<_Ms>::value,
      dash::GlobPtr<const void, _Ms>,
      const void*>::type type;
};

template <class _Ms, bool = has_type_memory_space_layout_tag<_Ms>::value>
struct memspace_traits_layout_tag {
  typedef typename _Ms::memory_space_layout_tag type;
};

template <class _Ms>
struct memspace_traits_layout_tag<_Ms, false> {
  typedef memory_space_noncontiguous type;
};

}  // namespace details

template <class MemSpace>
struct memory_space_traits {
  /**
   * The Memory Space Type
   */
  using memory_space_type = MemSpace;
  /**
   * The underlying memory type (Host, CUDA, HBW, etc.)
   */
  using memory_space_type_category =
      typename MemSpace::memory_space_type_category;

  /**
   * The underlying memory domain (local, global, etc.)
   */
  using memory_space_domain_category =
      typename MemSpace::memory_space_domain_category;

  /**
   * May be contiguous or noncontiguous
   */
  using memory_space_layout_tag =
      typename details::memspace_traits_layout_tag<MemSpace>::type;

  /**
   * Whether the memory space type is specified for global address space.
   */
  using is_global = typename details::memspace_traits_is_global<MemSpace>;

  /**
   * Whether the memory space type is specified for local address space.
   * As arbitrary address space domains can be defined, this trait is not
   * equivalent to `!is_global`.
   */
  using is_local = typename details::memspace_traits_is_local<MemSpace>;

  using void_pointer =
      typename details::memspace_traits_void_pointer_type<MemSpace>::type;

  using const_void_pointer =
      typename details::memspace_traits_const_void_pointer_type<
          MemSpace>::type;
};

///////////////////////////////////////////////////////////////////////////////
// LOCAL MEMORY SPACE
///////////////////////////////////////////////////////////////////////////////

template <class MemoryType>
class LocalMemorySpaceBase : public std::pmr::memory_resource {
  // We may add something here in figure
  using base_t = std::pmr::memory_resource;

public:
  /// Memory Traits
  using memory_space_domain_category = memory_domain_local;
  using memory_space_type_category   = MemoryType;
};

///////////////////////////////////////////////////////////////////////////////
// GLOBAL MEMORY SPACE
///////////////////////////////////////////////////////////////////////////////

template <
    /// HostSpace, HBWSpace, DeviceSpace, NVMSpace
    class MemoryType>
class GlobalMemorySpaceBase {
public:
  /// Memory Traits
  using memory_space_domain_category = memory_domain_global;
  using memory_space_type_category   = MemoryType;
public:
  virtual ~GlobalMemorySpaceBase() = default;

public:
  GlobalMemorySpaceBase()                             = default;
  GlobalMemorySpaceBase(const GlobalMemorySpaceBase&) = default;
  GlobalMemorySpaceBase(GlobalMemorySpaceBase&&)      = default;
  GlobalMemorySpaceBase& operator=(const GlobalMemorySpaceBase&) = default;
  GlobalMemorySpaceBase& operator=(GlobalMemorySpaceBase&&) = default;
};

///////////////////////////////////////////////////////////////////////////////
// MEMORY SPACE
///////////////////////////////////////////////////////////////////////////////

template <class MemoryDomain, class... Args>
class MemorySpace;

// Specialization for Local Memory Spaces
template <class... Args>
class MemorySpace<memory_domain_local, Args...>
  : public LocalMemorySpaceBase<Args...> {
};

template <class... Args>
class MemorySpace<memory_domain_global, Args...>
  : public GlobalMemorySpaceBase<Args...> {
};
}  // namespace dash

#endif  // DASH__MEMORY__MEMORY_SPACE_H__INCLUDED
