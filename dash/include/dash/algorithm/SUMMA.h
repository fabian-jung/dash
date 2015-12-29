#ifndef DASH__ALGORITHM__SUMMA_H_
#define DASH__ALGORITHM__SUMMA_H_

#include <dash/Exception.h>
#include <dash/bindings/LAPACK.h>

namespace dash {

namespace internal {

/**
 * Naive matrix multiplication for local multiplication of matrix blocks,
 * used only for tests and where BLAS is not available.
 */
template<
  typename MatrixTypeA,
  typename MatrixTypeB,
  typename MatrixTypeC
>
void multiply_naive(
  /// Matrix to multiply, extents n x m
  MatrixTypeA & A,
  /// Matrix to multiply, extents m x p
  MatrixTypeB & B,
  /// Matrix to contain the multiplication result, extents n x p,
  /// initialized with zeros
  MatrixTypeC & C,
  size_t        m,
  size_t        n,
  size_t        p)
{
  typedef typename MatrixTypeC::pattern_type pattern_c_type;
  typedef typename MatrixTypeC::value_type   value_type;
  for (auto i = 0; i < n; ++i) {
    // i = 0...n
    for (auto j = 0; j < p; ++j) {
      // j = 0...p
      for (auto k = 0; k < m; ++k) {
        // k = 0...m
        auto a_index = i * m + k;
        auto b_index = k * m + j;
        DASH_LOG_TRACE("dash::summa::multiply",
                       "C(", i, ",", j, ") =",
                       "A[", a_index, "] * B[", b_index, "]");
        C[i][j] += A[a_index] * B[b_index];
      }
    }
  }
} 

} // namespace internal

/// Constraints on pattern blocking properties of matrix operands passed to
/// \c dash::summa.
typedef dash::pattern_blocking_properties<
          // same number of elements in every block
          pattern_blocking_tag::balanced >
        summa_pattern_blocking_constraints;
/// Constraints on pattern topology properties of matrix operands passed to
/// \c dash::summa.
typedef dash::pattern_topology_properties<
          // same amount of blocks for every process
          pattern_topology_tag::balanced,
          // every process mapped in every row/column
          pattern_topology_tag::diagonal >
        summa_pattern_topology_constraints;
/// Constraints on pattern indexing properties of matrix operands passed to
/// \c dash::summa.
typedef dash::pattern_indexing_properties<
          // elements contiguous within block
          pattern_indexing_tag::local_phase >
        summa_pattern_indexing_constraints;

/**
 * Multiplies two matrices using the SUMMA algorithm.
 *
 * Pseudocode:
 *
 *   C = zeros(n,n)
 *   for k = 1:b:n {            // k increments in steps of blocksize b
 *     u = k:(k+b-1)            // u is [k, k+1, ..., k+b-1]
 *     C = C + A(:,u) * B(u,:)  // Multiply n x b matrix from A with
 *                              // b x p matrix from B
 *   }
 */
template<
  typename MatrixTypeA,
  typename MatrixTypeB,
  typename MatrixTypeC
>
void summa(
  /// Matrix to multiply, extents n x m
  MatrixTypeA & A,
  /// Matrix to multiply, extents m x p
  MatrixTypeB & B,
  /// Matrix to contain the multiplication result, extents n x p,
  /// initialized with zeros
  MatrixTypeC & C)
{
  typedef typename MatrixTypeA::value_type   value_type;
  typedef typename MatrixTypeA::index_type   index_type;
  typedef typename MatrixTypeA::pattern_type pattern_a_type;
  typedef typename MatrixTypeB::pattern_type pattern_b_type;
  typedef typename MatrixTypeC::pattern_type pattern_c_type;
  typedef std::array<index_type, 2>          coords_type;

  DASH_LOG_DEBUG("dash::summa()");
  // Verify that matrix patterns satisfy pattern constraints:
  if (!dash::check_pattern_constraints<
         summa_pattern_blocking_constraints,
         summa_pattern_topology_constraints,
         summa_pattern_indexing_constraints
       >(A.pattern())) {
    DASH_THROW(
      dash::exception::InvalidArgument,
      "dash::summa(): "
      "pattern of first matrix argument does not match constraints");
  }
  if (!dash::check_pattern_constraints<
         summa_pattern_blocking_constraints,
         summa_pattern_topology_constraints,
         summa_pattern_indexing_constraints
       >(B.pattern())) {
    DASH_THROW(
      dash::exception::InvalidArgument,
      "dash::summa(): "
      "pattern of second matrix argument does not match constraints");
  }
  if (!dash::check_pattern_constraints<
         summa_pattern_blocking_constraints,
         summa_pattern_topology_constraints,
         summa_pattern_indexing_constraints
       >(C.pattern())) {
    DASH_THROW(
      dash::exception::InvalidArgument,
      "dash::summa(): "
      "pattern of result matrix does not match constraints");
  }
  DASH_LOG_TRACE("dash::summa", "matrix pattern properties valid");

  //    A         B         C
  //  _____     _____     _____
  // |     |   |     |   |     |
  // n     | x m     | = n     |
  // |_ m _|   |_ p _|   |_ p _|
  //
  dash::Team & team = C.team();
  auto unit_id      = team.myid();
  // Check run-time invariants on pattern instances:
  auto pattern_a    = A.pattern();
  auto pattern_b    = B.pattern();
  auto pattern_c    = C.pattern();
  auto m = pattern_a.extent(0); // number of columns in A, rows in B
  auto n = pattern_a.extent(1); // number of rows in A and C
  auto p = pattern_b.extent(0); // number of columns in B and C

  DASH_ASSERT_EQ(
    pattern_a.extent(1),
    pattern_b.extent(0),
    "dash::summa(): "
    "Extents of first operand in dimension 1 do not match extents of"
    "second operand in dimension 0");
  DASH_ASSERT_EQ(
    pattern_c.extent(0),
    pattern_a.extent(0),
    "dash::summa(): "
    "Extents of result matrix in dimension 0 do not match extents of"
    "first operand in dimension 0");
  DASH_ASSERT_EQ(
    pattern_c.extent(1),
    pattern_b.extent(1),
    "dash::summa(): "
    "Extents of result matrix in dimension 1 do not match extents of"
    "second operand in dimension 1");

  DASH_LOG_TRACE("dash::summa", "matrix pattern extents valid");

  // Patterns are balanced, all blocks have identical size:
  auto block_size_m  = pattern_a.block(0).extent(0);
  auto block_size_n  = pattern_b.block(0).extent(1);
  auto block_size_p  = pattern_b.block(0).extent(0);
  auto num_blocks_l  = n / block_size_n;
  auto num_blocks_m  = m / block_size_m;
  auto num_blocks_n  = n / block_size_n;
  auto num_blocks_p  = p / block_size_p;
  // Size of temporary local blocks
  auto block_a_size  = block_size_n * block_size_m;
  auto block_b_size  = block_size_m * block_size_p;
  // Number of units in rows and columns:
  auto teamspec      = C.pattern().teamspec();
  auto num_units_x   = teamspec.extent(0);
  auto num_units_y   = teamspec.extent(1);
  // Coordinates of active unit in team spec (process grid):
  auto team_coords_u = teamspec.coords(unit_id);
  // Block row and column in C assigned to active unit:
  auto block_col_u   = team_coords_u[0];
  auto block_row_u   = team_coords_u[1];

  DASH_LOG_TRACE("dash::summa", "blocks:",
                 "m:", num_blocks_m, "*", block_size_m,
                 "n:", num_blocks_n, "*", block_size_n,
                 "p:", num_blocks_p, "*", block_size_p);
  DASH_LOG_TRACE("dash::summa", "number of units:",
                 "cols:", num_units_x,
                 "rows:", num_units_y);
  DASH_LOG_TRACE("dash::summa", "allocating local temporary blocks, sizes:",
                 "A:", block_a_size,
                 "B:", block_b_size);
  value_type * local_block_a = new value_type[block_a_size];
  value_type * local_block_b = new value_type[block_b_size];

  // Iterate local blocks in matrix C:
  auto num_local_blocks_c = (num_blocks_n * num_blocks_p) / teamspec.size();
  for (auto lb = 0; lb < num_local_blocks_c; ++lb) {
    auto l_block_c      = C.local.block(lb);
    auto l_block_c_view = l_block_c.begin().viewspec();
    // Block coordinate of current local block in matrix C:
    index_type l_block_c_row = l_block_c_view.offset(1) / block_size_n;
    index_type l_block_c_col = l_block_c_view.offset(0) / block_size_p;
    DASH_LOG_TRACE("dash::summa", "local block in C:",
                   "view:",      l_block_c_view,
                   "block row:", l_block_c_row,
                   "block col:", l_block_c_col);
    // Iterate blocks in rows of B:
    for (index_type block_row = 0; block_row < num_blocks_m; ++block_row) {
      // Iterate blocks in columns of A:
      for (index_type block_col = 0; block_col < num_blocks_p; ++block_col) {
        // Create local copy of block in A:
        //
        auto block_a = A.block(coords_type { block_col, l_block_c_row });
        DASH_LOG_TRACE("dash::summa", "creating local copy of A.block:",
                       "col:",  block_col,
                       "row:",  l_block_c_row,
                       "view:", block_a.begin().viewspec());
        dash::copy(block_a.begin(), block_a.end(), local_block_a);
        // Create local copy of block in B:
        //
        auto block_b = B.block(coords_type { l_block_c_col, block_row });
        DASH_LOG_TRACE("dash::summa", "creating local copy of B.block:",
                       "col:",  l_block_c_col,
                       "row:",  block_col,
                       "view:", block_b.begin().viewspec());
        dash::copy(block_b.begin(), block_b.end(), local_block_b);
        // Multiply local block matrices:
        //
        DASH_LOG_TRACE("dash::summa",
                       "multiplying local block matrices");
        dash::internal::multiply_naive(
            local_block_a,
            local_block_b,
            l_block_c,
            block_size_m,
            block_size_n,
            block_size_p);
        DASH_LOG_TRACE("dash::summa",
                       "multiplication of local blocks finished");
      }
    }
  } // for lb

#if ORIGINAL_IMPL
  // Perform block matrix multiplication:
  //
  //   C_ij = sum_(k:1..np) { A_ik x B_kj }
  //
  for (int block_k = 0; block_k < num_blocks_l; ++block_k) 
  {
    for (int block_i = 0; block_i < num_blocks_m; block_i += num_units_y) 
    {
      // Local copy of block A[i+u][k]:
      DASH_LOG_TRACE("dash::summa", "local copy of A.block[i+ru][k]",
                     "i:",    block_i,
                     "i+ru:", block_i + block_row_u,
                     "k:",    block_k);
      auto block_a = A.block(coords_type { block_i + block_row_u,
                                           block_k });
      DASH_LOG_TRACE_VAR("dash::summa", block_a.begin().viewspec());
      dash::copy(block_a.begin(),
                 block_a.end(),
                 local_block_a);
      DASH_LOG_TRACE("dash::summa", "created local copy of A.block");
      // Multiply block A[i][k] with blocks B[k][*]
      for (int block_j = 0; block_j < num_blocks_n; block_j += num_units_x) 
      {
        // Local copy of block B[k][j+u]:
        DASH_LOG_TRACE("dash::summa", "local copy of B.block[k][j+cu]",
                       "j:",    block_j,
                       "k:",    block_k,
                       "j+cu:", block_j + block_col_u);
        auto block_b = B.block(coords_type { block_k,
                                             block_j + block_col_u });
        DASH_LOG_TRACE_VAR("dash::summa", block_b.begin().viewspec());
        dash::copy(block_b.begin(),
                   block_b.end(),
                   local_block_b);
        DASH_LOG_TRACE("dash::summa", "created local copy of B.block");
        // Local result block:
        DASH_LOG_TRACE("dash::summa",
                       "multiplying A.block[i+ru][k] and B.block[k][j+cu]",
                       "i:",    block_i,
                       "j:",    block_j,
                       "i+ru:", block_i + block_row_u,
                       "k:",    block_k,
                       "j+cu:", block_j + block_col_u,
                       "to C.block[i+ru][j+cu]");
        auto block_c = C.block(coords_type { block_i + block_row_u,
                                             block_j + block_col_u });
        DASH_LOG_TRACE_VAR("dash::summa", block_c.begin().viewspec());
        // Plausibility check: first element in block expected to be local.
        DASH_ASSERT_MSG(block_c.is_local(0),
                        "block in result matrix is not local");
        DASH_LOG_TRACE("dash::summa",
                       "calculating block matrix product "
                       "C.block = A.block x B.block");
        // Multiply local block matrices:
        dash::internal::multiply_naive(
            local_block_a,
            local_block_b,
            block_c,
            m, n, p);
      } // for j
    } // for i
  } // for k
#endif

  delete[] local_block_a;
  delete[] local_block_b;

  dash::barrier();
}

/**
 * Registration of \c dash::summa as an implementation of matrix-matrix
 * multiplication (xDGEMM).
 *
 * Delegates  \c dash::multiply<MatrixType>
 * to         \c dash::summa<MatrixType>
 * if         \c MatrixType::pattern_type
 * satisfies the pattern property constraints of the SUMMA implementation.
 */
template<
  typename MatrixTypeA,
  typename MatrixTypeB,
  typename MatrixTypeC
>
typename std::enable_if<
  dash::pattern_constraints<
    dash::summa_pattern_blocking_constraints,
    dash::summa_pattern_topology_constraints,
    dash::summa_pattern_indexing_constraints,
    typename MatrixTypeA::pattern_type
  >::satisfied::value &&
  dash::pattern_constraints<
    dash::summa_pattern_blocking_constraints,
    dash::summa_pattern_topology_constraints,
    dash::summa_pattern_indexing_constraints,
    typename MatrixTypeB::pattern_type
  >::satisfied::value &&
  dash::pattern_constraints<
    dash::summa_pattern_blocking_constraints,
    dash::summa_pattern_topology_constraints,
    dash::summa_pattern_indexing_constraints,
    typename MatrixTypeC::pattern_type
  >::satisfied::value,
  void
>::type
multiply(
  /// Matrix to multiply, extents n x m
  MatrixTypeA & A,
  /// Matrix to multiply, extents m x p
  MatrixTypeB & B,
  /// Matrix to contain the multiplication result, extents n x p,
  /// initialized with zeros
  MatrixTypeC & C)
{
  dash::summa(A, B, C);
}

} // namespace dash

#endif
