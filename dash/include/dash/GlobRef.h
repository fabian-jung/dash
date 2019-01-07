#ifndef DASH__GlobRef_H_
#define DASH__GlobRef_H_

#include <dash/GlobPtr.h>
#include <dash/Onesided.h>
#include <dash/iterator/internal/GlobRefBase.h>
#include <dash/profiling/TracedReference.h>

namespace dash {

// forward declaration
template<typename T>
class GlobAsyncRef;

// Forward declarations
template<typename T, class MemSpaceT> class GlobPtr;

template<typename T>
class GlobRefImpl
{
  template<typename U>
  friend std::ostream & operator<<(
    std::ostream & os,
    const GlobRefImpl<U> & gref);

  template <typename ElementT>
  friend class GlobRefImpl;

public:
  using value_type          = T;
  using const_value_type    = typename std::add_const<T>::type;
  using nonconst_value_type = typename std::remove_const<T>::type;
  using self_t              = GlobRefImpl<T>;
  using const_type          = GlobRefImpl<const_value_type>;


  template <class _T, class _M>
  friend class GlobPtr;

  template <class _T, class _Pat, class _M, class _Ptr, class _Ref>
  friend class GlobIter;

  template <class _T, class _Pat, class _M, class _Ptr, class _Ref>
  friend class GlobViewIter;

private:
  /**
   * PRIVATE: Constructor, creates an GlobRefImplImpl object referencing an element in global
   * memory.
   */
  template<class ElementT, class MemSpaceT>
  explicit constexpr GlobRefImpl(
    /// Pointer to referenced object in global memory
    const GlobPtr<ElementT, MemSpaceT> & gptr)
  : GlobRefImpl(gptr.dart_gptr())
  { }

public:
  /**
   * Reference semantics forbid declaration without definition.
   */
  GlobRefImpl() = delete;

  GlobRefImpl(const GlobRefImpl & other) = delete;

  /**
   * Constructor, creates an GlobRefImpl object referencing an element in global
   * memory.
   */
  explicit constexpr GlobRefImpl(dart_gptr_t dart_gptr)
  : _gptr(dart_gptr)
  {
  }

  /**
   * Copy constructor, implicit if at least one of the following conditions is
   * satisfied:
   *    1) value_type and _T are exactly the same types (including const and
   *    volatile qualifiers
   *    2) value_type and _T are the same types after removing const and
   *    volatile qualifiers and value_type itself is const.
   */
  template <
      typename _T,
      long = internal::enable_implicit_copy_ctor<value_type, _T>::value>
  constexpr GlobRefImpl(const GlobRefImpl<_T>& gref)
    : GlobRefImpl(gref.dart_gptr())
  {
  }

  /**
   * Copy constructor, explicit if the following conditions are satisfied.
   *    1) value_type and _T are the same types after excluding const and
   *    volatile qualifiers
   *    2) value_type is const and _T is non-const
   */
  template <
      typename _T,
      int = internal::enable_explicit_copy_ctor<value_type, _T>::value>
  explicit constexpr GlobRefImpl(const GlobRefImpl<_T>& gref)
    : GlobRefImpl(gref.dart_gptr())
  {
  }

  /**
   * Constructor to convert \c GlobAsyncRef to GlobRefImpl. Set to explicit to
   * avoid unintendet conversion
   */
  template <
      typename _T,
      long = internal::enable_implicit_copy_ctor<value_type, _T>::value>
  constexpr GlobRefImpl(const GlobAsyncRef<_T>& gref)
    : _gptr(gref.dart_gptr())
  {
  }

  template <
      typename _T,
      int = internal::enable_explicit_copy_ctor<value_type, _T>::value>
  explicit constexpr GlobRefImpl(const GlobAsyncRef<_T>& gref)
    : GlobRefImpl(gref.dart_gptr())
  {
  }

  /**
   * Move Constructor
   */
  GlobRefImpl(self_t&& other)
    :_gptr(std::move(other._gptr))
  {
    DASH_LOG_TRACE("GlobRefImpl.GlobRefImpl(GlobRefImpl &&)", _gptr);
  }

  /**
   * Copy Assignment
   */
  const self_t & operator=(const self_t & other) const
  {
    if (DART_GPTR_EQUAL(_gptr, other._gptr)) {
      return *this;
    }
    set(static_cast<T>(other));
    return *this;
  }

  /**
   * Move Assignment: Redirects to Copy Assignment
   */
  self_t& operator=(self_t&& other) {
    DASH_LOG_TRACE("GlobRefImpl.operator=(GlobRefImpl &&)", _gptr);
    operator=(other);
    return *this;
  }

  /**
   * Value-assignment operator.
   */
  const self_t & operator=(const value_type& val) const {
    set(val);
    return *this;
  }


  operator nonconst_value_type() const {
    DASH_LOG_TRACE("GlobRefImpl.T()", "conversion operator");
    DASH_LOG_TRACE_VAR("GlobRefImpl.T()", _gptr);
    nonconst_value_type t;
    dash::internal::get_blocking(_gptr, &t, 1);
    DASH_LOG_TRACE_VAR("GlobRefImpl.T >", _gptr);
    return t;
  }

  template <typename ValueT>
  bool operator==(const GlobRefImpl<ValueT> & other) const {
    ValueT val = other.get();
    return operator==(val);
  }

  template <typename ValueT>
  bool operator!=(const GlobRefImpl<ValueT> & other) const {
    return !(*this == other);
  }

  template<typename ValueT>
  constexpr bool operator==(const ValueT& value) const
  {
    return static_cast<T>(*this) == value;
  }

  template<typename ValueT>
  constexpr bool operator!=(const ValueT& value) const
  {
    return !(*this == value);
  }

  void
  set(const value_type & val) const {
    static_assert(std::is_same<value_type, nonconst_value_type>::value,
                  "Cannot modify value referenced by GlobRefImpl<const T>!");

    DASH_LOG_TRACE_VAR("GlobRefImpl.set()", val);
    DASH_LOG_TRACE_VAR("GlobRefImpl.set", _gptr);
    // TODO: Clarify if dart-call can be avoided if
    //       _gptr->is_local()
    dash::internal::put_blocking(_gptr, &val, 1);
    DASH_LOG_TRACE_VAR("GlobRefImpl.set >", _gptr);
  }

  nonconst_value_type get() const {
    DASH_LOG_TRACE("T GlobRefImpl.get()", "explicit get");
    DASH_LOG_TRACE_VAR("GlobRefImpl.T()", _gptr);
    nonconst_value_type t;
    dash::internal::get_blocking(_gptr, &t, 1);
    return t;
  }

  void get(nonconst_value_type *tptr) const {
    DASH_LOG_TRACE("GlobRefImpl.get(T*)", "explicit get into provided ptr");
    DASH_LOG_TRACE_VAR("GlobRefImpl.T()", _gptr);
    dash::internal::get_blocking(_gptr, tptr, 1);
  }

  void get(nonconst_value_type& tref) const {
    DASH_LOG_TRACE("GlobRefImpl.get(T&)", "explicit get into provided ref");
    DASH_LOG_TRACE_VAR("GlobRefImpl.T()", _gptr);
    dash::internal::get_blocking(_gptr, &tref, 1);
  }

  void
  put(const_value_type& tref) const {
    static_assert(std::is_same<value_type, nonconst_value_type>::value,
                  "Cannot assign to GlobRefImpl<const T>!");
    DASH_LOG_TRACE("GlobRefImpl.put(T&)", "explicit put of provided ref");
    DASH_LOG_TRACE_VAR("GlobRefImpl.T()", _gptr);
    dash::internal::put_blocking(_gptr, &tref, 1);
  }

  void
  put(const_value_type* tptr) const {
    static_assert(std::is_same<value_type, nonconst_value_type>::value,
                  "Cannot modify value referenced by GlobRefImpl<const T>!");
    DASH_LOG_TRACE("GlobRefImpl.put(T*)", "explicit put of provided ptr");
    DASH_LOG_TRACE_VAR("GlobRefImpl.T()", _gptr);
    dash::internal::put_blocking(_gptr, tptr, 1);
  }

  const self_t &
  operator+=(const nonconst_value_type& ref) const {
    static_assert(std::is_same<value_type, nonconst_value_type>::value,
                  "Cannot modify value referenced by GlobRefImpl<const T>!");
#if 0
    // TODO: Alternative implementation, possibly more efficient:
    T add_val = ref;
    T old_val;
    dart_ret_t result = dart_fetch_and_op(
                          _gptr,
                          reinterpret_cast<void *>(&add_val),
                          reinterpret_cast<void *>(&old_val),
                          dash::dart_datatype<T>::value,
                          dash::plus<T>().dart_operation(),
                          dash::Team::All().dart_id());
    dart_flush(_gptr);
  #else
    nonconst_value_type val  = operator nonconst_value_type();
    val   += ref;
    operator=(val);
  #endif
    return *this;
  }

  const self_t &
  operator-=(const nonconst_value_type& ref) const {
    static_assert(std::is_same<value_type, nonconst_value_type>::value,
                  "Cannot modify value referenced by GlobRefImpl<const T>!");
    nonconst_value_type val  = operator nonconst_value_type();
    val   -= ref;
    operator=(val);
    return *this;
  }

  const self_t &
  operator++() const {
    static_assert(std::is_same<value_type, nonconst_value_type>::value,
                  "Cannot modify value referenced by GlobRefImpl<const T>!");
    nonconst_value_type val = operator nonconst_value_type();
    operator=(++val);
    return *this;
  }

  nonconst_value_type
  operator++(int) const {
    static_assert(std::is_same<value_type, nonconst_value_type>::value,
                  "Cannot modify value referenced by GlobRefImpl<const T>!");
    nonconst_value_type val = operator nonconst_value_type();
    nonconst_value_type res = val++;
    operator=(val);
    return res;
  }

  const self_t &
  operator--() const {
    static_assert(std::is_same<value_type, nonconst_value_type>::value,
                  "Cannot modify value referenced by GlobRefImpl<const T>!");
    nonconst_value_type val = operator nonconst_value_type();
    operator=(--val);
    return *this;
  }

  nonconst_value_type
  operator--(int) const {
    static_assert(std::is_same<value_type, nonconst_value_type>::value,
                  "Cannot modify value referenced by GlobRefImpl<const T>!");
    nonconst_value_type val = operator nonconst_value_type();
    nonconst_value_type res = val--;
    operator=(val);
    return res;
  }

  const self_t &
  operator*=(const_value_type& ref) const {
    static_assert(std::is_same<value_type, nonconst_value_type>::value,
                  "Cannot modify value referenced by GlobRefImpl<const T>!");
    nonconst_value_type val = operator nonconst_value_type();
    val   *= ref;
    operator=(val);
    return *this;
  }

  const self_t &
  operator/=(const_value_type& ref) const {
    static_assert(std::is_same<value_type, nonconst_value_type>::value,
                  "Cannot modify value referenced by GlobRefImpl<const T>!");
    nonconst_value_type val = operator nonconst_value_type();
    val   /= ref;
    operator=(val);
    return *this;
  }

  const self_t &
  operator^=(const_value_type& ref) const {
    static_assert(std::is_same<value_type, nonconst_value_type>::value,
                  "Cannot modify value referenced by GlobRefImpl<const T>!");
    nonconst_value_type val = operator nonconst_value_type();
    val   ^= ref;
    operator=(val);
    return *this;
  }

  constexpr dart_gptr_t dart_gptr() const noexcept {
    return _gptr;
  }

  /**
   * Checks whether the globally referenced element is in
   * the calling unit's local memory.
   */
  bool is_local() const {
    dart_team_unit_t luid;
    dart_team_myid(_gptr.teamid, &luid);
    return _gptr.unitid == luid.id;
  }

  /**
   * Get a global ref to a member of a certain type at the
   * specified offset
   */
  template<typename MEMTYPE>
  GlobRefImpl<typename internal::add_const_from_type<T, MEMTYPE>::type>
  member(size_t offs) const {
    dart_gptr_t dartptr = _gptr;
    DASH_ASSERT_RETURNS(
      dart_gptr_incaddr(&dartptr, offs),
      DART_OK);
    return GlobRefImpl<typename internal::add_const_from_type<T, MEMTYPE>::type>(dartptr);
  }

  /**
   * Get the member via pointer to member
   */
  template<class MEMTYPE, class P=T>
  GlobRefImpl<typename internal::add_const_from_type<T, MEMTYPE>::type>
  member(
    const MEMTYPE P::*mem) const {
    // TODO: Thaaaat ... looks hacky.
    auto offs = (size_t) & (reinterpret_cast<P*>(0)->*mem);
    return member<typename internal::add_const_from_type<T, MEMTYPE>::type>(offs);
  }

  /**
   * specialization which swappes the values of two global references
   */
  inline void swap(dash::GlobRefImpl<T> & b) const{
    static_assert(std::is_same<value_type, nonconst_value_type>::value,
                  "Cannot modify value referenced by GlobRefImpl<const T>!");
    auto tmp = static_cast<T>(*this);
    *this = b;
    b = tmp;
  }

private:
  dart_gptr_t _gptr{};
};

template<typename T>
std::ostream & operator<<(
  std::ostream     & os,
  const GlobRefImpl<T> & gref)
{
  char buf[100]; //
  sprintf(buf,
          "(%06X|%02X|%04X|%04X|%016lX)",
          gref._gptr.unitid,
          gref._gptr.flags,
          gref._gptr.segid,
          gref._gptr.teamid,
          gref._gptr.addr_or_offs.offset);
  os << dash::typestr(gref) << buf;
  return os;
}

/**
 * specialization for unqualified calls to swap
 */
template<typename T>
inline void swap(dash::GlobRefImpl<T> && a, dash::GlobRefImpl<T> && b){
  a.swap(b);
}

/**
 * specialization for unqualified calls to swap
 */
template <class MemSpaceT, class T>
inline auto addressof(dash::GlobRefImpl<T> const & ref)
{
  return dash::GlobPtr<T, MemSpaceT>(ref.dart_gptr());
}

template <class T>
class GlobRef : public TracedReference<GlobRefImpl<T>> {
public:

	using const_type = typename GlobRefImpl<T>::const_type;
	using value_type = typename GlobRefImpl<T>::value_type;

	GlobRef (dart_gptr_t dart_gptr) :
		TracedReference<GlobRefImpl<T>>(dart_gptr)
	{}

	template <class PatternT>
	operator GlobPtr<T, PatternT> () const {
		this->traced_call(&GlobRefImpl<T>::operator GlobPtr<T, PatternT>);
	}

	GlobRef& operator=(const value_type value) {
		this->TracedReference<GlobRefImpl<T>>::operator=(value);
		return *this;
	}

	dart_gptr_t dart_gptr () const {
		return this->traced_call(&GlobRefImpl<T>::dart_gptr);
	}

	bool is_local () const {
		return this->traced_call(&GlobRefImpl<T>::dart_gptr);
	}

	T get () const {
		return this->traced_call(static_cast<T (GlobRefImpl<T>::*)() const>(&GlobRefImpl<T>::get));
	}

	void get (T *tptr) const {
		this->traced_call(static_cast<void (GlobRefImpl<T>::*)(T *tptr) const>(&GlobRefImpl<T>::get), tptr);
	}

	void get (T &tref) const {
		this->traced_call(static_cast<void (GlobRefImpl<T>::*)(T &tptr) const>(&GlobRefImpl<T>::get), tref);
	}

	void put (T &tref) const {
		this->traced_call(static_cast<void (GlobRefImpl<T>::*)(T &tptr) const>(&GlobRefImpl<T>::put), tref);
	}

	void put (T *tptr) const {
		this->traced_call(static_cast<void (GlobRefImpl<T>::*)(T *tptr) const>(&GlobRefImpl<T>::put), tptr);
	}

	// GlobRef< MEMTYPE > 	member (size_t offs) const
	// GlobRef< MEMTYPE > 	member (const MEMTYPE P::*mem) const

// 	friend std::ostream& operator<<(std::ostream &os, const GlobRef< U > &gref);
// 	friend void swap (GlobRef< T > a, GlobRef< T > b);
	};

} // namespace dash

#endif // DASH__GlobRefImpl_H_
