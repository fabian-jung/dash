#ifndef DASH__ALGORITHM__LOCAL_RANGE_H__
#define DASH__ALGORITHM__LOCAL_RANGE_H__

#include <dash/GlobIter.h>
#include <dash/internal/Logging.h>

namespace dash {

template<typename ElementType>
struct LocalRange {
  ElementType * begin;
  ElementType * end;
};

template<typename IndexType>
struct LocalIndexRange {
  IndexType begin;
  IndexType end;
};

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
 * \return      A local index range consisting of offsets of the first and
 *              last element in local memory within the sequence limited by
 *              the given global iterators.
 *
 * \tparam      ElementType  Type of the elements in the sequence
 * \tparam      PatternType  Type of the global iterators' pattern 
 *                           implementation
 * \complexity  O(d), with \c d dimensions in the global iterators'
 *              pattern
 *
 * \ingroup     DashAlgorithms
 */
template<
  typename ElementType,
  class PatternType>
LocalIndexRange<typename PatternType::index_type>
local_index_range(
  /// Iterator to the initial position in the global sequence
  const GlobIter<ElementType, PatternType> & first,
  /// Iterator to the final position in the global sequence
  const GlobIter<ElementType, PatternType> & last)
{
  typedef typename PatternType::index_type idx_t;
  // Get offsets of iterators within global memory, O(1):
  idx_t begin_gindex  = static_cast<idx_t>(first.pos());
  idx_t end_gindex    = static_cast<idx_t>(last.pos());
  DASH_LOG_TRACE_VAR("local_index_range()", begin_gindex);
  DASH_LOG_TRACE_VAR("local_index_range()", end_gindex);
  // Check if input range is relative to a view spec (e.g. a block):
  if (first.is_relative() && last.is_relative()) {
    DASH_LOG_TRACE("local_index_range", "input iterators are relative");
    if (first.viewspec() == last.viewspec()) {
      // TODO: Only all-local or all-nonlocal ranges supported for now.
      DASH_LOG_TRACE("local_index_range", "input iterators in same view");
      bool first_is_local = first.is_local();
      // No need to check if last is local as both are relative to the same
      // view.
//    bool last_is_local  = (last - 1).is_local();
//    if (first_is_local && last_is_local) {
      if (first_is_local) {
        DASH_LOG_TRACE("local_index_range >", first.pos(), last.pos());
        return LocalIndexRange<idx_t> { first.pos(), last.pos() };
      } else {
        DASH_LOG_TRACE("local_index_range >", "not local -> (0,0)");
        return LocalIndexRange<idx_t> { 0, 0 };
      }
    }
  }
  // Get pattern from global iterators, O(1):
  auto pattern        = first.pattern();
  DASH_LOG_TRACE_VAR("local_index_range", pattern.local_size());
  if (pattern.local_size() == 0) {
    // Local index range is empty
    DASH_LOG_TRACE("local_index_range (lsize:0) ->", 0, 0);
    return LocalIndexRange<idx_t> { 0, 0 };
  }
  // Global index of first element in pattern, O(1):
  idx_t lbegin_gindex = pattern.lbegin();
  // Global index of last element in pattern, O(1):
  idx_t lend_gindex   = pattern.lend();
  DASH_LOG_TRACE_VAR("local_index_range", lbegin_gindex);
  DASH_LOG_TRACE_VAR("local_index_range", lend_gindex);
  if (lend_gindex   <= begin_gindex ||  // local end before global begin
      lbegin_gindex >= end_gindex) {    // local begin after global end
    // No overlap, intersection is empty
    DASH_LOG_TRACE("local_index_range (intersect:0)->", 0, 0);
    return LocalIndexRange<idx_t> { 0, 0 };
  }
  // Intersect local range and global range, in global index domain:
  auto goffset_lbegin = std::max<idx_t>(lbegin_gindex, begin_gindex);
  auto goffset_lend   = std::min<idx_t>(lend_gindex, end_gindex);
  // Global positions of local range to global coordinates, O(d):
  auto lbegin_gcoords = pattern.coords(goffset_lbegin);
  // Subtract 1 from global end offset as it points one coordinate
  // past the last index which is out of the valid coordinates range:
  auto lend_gcoords   = pattern.coords(goffset_lend-1);
  // Global coordinates of local range to local indices, O(d):
  auto lbegin_index   = pattern.at(lbegin_gcoords);
  // Add 1 to local end index to it points one coordinate past the
  // last index:
  auto lend_index     = pattern.at(lend_gcoords)+1;
  // Return local index range
  DASH_LOG_TRACE("local_index_range ->", lbegin_index, lend_index);
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
 *
 * \ingroup     DashAlgorithms
 */
template<
  typename ElementType,
  class PatternType>
LocalRange<const ElementType> local_range(
  /// Iterator to the initial position in the global sequence
  const GlobIter<ElementType, PatternType> & first,
  /// Iterator to the final position in the global sequence
  const GlobIter<ElementType, PatternType> & last)
{
  typedef typename PatternType::index_type idx_t;
  /// Global iterators to local index range, O(d):
  auto index_range   = dash::local_index_range(first, last);
  idx_t lbegin_index = index_range.begin;
  idx_t lend_index   = index_range.end;
  if (lbegin_index == lend_index) {
    // Local range is empty
    DASH_LOG_TRACE("local_range", "empty local range",
                   lbegin_index, lend_index);
    return LocalRange<const ElementType> { nullptr, nullptr };
  }
  // Local start address from global memory:
  auto pattern = first.pattern();
  auto lbegin  = first.globmem().lbegin(
                   pattern.team().myid());
  // Add local offsets to local start address:
  if (lbegin == nullptr) {
    DASH_LOG_TRACE("local_range", "lbegin null");
    return LocalRange<const ElementType> { nullptr, nullptr };
  }
  return LocalRange<const ElementType> {
           lbegin + lbegin_index,
           lbegin + lend_index };
}

template<
  typename ElementType,
  class PatternType>
LocalRange<ElementType> local_range(
  /// Iterator to the initial position in the global sequence
  GlobIter<ElementType, PatternType> & first,
  /// Iterator to the final position in the global sequence
  GlobIter<ElementType, PatternType> & last)
{
  typedef typename PatternType::index_type idx_t;
  /// Global iterators to local index range, O(d):
  auto index_range   = dash::local_index_range(first, last);
  idx_t lbegin_index = index_range.begin;
  idx_t lend_index   = index_range.end;
  if (lbegin_index == lend_index) {
    // Local range is empty
    DASH_LOG_TRACE("local_range", "empty local range",
                   lbegin_index, lend_index);
    return LocalRange<ElementType> { nullptr, nullptr };
  }
  // Local start address from global memory:
  auto pattern = first.pattern();
  auto lbegin  = first.globmem().lbegin(
                   pattern.team().myid());
  // Add local offsets to local start address:
  if (lbegin == nullptr) {
    DASH_LOG_TRACE("local_range", "lbegin null");
    return LocalRange<ElementType> { nullptr, nullptr };
  }
  return LocalRange<ElementType> {
           lbegin + lbegin_index,
           lbegin + lend_index };
}

/**
 * Convert global iterator referencing an element the active unit's memory to
 * a corresponding native pointer referencing the element.
 *
 * Precondition:  \c g_it  is local
 *
 */
template<
  typename ElementType,
  class PatternType>
ElementType * local(
  /// Global iterator referencing element in local memory
  const GlobIter<ElementType, PatternType> & g_it)
{
  DASH_ASSERT_MSG(
    g_it.is_local(),
    "dash::local: global iterator does not reference local element");
  // Global iterator to global pointer:
  GlobPtr<ElementType> g_ptr = static_cast< GlobPtr<ElementType> >(g_it);
  // Global pointer to native pointer:
  return static_cast<ElementType*>(g_ptr);
}

} // namespace dash

#endif  // DASH__ALGORITHM__LOCAL_RANGE_H__
