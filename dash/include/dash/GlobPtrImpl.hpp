#ifndef DASH__GLOB_PTR_IMPL_H_
#define DASH__GLOB_PTR_IMPL_H_

#include <cstddef>
#include <iostream>
#include <sstream>

#include <dash/dart/if/dart.h>

#include <dash/Exception.h>
#include <dash/Init.h>

#include <dash/atomic/Type_traits.h>

#include <dash/internal/Logging.h>
#include <dash/iterator/internal/GlobPtrBase.h>

// #include <dash/memory/MemorySpaceBase.h>

#include <dash/profiling/TracedPointer.h>

std::ostream &operator<<(std::ostream &os, const dart_gptr_t &dartptr);

bool operator==(const dart_gptr_t &lhs, const dart_gptr_t &rhs);

bool operator!=(const dart_gptr_t &lhs, const dart_gptr_t &rhs);

namespace dash {

  // Forward-declarations
template <typename T>
class GlobRefImpl;

template <class T, class MemSpaceT>
class GlobPtrImpl;

template <class T, class MemSpaceT>
dash::gptrdiff_t distance(
    GlobPtrImpl<T, MemSpaceT> gbegin, GlobPtrImpl<T, MemSpaceT> gend);

/**
 * Pointer in global memory space with random access arithmetics.
 *
 * \see GlobIter
 *
 * \concept{DashMemorySpaceConcept}
 */
template <typename ElementType, class GlobMemT>
class GlobPtrImpl {
private:
  typedef GlobPtrImpl<ElementType, GlobMemT> self_t;

  using local_pointer_traits =
      std::pointer_traits<typename GlobMemT::local_void_pointer>;

  using memory_traits = dash::memory_space_traits<GlobMemT>;

public:
  typedef ElementType                          value_type;
  typedef GlobPtrImpl<const ElementType, GlobMemT> const_type;
  typedef typename GlobMemT::index_type        index_type;
  typedef typename GlobMemT::size_type         size_type;
  typedef index_type                           gptrdiff_t;

  typedef typename local_pointer_traits::template rebind<value_type>
      local_type;

  typedef typename local_pointer_traits::template rebind<value_type const>
      const_local_type;

  typedef GlobMemT memory_type;

  /**
   * Rebind to a different type of pointer
   */
  template <typename T>
  using rebind = dash::GlobPtrImpl<T, GlobMemT>;

public:
  template <typename T, class MemSpaceT>
  friend class GlobPtrImpl;

  template <typename T, class MemSpaceT>
  friend std::ostream &operator<<(
      std::ostream &os, const GlobPtrImpl<T, MemSpaceT> &gptr);

  template <class T, class MemSpaceT>
  friend dash::gptrdiff_t distance(
      GlobPtrImpl<T, MemSpaceT> gbegin, GlobPtrImpl<T, MemSpaceT> gend);

private:
  // Raw global pointer used to initialize this pointer instance
  dart_gptr_t m_dart_pointer = DART_GPTR_NULL;
public:
  /**
   * Default constructor, underlying global address is unspecified.
   */
  constexpr GlobPtrImpl() = default;

  /**
   * Constructor, specifies underlying global address.
   */
  explicit constexpr GlobPtrImpl(dart_gptr_t gptr)
    : m_dart_pointer(gptr)
  {
  }

  /**
   * Constructor for conversion of std::nullptr_t.
   */
  explicit constexpr GlobPtrImpl(std::nullptr_t)
    : m_dart_pointer(DART_GPTR_NULL)
  {
  }

  /**
   * Copy constructor.
   */
  constexpr GlobPtrImpl(const self_t &other) = default;

  /**
   * Assignment operator.
   */
  self_t &operator=(const self_t &rhs) = default;

  //clang-format off
  /**
   * Implicit Converting Constructor, only allowed if one of the following
   * conditions is satisfied:
   *   - Either From or To (value_type) are void. Like in C we can convert any
   *     pointer type to a void pointer and back again.
   *   - From::value_type& is assignable to value_type&.
   *
   * NOTE: Const correctness is considered. We can assign a GlobPtrImpl<const T>
   *       to a GlobPtrImpl<T> but not the other way around
   */
  //clang-format on
  template <
      typename From,
      typename = typename std::enable_if<
          // We always allow GlobPtrImpl<T> -> GlobPtrImpl<void> or the other way)
          // or if From is assignable to To (value_type)
          dash::internal::is_pointer_assignable<
              typename dash::remove_atomic<From>::type,
              typename dash::remove_atomic<value_type>::type>::value>

      ::type>
  constexpr GlobPtrImpl(const GlobPtrImpl<From, GlobMemT> &other)
    : m_dart_pointer(other.m_dart_pointer)
  {
  }

  /**
   * Move constructor.
   */
  constexpr GlobPtrImpl(self_t &&other) = default;

  /**
   * Move-assignment operator.
   */
  self_t &operator=(self_t &&rhs) = default;

  /**
   * Converts pointer to its referenced native pointer or
   * \c nullptr if the \c GlobPtrImpl does not point to a local
   * address.
   */
  explicit operator value_type *() const noexcept
  {
    return local();
  }

  /**
   * Conversion operator to local pointer.
   *
   * \returns  A native pointer to the local element referenced by this
   *           GlobPtrImpl instance, or \c nullptr if the referenced element
   *           is not local to the calling unit.
   */
  explicit operator value_type *() noexcept
  {
    return local();
  }

  /**
   * Converts pointer to its underlying global address.
   */
  explicit constexpr operator dart_gptr_t() const noexcept
  {
    return m_dart_pointer;
  }

  /**
   * The pointer's underlying global address.
   */
  constexpr dart_gptr_t dart_gptr() const noexcept
  {
    return m_dart_pointer;
  }

  /**
   * Pointer offset difference operator.
   */
  constexpr index_type operator-(const self_t &rhs) const noexcept
  {
    return dash::distance(rhs, *this);
  }

  /**
   * Prefix increment operator.
   */
  self_t &operator++()
  {
    increment(1);
    return *this;
  }

  /**
   * Postfix increment operator.
   */
  self_t operator++(int)
  {
    self_t res(*this);
    increment(1);
    return res;
  }

  /**
   * Pointer increment operator.
   */
  self_t operator+(index_type n) const
  {
    self_t res(*this);
    res += n;
    return res;
  }

  /**
   * Pointer increment operator.
   */
  self_t &operator+=(index_type n)
  {
    if (n >= 0) {
      increment(n);
    }
    else {
      decrement(-n);
    }
    return *this;
  }

  /**
   * Prefix decrement operator.
   */
  self_t &operator--()
  {
    decrement(1);
    return *this;
  }

  /**
   * Postfix decrement operator.
   */
  self_t operator--(int)
  {
    self_t res(*this);
    decrement(1);
    return res;
  }

  /**
   * Pointer decrement operator.
   */
  self_t operator-(index_type n) const
  {
    self_t res(*this);
    res -= n;
    return res;
  }

  /**
   * Pointer decrement operator.
   */
  self_t &operator-=(index_type n)
  {
    if (n >= 0) {
      decrement(n);
    }
    else {
      increment(-n);
    }
    return *this;
  }

  /**
   * Equality comparison operator.
   */
  // template <class GlobPtrImplT>
  constexpr bool operator==(const GlobPtrImpl &other) const noexcept
  {
    return DART_GPTR_EQUAL(
        static_cast<dart_gptr_t>(m_dart_pointer),
        static_cast<dart_gptr_t>(other.m_dart_pointer));
  }

  /**
   * Equality comparison operator.
   */
  constexpr bool operator==(std::nullptr_t) const noexcept
  {
    return DART_GPTR_ISNULL(static_cast<dart_gptr_t>(m_dart_pointer));
  }

  /**
   * Inequality comparison operator.
   */
  template <class GlobPtrT>
  constexpr bool operator!=(const GlobPtrT &other) const noexcept
  {
    return !(*this == other);
  }

  /**
   * Less comparison operator.
   */
  template <class GlobPtrT>
  constexpr bool operator<(const GlobPtrT &other) const noexcept
  {
    return (other - *this) > 0;
  }

  /**
   * Less-equal comparison operator.
   */
  template <class GlobPtrT>
  constexpr bool operator<=(const GlobPtrT &other) const noexcept
  {
    return !(*this > other);
  }

  /**
   * Greater comparison operator.
   */
  template <class GlobPtrT>
  constexpr bool operator>(const GlobPtrT &other) const noexcept
  {
    return (*this - other) > 0;
  }

  /**
   * Greater-equal comparison operator.
   */
  template <class GlobPtrT>
  constexpr bool operator>=(const GlobPtrT &other) const noexcept
  {
    return (*this - other) >= 0;
  }

  /**
   * Subscript operator.
   */
  constexpr GlobRefImpl<const value_type> operator[](gptrdiff_t n) const
  {
    return GlobRefImpl<const value_type>(self_t((*this) + n));
  }

  /**
   * Subscript assignment operator.
   */
    GlobRefImpl<value_type> operator[](gptrdiff_t n)
  {
    return GlobRefImpl<value_type>(self_t((*this) + n));
  }

  /**
   * Dereference operator.
   */
    GlobRefImpl<value_type> operator*()
  {
    return GlobRefImpl<value_type>(*this);
  }

  /**
   * Dereference operator.
   */
  constexpr GlobRefImpl<const value_type> operator*() const
  {
    return GlobRefImpl<const value_type>(*this);
  }

  /**
   * Conversion to local pointer.
   *
   * \returns  A native pointer to the local element referenced by this
   *           GlobPtr instance, or \c nullptr if the referenced element
   *           is not local to the calling unit.
   */
  value_type *local()
  {
    void *addr = nullptr;
    if (dart_gptr_getaddr(m_dart_pointer, &addr) == DART_OK) {
      return static_cast<value_type *>(addr);
    }
    return nullptr;
  }

  /**
   * Conversion to local const pointer.
   *
   * \returns  A native pointer to the local element referenced by this
   *           GlobPtr instance, or \c nullptr if the referenced element
   *           is not local to the calling unit.
   */
  const value_type * local() const {
    void *addr = nullptr;
    if (dart_gptr_getaddr(m_dart_pointer, &addr) == DART_OK) {
      return static_cast<const value_type *>(addr);
    }
    return nullptr;
  }

  /**
   * Set the global pointer's associated unit.
   */
  void set_unit(team_unit_t unit_id)
  {
    m_dart_pointer.unitid = unit_id.id;
  }

  /**
   * Check whether the global pointer is in the local
   * address space the pointer's associated unit.
   */
  bool is_local() const
  {
    return dash::internal::is_local(m_dart_pointer);
  }

  constexpr explicit operator bool() const noexcept
  {
    return !DART_GPTR_ISNULL(m_dart_pointer);
  }

private:
  void increment(size_type offs)
  {
    if (offs == 0) {
      return;
    }
    using memory_space_traits = dash::memory_space_traits<GlobMemT>;

    if (DART_GPTR_ISNULL(m_dart_pointer)) {
      DASH_LOG_ERROR(
          "GlobPtr.increment(offs)",
          "cannot increment a global null pointer");
    }

    auto& reg = dash::internal::MemorySpaceRegistry::GetInstance();
    auto const * mem_space = static_cast<const GlobMemT *>(reg.lookup(m_dart_pointer));
    // get a new dart with the requested offset
    auto const newPtr = dash::internal::increment<value_type>(
        m_dart_pointer,
        offs,
        mem_space,
        typename memory_space_traits::memory_space_layout_tag{});

    DASH_ASSERT(!DART_GPTR_ISNULL(newPtr));

    m_dart_pointer = newPtr;
  }

  void decrement(size_type offs)
  {
    if (offs == 0) {
      return;
    }

    using memory_space_traits = dash::memory_space_traits<GlobMemT>;

    if (DART_GPTR_ISNULL(m_dart_pointer)) {
      DASH_LOG_ERROR(
          "GlobPtr.increment(offs)",
          "cannot decrement a global null pointer");
    }

    auto &      reg = dash::internal::MemorySpaceRegistry::GetInstance();
    auto const *mem_space =
        static_cast<const GlobMemT *>(reg.lookup(m_dart_pointer));

    // get a new dart with the requested offset
    auto const newPtr = dash::internal::decrement<value_type>(
        m_dart_pointer,
        offs,
        mem_space,
        typename memory_space_traits::memory_space_layout_tag{});

    DASH_ASSERT(!DART_GPTR_ISNULL(newPtr));

    m_dart_pointer = newPtr;
  }
};

template <typename T, class MemSpaceT>
std::ostream &operator<<(std::ostream &os, const GlobPtrImpl<T, MemSpaceT> &gptr)
{
  std::ostringstream ss;
  char               buf[100];
  sprintf(
      buf,
      "u%06X|f%02X|s%04X|t%04X|o%016lX",
      gptr.m_dart_pointer.unitid,
      gptr.m_dart_pointer.flags,
      gptr.m_dart_pointer.segid,
      gptr.m_dart_pointer.teamid,
      gptr.m_dart_pointer.addr_or_offs.offset);
  ss << "dash::GlobPtr<" << typeid(T).name() << ">(" << buf << ")";
  return operator<<(os, ss.str());
}

/**
 *
 * Returns the number of hops from gbegin to gend.
 *
 * Equivalent to \c (gend - gbegin).
 *
 * \note
 * The distance is calculate by considering the memory traits of the
 * underlying global memory distances.
 *
 *
 * \return  The number of increments to go from gbegin to gend
 *
 * \concept{DashMemorySpaceConcept}
 */
template <class T, class MemSpaceT>
dash::gptrdiff_t distance(
    // First global pointer in range
    GlobPtrImpl<T, MemSpaceT> gbegin,
    // Final global pointer in range
    GlobPtrImpl<T, MemSpaceT> gend)
{
  using memory_space_traits = dash::memory_space_traits<MemSpaceT>;

  auto const begin = static_cast<dart_gptr_t>(gbegin);
  auto const end = static_cast<dart_gptr_t>(gbegin);

  DASH_ASSERT_EQ(begin.teamid, end.teamid, "teamid must be equal");
  DASH_ASSERT_EQ(begin.segid, end.segid, "segid must be equal");

  auto &      reg = dash::internal::MemorySpaceRegistry::GetInstance();
  auto const *mem_space =
      static_cast<const MemSpaceT *>(reg.lookup(begin));

  return dash::internal::distance<T>(
      static_cast<dart_gptr_t>(gbegin),
      static_cast<dart_gptr_t>(gend),
      mem_space,
      typename memory_space_traits::memory_space_layout_tag{});
}

/**
 * Returns the begin of the local memory portion within a global memory
 * segment.
 *
 * @param     global_begin The pointer which identifies the global memory
 *            segment
 * @param     unit The unit_id whose local memory portion is required. This
 *            must be a unit which is running locally, i.e. the local memory
 *            portion must be reachable with a native pointer (void*)
 * \return    returns the local memory portion identified by a global memory
 *            segment.
 *
 * \concept{DashMemorySpaceConcept}
 */
template <class T, class MemSpaceT>
DASH_CONSTEXPR inline typename std::enable_if<
    std::is_same<
        typename dash::memory_space_traits<
            MemSpaceT>::memory_space_layout_tag,
        memory_space_contiguous>::value,
    T>::type *
local_begin(GlobPtrImpl<T, MemSpaceT> global_begin, dash::team_unit_t unit)
    DASH_NOEXCEPT
{
  // reset offset to 0
  auto dart_gptr                = static_cast<dart_gptr_t>(global_begin);
  dart_gptr.unitid              = unit.id;
  dart_gptr.addr_or_offs.offset = 0;
  return GlobPtrImpl<T, MemSpaceT>(dart_gptr).local();
}

template <class T, class MemSpaceT>
class TracedGlobPtr;

// TODO enable / disable tracing
template <class T, class MemSpaceT>
class TracedGlobPtr : public TracedPointer<GlobPtrImpl<T, MemSpaceT>> {
	using base = GlobPtrImpl<T, MemSpaceT>;
public:
	using gptrdiff_t = typename GlobPtrImpl<T, MemSpaceT>::gptrdiff_t;
	using index_type = typename GlobPtrImpl<T, MemSpaceT>::index_type;
	using value_type = typename GlobPtrImpl<T, MemSpaceT>::value_type;

	template <typename Y>
	using rebind = dash::GlobPtrImpl<Y, MemSpaceT>;

	TracedGlobPtr() :
		TracedPointer<GlobPtrImpl<T, MemSpaceT>>()
	{}

	TracedGlobPtr(const TracedPointer<GlobPtrImpl<std::remove_const_t<T>, MemSpaceT>>& conv) :
		TracedPointer<GlobPtrImpl<T, MemSpaceT>>(conv)
	{}

	TracedGlobPtr(const GlobPtrImpl<T, MemSpaceT>& conv) :
		TracedPointer<GlobPtrImpl<T, MemSpaceT>>(conv)
	{}

	TracedGlobPtr(const dart_gptr_t& gptr) :
		TracedPointer<GlobPtrImpl<T, MemSpaceT>>(GlobPtrImpl<T, MemSpaceT>(gptr))
	{}

	template <
		typename From,
		typename = decltype(TracedPointer<GlobPtrImpl<T, MemSpaceT>>(std::declval<GlobPtrImpl<From, MemSpaceT>>()))
		/*typename std::enable_if<
			// We always allow GlobPtrImpl<T> -> GlobPtrImpl<void> or the other way)
			// or if From is assignable to To (value_type)
			dash::internal::is_pointer_assignable<
				typename dash::remove_atomic<From>::type,
				typename dash::remove_atomic<value_type>::type>::value>
		::type>*/
	>
	constexpr TracedGlobPtr(const GlobPtrImpl<From, MemSpaceT>& conv) :
		TracedPointer<GlobPtrImpl<T, MemSpaceT>>(conv)
	{
	}

	template <
		typename From,
		typename = decltype(TracedPointer<GlobPtrImpl<T, MemSpaceT>>(std::declval<GlobPtrImpl<From, MemSpaceT>>()))
// 		typename = typename std::enable_if<
// 			// We always allow GlobPtrImpl<T> -> GlobPtrImpl<void> or the other way)
// 			// or if From is assignable to To (value_type)
// 			dash::internal::is_pointer_assignable<
// 				typename dash::remove_atomic<From>::type,
// 				typename dash::remove_atomic<value_type>::type>::value>
//
// 		::type
	>
	constexpr TracedGlobPtr(const GlobPtr<From, MemSpaceT>& conv) :
		TracedPointer<GlobPtrImpl<T, MemSpaceT>>(conv)
	{
	}

	void set_unit(team_unit_t unit_id) {
		this->traced_call(&base::set_unit, unit_id);
	}

	dart_gptr_t dart_gptr() const {
		return this->traced_call(&base::dart_gptr);
	}

	value_type *local() {
		return this->traced_call(static_cast<value_type* (base::*)()>(&base::local));
	}

	const value_type * local() const {
		return this->traced_call(static_cast<value_type* (base::*)() const>(&base::local));
	}

	operator dart_gptr_t () const {
		// after converting to dart_gptr_t all tracing is impossible
		return this->traced_call(&base::operator dart_gptr_t);
	}

};

}  // namespace dash

#endif  // DASH__GLOB_PTR_H_

