#ifndef DASH__ALGORITHM_H_
#define DASH__ALGORITHM_H_

#include <dash/internal/Logging.h>
#include <algorithm>
#include <functional>
#include <cstddef>

namespace dash {

template<typename ElementType>
struct LocalRange {
  const ElementType * begin;
  const ElementType * end;
};

template<typename IndexType>
struct LocalIndexRange {
  IndexType begin;
  IndexType end;
};

/**
 * Resolve the number of elements between two global iterators.
 *
 * \tparam      ElementType  Type of the elements in the range
 * \complexity  O(d)
 */
template<typename ElementType>
gptrdiff_t distance(
  /// Iterator to the initial position in the global sequence
  const GlobIter<ElementType> & first,
  /// Iterator to the final position in the global sequence
  const GlobIter<ElementType> & last) {
  return last - first;
}

/**
 * Resolves the local index range between global iterators.
 *
 * \b Example:
 *
 *   Total range      | <tt>0 1 2 3 4 5 6 7 8 9</tt>
 *   ---------------- | --------------------------------
 *   Global iterators | <tt>first = 4; last = 7;</tt>
 *                    | <tt>0 1 2 3 [4 5 6 7] 8 9]</tt>
 *   Local elements   | (local index:value) <tt>0:2 1:3 2:6 3:7</tt>
 *   Result           | (local indices) <tt>2 3</tt>
 *
 * \return      A local range consisting of native pointers to the first
 *              and last local element within the sequence limited by the
 *              given global iterators.
 *
 * \tparam      ElementType  Type of the elements in the sequence
 * \tparam      PatternType  Type of the global iterators' pattern 
 *                           implementation
 * \complexity  O(d), with \c d dimensions in the global iterators' pattern
 */
template<
  typename ElementType,
  class PatternType>
LocalIndexRange<typename PatternType::index_type>
local_index_subrange(
  /// Iterator to the initial position in the global sequence
  const GlobIter<ElementType, PatternType> & first,
  /// Iterator to the final position in the global sequence
  const GlobIter<ElementType, PatternType> & last) {
  typedef typename PatternType::index_type idx_t;
  // Get pattern from global iterators, O(1):
  auto pattern        = first.pattern();
  // Get offsets of iterators within global memory, O(1):
  idx_t begin_gindex  = static_cast<idx_t>(first.pos());
  idx_t end_gindex    = static_cast<idx_t>(last.pos());
  DASH_LOG_TRACE_VAR("local_index_subrange", begin_gindex);
  DASH_LOG_TRACE_VAR("local_index_subrange", end_gindex);
  DASH_LOG_TRACE_VAR("local_index_subrange", pattern.local_size());
  // Global index of first element in pattern, O(1):
  idx_t lbegin_gindex = pattern.lbegin();
  // Global index of last element in pattern, O(1):
  idx_t lend_gindex   = pattern.lend();
  DASH_LOG_TRACE_VAR("local_index_subrange", lbegin_gindex);
  DASH_LOG_TRACE_VAR("local_index_subrange", lend_gindex);
  // Intersect local range and global range, in global index domain:
  auto goffset_lbegin = std::max<idx_t>(lbegin_gindex, begin_gindex);
  auto goffset_lend   = std::min<idx_t>(lend_gindex, end_gindex);
  // Global positions of local range to global coordinates, O(d):
  auto lbegin_gcoords = pattern.coords(goffset_lbegin);
  // Subtract 1 from global end offset as it points one coordinate
  // past the last index which is out of the valid coordinates range:
  auto lend_gcoords   = pattern.coords(goffset_lend-1);
  // Global coordinates of local range to local indices, O(d):
  auto lbegin_index   = pattern.index_to_elem(lbegin_gcoords);
  // Add 1 to local end index to it points one coordinate past the
  // last index:
  auto lend_index     = pattern.index_to_elem(lend_gcoords)+1;
  // Return local index range
  return LocalIndexRange<idx_t> { lbegin_index, lend_index };
}

/**
 * Resolves the local address range between global iterators.
 *
 * \b Example:
 *
 *   Total range      | <tt>a b c d e f g h i j</tt>
 *   ---------------- | --------------------------------
 *   Global iterators | <tt>first = b; last = i;</tt>
 *                    | <tt>a b [c d e f g h] i j]</tt>
 *   Local elements   | <tt>a b d e</tt>
 *   Result           | <tt>d e</tt>
 *
 * \return      A local range consisting of native pointers to the first
 *              and last local element within the sequence limited by the
 *              given global iterators.
 *
 * \tparam      ElementType  Type of the elements in the sequence
 * \tparam      PatternType  Type of the global iterators' pattern 
 *                           implementation
 * \complexity  O(d), with \c d dimensions in the global iterators' pattern
 */
template<
  typename ElementType,
  class PatternType>
LocalRange<ElementType> local_subrange(
  /// Iterator to the initial position in the global sequence
  const GlobIter<ElementType, PatternType> & first,
  /// Iterator to the final position in the global sequence
  const GlobIter<ElementType, PatternType> & last) {
  typedef typename PatternType::index_type idx_t;
  /// Global iterators to local index range, O(d):
  auto index_range   = local_index_subrange(first, last);
  idx_t lbegin_index = index_range.begin;
  idx_t lend_index   = index_range.end;
  // Local start address from global memory:
  auto lbegin        = first.globmem().lbegin();
  // Add local offsets to local start address:
  if (lbegin == nullptr) {
    return LocalRange<ElementType> { nullptr, nullptr };
  }
  return LocalRange<ElementType> { lbegin + lbegin_index,
                                   lbegin + lend_index };
}

/**
 * Invoke a function on every element in a range distributed by a pattern.
 * Being a collaborative operation, each unit will invoke the given
 * function on its local elements only.
 *
 * \tparam      ElementType  Type of the elements in the sequence
 * \tparam      IndexType    Parameter type expected by function to
 *                           invoke, deduced from parameter \c func
 * \complexity  O(d) + O(nl), with \c d dimensions in the global iterators'
 *              pattern and \c nl local elements within the global range
 */
template<
  typename ElementType,
  typename IndexType,
  class PatternType>
void for_each(
  /// Iterator to the initial position in the sequence
  const GlobIter<ElementType, PatternType> & first,
  /// Iterator to the final position in the sequence
  const GlobIter<ElementType, PatternType> & last,
  /// Function to invoke on every index in the range
  ::std::function<void(IndexType)> & func) {
  /// Global iterators to local index range:
  auto index_range  = local_index_subrange(first, last);
  auto lbegin_index = index_range.begin;
  auto lend_index   = index_range.end;
  // Pattern from global begin iterator:
  auto pattern      = first.pattern();
  for (IndexType lindex = lbegin_index;
       lindex != lend_index;
       ++lindex) {
    IndexType gindex = pattern.local_to_global_index(lindex);
    func(gindex);
  }
}

/**
 * Returns an iterator pointing to the element with the smallest value in
 * the range [first,last).
 *
 * \return  An iterator to smallest value in the range, or last if the 
 *          range is empty.
 *
 * \tparam      ElementType  Type of the elements in the sequence
 * \complexity  O(d) + O(nl), with \c d dimensions in the global iterators'
 *              pattern and \c nl local elements within the global range
 */
template<
  typename ElementType,
  class PatternType>
GlobPtr<ElementType> min_element(
  /// Iterator to the initial position in the sequence
  const GlobIter<ElementType, PatternType> & first,
  /// Iterator to the final position in the sequence
  const GlobIter<ElementType, PatternType> & last) {
  typedef dash::GlobPtr<ElementType> globptr_t;
  auto pattern         = first.pattern();
  dash::Team & team    = pattern.team();
  dash::Array<globptr_t> minarr(team.size());
  // return last for empty array
  if (first == last) {
    return last;
  }
  // Find the local min. element in parallel
  // Get local address range between global iterators:
  auto local_range           = dash::local_subrange(first, last);
  const ElementType * lbegin = local_range.begin;
  const ElementType * lend   = local_range.end;
  const ElementType * lmin   = ::std::min_element(lbegin, lend);     
  if (lmin == lend) {
    // local range is empty
    minarr[team.myid()] = nullptr;
  } else {
    minarr[team.myid()] = first.globmem().index_to_gptr(
                            team.myid(),
                            lmin - lbegin);
  }
  dash::barrier();
  dash::Shared<globptr_t> min; 
  // find the global min. element
  if (team.myid() == 0) {
    globptr_t minloc   = minarr[0];
    ElementType minval = *minloc;
    for (auto i = 1; i < minarr.size(); ++i) {
      if ((globptr_t)minarr[i] != nullptr) {
        ElementType val = *(globptr_t)minarr[i];
        if (val < minval) {
          minloc = minarr[i];
          DASH_LOG_TRACE("Array.min_element", 
                         "Setting min val to ", val);
          minval = val;
        }
      }
    }
    min.set(minloc);
  }
  team.barrier();
  // Minimum has been set by unit 0 at this point
  globptr_t minimum = min.get();
  if (minimum == nullptr) {
    return last;
  }
  return minimum;
}

} // namespace dash

#endif // DASH__ALGORITHM_H_
