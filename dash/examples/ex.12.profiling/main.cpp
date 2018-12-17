#include <random>
#include <libdash.h>
#include <dash/profiling/TracedPointer.h>
#include <dash/Onesided.h>

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


class stop_watch_t {
	using clock = std::chrono::high_resolution_clock;
	using desc = std::chrono::duration<double>;
	using tp = std::chrono::time_point<clock, desc>;

	const desc execution_time;
	const size_t max_runs;
	const char* delim = "	,";
	std::function<void()> barrier;
	std::function<bool()> rank_checker;
	std::function<size_t()> world_size;

public:
	stop_watch_t(
		desc execution_time = std::chrono::seconds(600),
		size_t max_runs = 256,
		std::function<void()>  barrier = [](){},
		std::function<bool()> rank_checker = [](){ return true; },
		std::function<size_t()> world_size = []() -> size_t { return 1; }
	) :
		execution_time(execution_time),
		max_runs(max_runs),
		barrier(barrier),
		rank_checker(rank_checker),
		world_size(world_size)
	{
		if(rank_checker()) {
			std::cout
				<< "bench name" << delim
				<< "worldsize" << delim
				<< " runs " << delim
				<< " avg(s) " << delim
				<< " median(s) " << delim
				<< " min(s) " << delim
				<< " max(s) " << delim
				<< " stdev(s) " << delim << "\n";
		}
	}


	template <class T>
	void run(const char * name, T function)
	{
		std::vector<double> times;
		times.reserve(max_runs);
		barrier();
		tp begin = clock::now();
		tp now;
		char finished = true;;
		do {
			barrier();
			tp b = clock::now();
			function();
			barrier();
			now = clock::now();
			times.push_back(std::chrono::duration_cast<desc>(now- b).count());
			finished = (now - begin < execution_time) && (times.size() < max_runs);
			dart_bcast(
				&finished,
				1,
				dash::dart_datatype<decltype(finished)>::value,
				0,
				dash::Team::All().dart_id()
			);
		} while(finished);
		// Sort for median and higher floating point precision while adding
		std::sort(times.begin(), times.end());
		const auto avg = std::accumulate(times.begin(), times.end(), 0.0) / static_cast<double>(times.size());
		const auto median =
			(times.size() % 2 == 1) ?
				times[times.size()/2] :
				(times[times.size()/2-1]+times[times.size()/2])/2;
		const auto minmax = std::minmax_element(times.begin(), times.end());
		const auto var =
			std::accumulate(
				times.begin(),
				times.end(),
				0.0,
				[avg](double lhs, double rhs) -> double {
					return lhs + std::pow(rhs - avg, 2);
				}
			);
		const auto stdev = std::sqrt(var / times.size());
		if(rank_checker()) {
			std::cout.precision(3);
			std::cout << std::scientific
				<< name << delim
				<< world_size() << delim
				<< times.size() << delim
				<< avg << delim
				<< median << delim
				<< *(minmax.first) << delim
				<< *(minmax.second) << delim
				<< stdev << delim << "\n";
		}
	}

};


int main(int argc, char* argv[]) {
	dash::init(&argc, &argv);
	decltype(auto) profiler = dash::Profiler::get();

	stop_watch_t sw(
		std::chrono::seconds(600),
		17,//4096,
		&dash::barrier,
		[]() { return dash::myid()==0; },
		[]() { return dash::size(); }
	);

	const auto myid = dash::myid();
	const auto size = dash::size();
// 	const size_t dim = 512;
	const size_t max_dim = 2048;


	dash::Array<int> lhs(max_dim*max_dim);
	profiler.container_set_name(lhs, "lhs");
	dash::Array<int> rhs(max_dim*max_dim);
	profiler.container_set_name(rhs, "rhs");
// 	dash::Array<int> res(max_dim*max_dim);
// 	profiler.container_set_name(res, "res");
// 	dash::Array<int> res2(max_dim*max_dim);
// 	profiler.container_set_name(res2, "res2");
	dash::Array<int> res3(max_dim*max_dim);
	profiler.container_set_name(res3, "res3");
	dash::Array<int> res4(max_dim*max_dim);
	profiler.container_set_name(res4, "res4");
// 	profiler.report("Init");

// 	dash::fill(res.begin(), res.end(), 0);
// 	dash::fill(res2.begin(), res2.end(), 0);
//	dash::fill(res3.begin(), res3.end(), static_cast<int>(0));
//	dash::fill(res4.begin(), res4.end(), static_cast<int>(0));
	std::fill(res3.lbegin(), res3.lend(), 0);
	std::fill(res4.lbegin(), res4.lend(), 0);

	std::random_device rd;  //Will be used to obtain a seed for the random number engine
	std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
	uniform_distribution<int>::type dis(-10, 10);

	dash::generate(lhs.begin(), lhs.end(), std::bind(dis, gen));
	dash::generate(rhs.begin(), rhs.end(), std::bind(dis, gen));

	dash::Team::All().barrier();
	profiler.reset();


// 	dash::fill(arr.begin(), arr.end(), 42);
// 	auto gptr = arr.data();

//
// 	{
// 		for(size_t i = myid*stride; i < (myid+1)*stride; ++i) {
// 			for(size_t j = 0; j < dim; ++j) {
// 				for(size_t k = 0; k < dim; ++k) {
// 					res[i*dim+j] += lhs[k+i*dim] * get(dim, rhs, k, j);
// 				}
// 			}
// 		}
// 	}
// 	profiler.report("Multiplication");
// 	dash::Team::All().barrier();
//
// 	{
// 		for(size_t i = 0; i < stride; ++i) {
// 			for(size_t j = 0; j < dim; ++j) {
// 				for(size_t k = 0; k < dim; ++k) {
// 					res2.lbegin()[i*dim+j] += lhs.lbegin()[k+i*dim] * get(dim, rhs, k, j);
// 				}
// 			}
// 		}
// 	}

// 	profiler.report("Multiplication_local");
	dash::Team::All().barrier();
//
	std::vector<int> rhs_line(max_dim);
	sw.run(
		(std::string("mul_local_cached_")+std::to_string(max_dim)).c_str(),
		[&](){
			constexpr auto dim = max_dim;
			const auto stride = dim/size;
			for(size_t k_rank = 0; k_rank < size; ++k_rank) {
				if(k_rank != myid) {
					for(size_t k = k_rank*stride; k < (k_rank+1)*stride; ++k) {
						dash::internal::get(rhs[k*dim].dart_gptr(), rhs_line.data(), dim);
						for(size_t i = 0; i < stride; ++i) {
							for(size_t j = 0; j < dim; ++j) {
								res4.lbegin()[i*dim+j] += lhs.lbegin()[k+i*dim] * rhs_line[j];
							}
						}
					}
				} else {
					for(size_t i = 0; i < stride; ++i) {
						for(size_t k = 0; k < stride; ++k) {
							for(size_t j = 0; j < dim; ++j) {
								res4.lbegin()[i*dim+j] += lhs.lbegin()[(k+myid*stride)+i*dim] * rhs.lbegin()[k*dim+j];
							}
						}
					}
				}
			}
		}
	);
//	profiler.report("Multiplication_all_local_cached");

	sw.run(
		("mul_local_"+std::to_string(max_dim/2)+"^2").c_str(),
		[&](){
			constexpr auto dim = max_dim/2;
		   	const auto stride = dim/size;
			for(size_t j = 0; j < dim; ++j) {
				for(size_t k_rank = 0; k_rank < size; ++k_rank) {
					if(k_rank != myid) {
						for(size_t k = k_rank*stride; k < (k_rank+1)*stride; ++k) {
							int r;
							dash::internal::get(get(dim, rhs, k, j).dart_gptr(), &r, 1);
							for(size_t i = 0; i < stride; ++i) {
								res3.lbegin()[i*dim+j] += lhs.lbegin()[k+i*dim] * r;
							}
						}
					} else {
						for(size_t i = 0; i < stride; ++i) {
							for(size_t k = 0; k < stride; ++k) {
								res3.lbegin()[i*dim+j] += lhs.lbegin()[(k+myid*stride)+i*dim] * rhs.lbegin()[k*dim+j];
							}
						}
					}
				}
			}
		}
	);
//	profiler.report("Multiplication_all_local");

//
// 	if(myid == 0) {
// 		for(auto it = res.begin(); it != res.end(); ++it) {
// 			std::cout << static_cast<int>(*it) << ", ";
// 		}
// 		std::cout << std::endl;
// 		for(auto it = res2.begin(); it != res2.end(); ++it) {
// 			std::cout << static_cast<int>(*it) << ", ";
// 		}
// 		std::cout << std::endl;
// 		for(auto it = res3.begin(); it != res3.end(); ++it) {
// 			std::cout << static_cast<int>(*it) << ", ";
// 		}
// 		std::cout << std::endl;
// 	}
// 	if(
// 		!(
// // 			std::equal(res.begin(), res.end(), res2.begin()) &&
// // 			std::equal(res.begin(), res.end(), res3.begin()) &&
// 			std::equal(res3.begin(), res3.end(), res4.begin())
// 		)
//
// 	) {
// 		return 1;
// 	}
// 	dash::Team::All().barrier();

	dash::finalize();
	return 0;
}
