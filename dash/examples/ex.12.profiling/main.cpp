#include <random>
#include <libdash.h>
#include <dash/profiling/TracedPointer.h>

template <class D, bool b = std::is_integral<D>::value>
struct uniform_distribution;


template <class D>
struct uniform_distribution<D, true> {
	using type = std::uniform_int_distribution<D>;
};

template <class D>
struct uniform_distribution<D, false> {
	using type = std::uniform_real_distribution<D>;
};

template <class T>
auto get(const size_t dim, dash::Array<T>& arr, size_t i, size_t j) {
	return arr[i*dim + j];
}


int main(int argc, char* argv[]) {
	dash::init(&argc, &argv);
	auto& profiler = dash::Profiler::get();

	const auto myid = dash::myid();
	const auto size = dash::size();
	const size_t dim = 3*size;


	dash::Array<int> lhs(dim*dim);
	dash::Array<int> rhs(dim*dim);
	dash::Array<int> res(dim*dim);


	profiler.report("Init");

	dash::fill(res.begin(), res.end(), 0);

	std::random_device rd;  //Will be used to obtain a seed for the random number engine
	std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
	uniform_distribution<int>::type dis(-10, 10);

	dash::generate(lhs.begin(), lhs.end(), std::bind(dis, gen));
	dash::generate(rhs.begin(), rhs.end(), std::bind(dis, gen));

	dash::Team::All().barrier();
	profiler.report("Fill");


// 	dash::fill(arr.begin(), arr.end(), 42);
// 	auto gptr = arr.data();

	const auto stride = dim/size;

	for(size_t i = myid*stride; i < (myid+1)*stride; ++i) {
		for(size_t j = 0; j < dim; ++j) {
			for(size_t k = 0; k < dim; ++k) {
				get(dim, res, i, j) += get(dim, lhs, i, k) * get(dim, rhs, k, j);
			}
		}
	}

	for(auto it = res.lbegin(); it != res.lend(); ++it) {
		std::cout << *it << ", ";
	}
	std::cout << std::endl;
	dash::Team::All().barrier();
	profiler.report("Multiplication");

	dash::finalize();
	return 0;
}
