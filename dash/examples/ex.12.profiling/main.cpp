#include <libdash.h>
#include <dash/profiling/TracedPointer.h>

int main(int argc, char* argv[]) {
	dash::init(&argc, &argv);

	const auto myid = dash::myid();
	const auto size = dash::size();

	dash::Array<int> arr(size * 5);

// 	dash::fill(arr.begin(), arr.end(), 42);
// 	auto gptr = arr.data();
	for(auto i = 0; i < 5*size; ++i) {
		arr[i] = i;
	}

	arr.barrier();

	dash::finalize();
	return 0;
}
