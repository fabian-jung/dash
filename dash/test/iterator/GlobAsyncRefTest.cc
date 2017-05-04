
#include "GlobAsyncRefTest.h"

#include <dash/GlobAsyncRef.h>
#include <dash/Array.h>
#include <dash/Mutex.h>
#include <dash/Algorithm.h>


TEST_F(GlobAsyncRefTest, IsLocal) {
  int num_elem_per_unit = 20;
  // Initialize values:
  dash::Array<int> array(dash::size() * num_elem_per_unit);
  for (auto li = 0; li < array.lcapacity(); ++li) {
    array.local[li] = dash::myid().id;
  }
  array.barrier();
  // Test global async references on array elements:
  auto global_offset      = array.pattern().global(0);
  // Reference first local element in global memory:
  dash::GlobRef<int> gref = array[global_offset];
  dash::GlobAsyncRef<int> gar_local_g(gref);
  ASSERT_EQ_U(true, gar_local_g.is_local());
}

TEST_F(GlobAsyncRefTest, GetSet) {
  // Initialize values:
  dash::Array<int> array(dash::size());
  for (auto li = 0; li < array.lcapacity(); ++li) {
    array.local[li] = dash::myid().id;
  }
  array.barrier();

  int neighbor = (dash::myid() + 1) % dash::size();

  // Reference a neighbors element in global memory:
  dash::GlobAsyncRef<int> garef = array.async[neighbor];

  int val = garef.get();
  garef.flush();
  ASSERT_EQ_U(neighbor, val);

  val = 0;

  garef.get(val);
  garef.flush();
  ASSERT_EQ_U(neighbor, val);

  array.barrier();
  garef.set(dash::myid());
  ASSERT_EQ_U(static_cast<int>(garef), dash::myid().id);
  garef.flush();
  array.barrier();
  garef.put(dash::myid());
  ASSERT_EQ_U(static_cast<int>(garef), dash::myid().id);
  garef.flush();
  array.barrier();
  int left_neighbor = (dash::myid() + dash::size() - 1) % dash::size();
  ASSERT_EQ_U(left_neighbor, array.local[0]);
}

TEST_F(GlobAsyncRefTest, Conversion)
{
  // Initialize values:
  dash::Array<int> array(dash::size());
  for (auto li = 0; li < array.lcapacity(); ++li) {
    array.local[li] = dash::myid().id;
  }
  array.barrier();

  auto gref_async = static_cast<dash::GlobAsyncRef<int>>(
                        array[dash::myid().id]);
  auto gref_sync  = static_cast<dash::GlobRef<int>>(
                        array.async[dash::myid().id]);
  ASSERT_EQ_U(gref_async.is_local(), true);
  ASSERT_EQ_U(gref_sync.is_local(), true);
}

struct mytype {int a; double b; };

std::ostream&
operator<<(std::ostream& os, mytype const s) {
  os << "{" << "a: " << s.a << ", b:" << s.b << "}";
  return os;
}

TEST_F(GlobAsyncRefTest, RefOfStruct)
{
  if(dash::size() < 2){
    SKIP_TEST_MSG("this test requires at least 2 units");
  }

  dash::Array<mytype> array(dash::size());

  int neighbor = (dash::myid() + 1) % dash::size();
  // Reference a neighbors element in global memory:
  auto garef_rem = array.async[neighbor];
  auto garef_loc = array.async[dash::myid().id];

  {
    auto garef_a_rem = garef_rem.member<int>(&mytype::a);
    auto garef_b_rem = garef_rem.member<double>(&mytype::b);

    auto garef_a_loc = garef_loc.member<int>(&mytype::a);
    auto garef_b_loc = garef_loc.member<double>(&mytype::b);

    ASSERT_EQ_U(garef_rem.is_local(), false);
    ASSERT_EQ_U(garef_a_rem.is_local(), false);
    ASSERT_EQ_U(garef_b_rem.is_local(), false);

    ASSERT_EQ_U(garef_loc.is_local(), true);
    ASSERT_EQ_U(garef_a_loc.is_local(), true);
    ASSERT_EQ_U(garef_b_loc.is_local(), true);
  }
  array.barrier();
  {
    mytype data {1, 2.0};
    garef_rem = data;
    auto garef_a_rem = garef_rem.member<int>(&mytype::a);
    auto garef_b_rem = garef_rem.member<double>(&mytype::b);

    // GlobRefAsync is constructed after data is set, so it stores value
    int a = garef_a_rem;
    int b = garef_b_rem;
    ASSERT_EQ_U(a, 1);
    ASSERT_EQ_U(b, 2.0);
  }

}

TEST_F(GlobAsyncRefTest, ContainerFlush) {
  dash::Array<int> array(dash::size());
  *(array.lbegin()) = 0;
  array.barrier();
  constexpr int num_iter = 5;

  dash::Mutex mutex;

  // async writes to array.async
  for (int i = 0; i < num_iter; ++i) {
    mutex.lock();
    int tmp = array.async[0];
    for (int j = 0; j < num_iter; ++j) {
      tmp += 1;
      array.async[0] = tmp;
    }
    array.flush();
    mutex.unlock();
  }
  array.barrier();
  ASSERT_EQ_U(num_iter*num_iter*dash::size(), (int)array[0]);

}

TEST_F(GlobAsyncRefTest, FutureTest) {
  using value_t = int;

  dash::Array<value_t> array(dash::size());

  dash::fill(array.begin(), array.end(), dash::myid().id);
  array.barrier();

  int lneighbor = (dash::myid() + dash::size() - 1) % dash::size();
  auto gref = array[lneighbor];
  dash::Future<dash::GlobRef<value_t>> gref_fut = gref;

  gref_fut.test();
  gref_fut.wait();
  ASSERT_EQ_U(lneighbor, gref_fut.get());


  int rneighbor = (dash::myid() + 1) % dash::size();
  auto agref = array.async[rneighbor];
  dash::Future<dash::GlobRef<value_t>> agref_fut = agref;

  agref_fut.wait();
  ASSERT_EQ_U(rneighbor, agref_fut.get());

}

