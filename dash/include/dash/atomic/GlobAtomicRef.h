#ifndef DASH__ATOMIC_GlobRefImpl_H_
#define DASH__ATOMIC_GlobRefImpl_H_

#include <dash/GlobPtr.h>
//#include <dash/Types.h>
#include <dash/algorithm/Operation.h>
#include <dash/iterator/internal/GlobRefBase.h>

namespace dash {

// forward decls
template <typename T>
class Atomic;


template <typename T>
class GlobAsyncRef;
/**
 * Specialization for atomic values. All atomic operations are
 * \c const as the \c GlobRefImpl does not own the atomic values.
 */
template <typename T>
class GlobRefImpl<dash::Atomic<T>> {
  /* Notes on type compatibility:
   *
   * - The general support of atomic operations on values of type T is
   *   checked in `dash::Atomic` and is not verified here.
   * - Whether arithmetic operations (like `fetch_add`) are supported
   *   for values of type T is implicitly tested in the DASH operation
   *   types (like `dash::plus<T>`) and is not verified here.
   *
   */

  template <typename U>
  friend std::ostream& operator<<(std::ostream& os, const GlobRefImpl<U>& gref);

public:
  using value_type          = T;
  using const_value_type    = typename std::add_const<T>::type;
  using nonconst_value_type = typename std::remove_const<T>::type;
  using atomic_t            = dash::Atomic<T>;
  using const_atomic_t      = typename dash::Atomic<const_value_type>;
  using nonconst_atomic_t   = typename dash::Atomic<nonconst_value_type>;
  using self_t              = GlobRefImpl<atomic_t>;
  using const_type          = GlobRefImpl<const_atomic_t>;
  using nonconst_type       = GlobRefImpl<dash::Atomic<nonconst_value_type>>;

private:
  dart_gptr_t _gptr{};


public:
  /**
   * Reference semantics forbid declaration without definition.
   */
  GlobRefImpl() = delete;

  //TODO rkowalewski: Clarify constructors by passing various pointer types

  /**
   * Constructor: Create an atomic reference to a element in global memory
   */
  explicit GlobRefImpl(dart_gptr_t dart_gptr)
    : _gptr(dart_gptr)
  {
    DASH_LOG_TRACE_VAR("GlobRefImpl(dart_gptr_t)", dart_gptr);
  }

  /**
   * Constructor, creates an GlobRefImpl object referencing an element in global
   * memory.
   */
  template <typename PatternT>
  explicit GlobRefImpl(
      /// Pointer to referenced object in global memory
      const GlobPtr<const_atomic_t, PatternT>& gptr)
    : GlobRefImpl(gptr.dart_gptr())
  {
    static_assert(
        std::is_same<value_type, const_value_type>::value,
        "Cannot create GlobRefImpl<Atomic<T>> from GlobPtr<Atomic<const T>>!");
  }

  template <typename PatternT>
  explicit GlobRefImpl(
      /// Pointer to referenced object in global memory
      const GlobPtr<nonconst_atomic_t, PatternT>& gptr)
    : GlobRefImpl(gptr.dart_gptr())
  {
  }

  /**
   * Copy constructor: Implicit if at least one of the following conditions is
   * satisfied:
   *    1) value_type and _T are exactly the same types (including const and
   *    volatile qualifiers
   *    2) value_type and _T are the same types after removing const and
   *    volatile qualifiers and value_type itself is const.
   */
  template <
      typename _T,
      int = internal::enable_implicit_copy_ctor<value_type, _T>::value>
  GlobRefImpl(const GlobRefImpl<dash::Atomic<_T>>& gref)
    : GlobRefImpl(gref.dart_gptr())
  {
  }

  /**
   * Copy constructor: Explicit if the following conditions are satisfied.
   *    1) value_type and _T are the same types after excluding const and
   *    volatile qualifiers
   *    2) value_type is const and _T is non-const
   */
  template <
      typename _T,
      long = internal::enable_explicit_copy_ctor<value_type, _T>::value>
  explicit GlobRefImpl(const GlobRefImpl<dash::Atomic<_T>>& gref)
    : GlobRefImpl(gref.dart_gptr())
  {
  }

  /**
   * Copy constructor: Implicit if at least one of the following conditions is
   * satisfied:
   *    1) value_type and _T are exactly the same types (including const and
   *    volatile qualifiers
   *    2) value_type and _T are the same types after removing const and
   *    volatile qualifiers and value_type itself is const.
   */
  template <
      typename _T,
      int = internal::enable_implicit_copy_ctor<value_type, _T>::value>
  GlobRefImpl(const GlobAsyncRef<dash::Atomic<_T>>& gref)
    : GlobRefImpl(gref.dart_gptr())
  {
  }

  /**
   * Copy constructor: Explicit if the following conditions are satisfied.
   *    1) value_type and _T are the same types after excluding const and
   *    volatile qualifiers
   *    2) value_type is const and _T is non-const
   */
  template <
      typename _T,
      long = internal::enable_explicit_copy_ctor<value_type, _T>::value>
  explicit GlobRefImpl(const GlobAsyncRef<dash::Atomic<_T>>& gref)
    : GlobRefImpl(gref.dart_gptr())
  {
  }

  /**
   * Move Constructor.
   */
  GlobRefImpl(self_t&& other) = default;

  /**
   * Copy Assignment: Copies atomically the value from other
   */
  self_t& operator=(const self_t& other) const
  {
    store(static_cast<atomic_t>(other));
    return *this;
  }

  /**
   * Move Assignment: Redirects to Copy Assignment
   */
  self_t& operator=(self_t&& other) {
    operator=(other);
    return *this;
  }

  operator T() const
  {
    return load();
  }

  dart_gptr_t dart_gptr() const
  {
    return _gptr;
  }

  /**
   * Checks whether the globally referenced element is in
   * the calling unit's local memory.
   */
  bool is_local() const
  {
    return dash::internal::is_local(_gptr);
  }

  /**
   * atomically assigns value
   *
   * \return The assigned value.
   *
   * \note This operator does not return a reference but a copy of the value
   * in order to ensure atomicity. This is consistent with the C++ std::atomic
   * \c operator=, see
   * http://en.cppreference.com/w/cpp/atomic/atomic/operator%3D.
   */
  T operator=(const T& value) const
  {
    store(value);
    return value;
  }

  /**
   * Set the value of the shared atomic variable.
   */
  void set(const T& value) const
  {
    static_assert(
        dash::dart_punned_datatype<T>::value != DART_TYPE_UNDEFINED,
        "Basic type or type smaller than 64bit required for "
        "atomic set!");
    static_assert(
        std::is_same<value_type, nonconst_value_type>::value,
        "Cannot modify value referenced by GlobRefImpl<Atomic<const T>>!");
    DASH_LOG_DEBUG_VAR("GlobRefImpl<Atomic>.store()", value);
    DASH_LOG_TRACE_VAR("GlobRefImpl<Atomic>.store", _gptr);
    dart_ret_t ret = dart_accumulate(
        _gptr,
        &value,
        1,
        dash::dart_punned_datatype<T>::value,
        DART_OP_REPLACE);
    dart_flush(_gptr);
    DASH_ASSERT_EQ(DART_OK, ret, "dart_accumulate failed");
    DASH_LOG_DEBUG("GlobRefImpl<Atomic>.store >");
  }

  /**
   * Set the value of the shared atomic variable.
   */
  inline void store(const T& value) const
  {
    set(value);
  }

  /// atomically fetches value
  T get() const
  {
    static_assert(
        dash::dart_punned_datatype<T>::value != DART_TYPE_UNDEFINED,
        "Basic type or type smaller than 64bit required for "
        "atomic get!");
    DASH_LOG_DEBUG("GlobRefImpl<Atomic>.load()");
    DASH_LOG_TRACE_VAR("GlobRefImpl<Atomic>.load", _gptr);
    nonconst_value_type nothing;
    nonconst_value_type result;
    dart_ret_t          ret = dart_fetch_and_op(
        _gptr,
        &nothing,
        &result,
        dash::dart_punned_datatype<T>::value,
        DART_OP_NO_OP);
    dart_flush_local(_gptr);
    DASH_ASSERT_EQ(DART_OK, ret, "dart_accumulate failed");
    DASH_LOG_DEBUG_VAR("GlobRefImpl<Atomic>.get >", result);
    return result;
  }

  /**
   * Get the value of the shared atomic variable.
   */
  inline T load() const
  {
    return get();
  }

  /**
   * Atomically executes specified operation on the referenced shared value.
   */
  template <typename BinaryOp>
  void op(
      /// Binary operation to be performed on global atomic variable
      BinaryOp binary_op,
      /// Value to be used in binary op on global atomic variable.
      const T& value) const
  {
    static_assert(
        dash::dart_punned_datatype<T>::value != DART_TYPE_UNDEFINED,
        "Basic type or type smaller than 64bit required for "
        "atomic operation!");
    static_assert(
        dash::dart_datatype<T>::value != DART_TYPE_UNDEFINED ||
            binary_op.op_kind() != dash::internal::OpKind::ARITHMETIC,
        "Atomic arithmetic operations only valid on basic types");
    static_assert(
        std::is_same<value_type, nonconst_value_type>::value,
        "Cannot modify value referenced by GlobRefImpl<Atomic<const T>>!");
    DASH_LOG_DEBUG_VAR("GlobRefImpl<Atomic>.op()", value);
    DASH_LOG_TRACE_VAR("GlobRefImpl<Atomic>.op", _gptr);
    nonconst_value_type acc = value;
    DASH_LOG_TRACE("GlobRefImpl<Atomic>.op", "dart_accumulate");
    dart_ret_t ret = dart_accumulate(
        _gptr,
        &acc,
        1,
        dash::dart_punned_datatype<T>::value,
        binary_op.dart_operation());
    dart_flush(_gptr);
    DASH_ASSERT_EQ(DART_OK, ret, "dart_accumulate failed");
    DASH_LOG_DEBUG_VAR("GlobRefImpl<Atomic>.op >", acc);
  }

  /**
   * Atomic fetch-and-op operation on the referenced shared value.
   *
   * \return  The value of the referenced shared variable before the
   *          operation.
   */
  template <typename BinaryOp>
  T fetch_op(
      BinaryOp binary_op,
      /// Value to be added to global atomic variable.
      const T& value) const
  {
    static_assert(
        dash::dart_punned_datatype<T>::value != DART_TYPE_UNDEFINED,
        "Basic type or type smaller than 64bit required for "
        "atomic fetch_op!");
    static_assert(
        dash::dart_datatype<T>::value != DART_TYPE_UNDEFINED ||
            binary_op.op_kind() != dash::internal::OpKind::ARITHMETIC,
        "Atomic arithmetic operations only valid on basic types!");
    static_assert(
        std::is_same<value_type, nonconst_value_type>::value,
        "Cannot modify value referenced by GlobRefImpl<Atomic<const T>>!");
    DASH_LOG_DEBUG_VAR("GlobRefImpl<Atomic>.fetch_op()", value);
    DASH_LOG_TRACE_VAR("GlobRefImpl<Atomic>.fetch_op", _gptr);
    DASH_LOG_TRACE_VAR("GlobRefImpl<Atomic>.fetch_op", typeid(value).name());
    nonconst_value_type res;
    dart_ret_t          ret = dart_fetch_and_op(
        _gptr,
        &value,
        &res,
        dash::dart_punned_datatype<T>::value,
        binary_op.dart_operation());
    dart_flush(_gptr);
    DASH_ASSERT_EQ(DART_OK, ret, "dart_fetch_op failed");
    DASH_LOG_DEBUG_VAR("GlobRefImpl<Atomic>.fetch_op >", res);
    return res;
  }

  /**
   * Atomically exchanges value
   */
  T exchange(const T& value) const
  {
    return fetch_op(dash::second<T>(), value);
  }

  /**
   * Atomically compares the value with the value of expected and if thosei
   * are bitwise-equal, replaces the former with desired.
   *
   * \return  True if value is exchanged
   *
   * \see \c dash::atomic::compare_exchange
   */
  bool compare_exchange(const T& expected, const T& desired) const
  {
    static_assert(
        dash::dart_punned_datatype<T>::value != DART_TYPE_UNDEFINED,
        "Integral type or type smaller than 64bit required for "
        "compare_exchange!");
    static_assert(
        !std::is_floating_point<T>::value,
        "compare_exchange not available for floating point!");
    static_assert(
        std::is_same<value_type, nonconst_value_type>::value,
        "Cannot modify value referenced by GlobRefImpl<const T>!");
    DASH_LOG_DEBUG_VAR("GlobRefImpl<Atomic>.compare_exchange()", desired);
    DASH_LOG_TRACE_VAR("GlobRefImpl<Atomic>.compare_exchange", _gptr);
    DASH_LOG_TRACE_VAR("GlobRefImpl<Atomic>.compare_exchange", expected);
    DASH_LOG_TRACE_VAR(
        "GlobRefImpl<Atomic>.compare_exchange", typeid(desired).name());
    nonconst_value_type result;
    dart_ret_t          ret = dart_compare_and_swap(
        _gptr,
        &desired,
        &expected,
        &result,
        dash::dart_punned_datatype<T>::value);
    dart_flush(_gptr);
    DASH_ASSERT_EQ(DART_OK, ret, "dart_compare_and_swap failed");
    DASH_LOG_DEBUG_VAR(
        "GlobRefImpl<Atomic>.compare_exchange >", (expected == result));
    return (expected == result);
  }

  /**
   * DASH specific variant which is faster than \c fetch_add
   * but does not return value
   */
  void add(const T& value) const
  {
    static_assert(
        std::is_same<value_type, nonconst_value_type>::value,
        "Cannot modify value referenced by GlobRefImpl<const T>!");
    op(dash::plus<T>(), value);
  }

  /**
   * Atomic fetch-and-add operation on the referenced shared value.
   *
   * \return  The value of the referenced shared variable before the
   *          operation.
   */
  T fetch_add(
      /// Value to be added to global atomic variable.
      const T& value) const
  {
    return fetch_op(dash::plus<T>(), value);
  }

  /**
   * DASH specific variant which is faster than \c fetch_sub
   * but does not return value
   */
  void sub(const T& value) const
  {
    op(dash::plus<T>(), -value);
  }

  /**
   * Atomic fetch-and-sub operation on the referenced shared value.
   *
   * \return  The value of the referenced shared variable before the
   *          operation.
   */
  T fetch_sub(
      /// Value to be subtracted from global atomic variable.
      const T& value) const
  {
    return fetch_op(dash::plus<T>(), -value);
  }

  /**
   * DASH specific variant which is faster than \c fetch_multiply
   * but does not return value
   */
  void multiply(const T& value) const
  {
    op(dash::multiply<T>(), value);
  }

  /**
   * Atomic fetch-and-multiply operation on the referenced shared value.
   *
   * \return  The value of the referenced shared variable before the
   *          operation.
   */
  T fetch_multiply(
      /// Value to be added to global atomic variable.
      const T& value) const
  {
    return fetch_op(dash::multiply<T>(), value);
  }

  /**
   * prefix atomically increment value by one
   *
   * \return  The value of the referenced shared variable after the
   *          operation.
   *
   * \note This operator does not return a reference but a copy of the value
   * in order to ensure atomicity. This is consistent with the C++ std::atomic
   * \c operator++, see
   * http://en.cppreference.com/w/cpp/atomic/atomic/operator_arith.
   */
  T operator++() const
  {
    return fetch_add(1) + 1;
  }

  /**
   * postfix atomically increment value by one
   *
   * \return  The value of the referenced shared variable before the
   *          operation.
   */
  T operator++(int) const
  {
    return fetch_add(1);
  }

  /**
   * prefix atomically decrement value by one
   *
   * \return  The value of the referenced shared variable after the
   *          operation.
   *
   * \note This operator does not return a reference but a copy of the value
   * in order to ensure atomicity. This is consistent with the C++ std::atomic
   * \c operator--, see
   * http://en.cppreference.com/w/cpp/atomic/atomic/operator_arith.
   */
  T operator--() const
  {
    return fetch_sub(1) - 1;
  }

  /**
   * postfix atomically decrement value by one
   *
   * \return  The value of the referenced shared variable before the
   *          operation.
   */
  T operator--(int) const
  {
    return fetch_sub(1);
  }

  /**
   * atomically increment value by ref
   *
   * \return  The value of the referenced shared variable after the
   *          operation.
   *
   * \note This operator does not return a reference but a copy of the value
   * in order to ensure atomicity. This is consistent with the C++ std::atomic
   * \c operator+=, see
   * http://en.cppreference.com/w/cpp/atomic/atomic/operator_arith2.
   */
  T operator+=(const T& value) const
  {
    return fetch_add(value) + value;
  }

  /**
   * atomically decrement value by ref
   *
   * \return  The value of the referenced shared variable after the
   *          operation.
   *
   * Note that this operator does not return a reference but a copy of the
   * value in order to ensure atomicity. This is consistent with the C++
   * std::atomic \c operator-=, see
   * http://en.cppreference.com/w/cpp/atomic/atomic/operator_arith2.
   */
  T operator-=(const T& value) const
  {
    return fetch_sub(value) - value;
  }
};

template <class T>
class TracedGlobRef<dash::Atomic<T>> : public TracedReference<GlobRefImpl<dash::Atomic<T>>> {
public:

	using const_type = typename GlobRefImpl<dash::Atomic<T>>::const_type;

	TracedGlobRef (dart_gptr_t dart_gptr) :
		TracedReference<GlobRefImpl<dash::Atomic<T>>>(dart_gptr)
	{}

	template <class PatternT>
	operator GlobPtr<T, PatternT> () const {
		this->traced_call(&GlobRefImpl<T>::operator GlobPtr<T, PatternT>);
	}

	dart_gptr_t dart_gptr () const {
		return this->traced_call(&GlobRefImpl<dash::Atomic<T>>::dart_gptr);
	}

	bool is_local () const {
		return this->traced_call(&GlobRefImpl<T>::dart_gptr);
	}

	T get () const {
		return this->traced_call(static_cast<T (GlobRefImpl<dash::Atomic<T>>::*)() const>(&GlobRefImpl<dash::Atomic<T>>::get));
	}

	void get (T *tptr) const {
		this->traced_call(static_cast<void (GlobRefImpl<dash::Atomic<T>>::*)(T *tptr) const>(&GlobRefImpl<dash::Atomic<T>>::get), tptr);
	}

	void get (T &tref) const {
		this->traced_call(static_cast<void (GlobRefImpl<dash::Atomic<T>>::*)(T &tptr) const>(&GlobRefImpl<dash::Atomic<T>>::get), tref);
	}

	void put (T &tref) const {
		this->traced_call(static_cast<void (GlobRefImpl<dash::Atomic<T>>::*)(T &tptr) const>(&GlobRefImpl<dash::Atomic<T>>::put), tref);
	}

	void put (T *tptr) const {
		this->traced_call(static_cast<void (GlobRefImpl<dash::Atomic<T>>::*)(T *tptr) const>(&GlobRefImpl<dash::Atomic<T>>::put), tptr);
	}

	void add(const T& value) const {
		this->traced_call(&GlobRefImpl<dash::Atomic<T>>::add, value);
	}

	T fetch_add(const T& value) const {
		return this->traced_call(&GlobRefImpl<dash::Atomic<T>>::fetch_add, value);
	}

	void sub(const T& value) const {
		this->traced_call(&GlobRefImpl<dash::Atomic<T>>::sub, value);
	}

	T fetch_sub(const T& value) const {
		return this->traced_call(&GlobRefImpl<dash::Atomic<T>>::fetch_sub, value);
	}

	void multiply(const T& value) const {
		this->traced_call(&GlobRefImpl<dash::Atomic<T>>::multiply, value);
	}

	T fetch_multiply(const T& value) const {
		return this->traced_call(&GlobRefImpl<dash::Atomic<T>>::fetch_multiply, value);
	}

	T load() const {
		return this->traced_call(&GlobRefImpl<dash::Atomic<T>>::load);
	}
	// GlobRef< MEMTYPE > 	member (size_t offs) const
	// GlobRef< MEMTYPE > 	member (const MEMTYPE P::*mem) const

// 	friend std::ostream& operator<<(std::ostream &os, const GlobRef< U > &gref);
// 	friend void swap (GlobRef< T > a, GlobRef< T > b);
}; // End of class GlobRef

}  // namespace dash

#endif  // DASH__ATOMIC_GlobRefImpl_H_
