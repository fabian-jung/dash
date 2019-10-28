#include <chrono>
#include <iostream>
#include <libdash.h>
#include <algorithm>

using high_res_clock = std::chrono::high_resolution_clock;
using duration_t = std::chrono::duration<double>;
using time_point_t = std::chrono::time_point<high_res_clock, duration_t>;

constexpr size_t elem_per_unit = 8096;;

struct stopwatch {
	template <class function_t>
	static void run(std::string name, size_t runs, function_t function) {
		std::vector<duration_t> times;
		for(decltype(runs) i = 0; i < runs; ++i) {
			const time_point_t begin = high_res_clock::now();
			function();
			const time_point_t end = high_res_clock::now();
			times.emplace_back(end-begin);
		}
		std::sort(times.begin(), times.end());
		const auto min = *times.begin();
		const auto median = times[times.size()/2];
		const auto max = *times.rbegin();

		std::cout << name << " took median " << median.count()/elem_per_unit
						  << " s, min " << min.count()/elem_per_unit
						  << " s, max " << max.count()/elem_per_unit << " s." << std::endl;
	}
};

class touch {
public:
	static void put(double v) {
		value = v;
	}

	static double get() {
		return value;
	}

	static double value;
};

double touch::value = 0;

int main(int argc, char* argv[]) {
	dash::init(&argc, &argv);

	const auto world_size = dash::size();

	dash::Array<double> array(elem_per_unit*world_size);
	std::fill(array.lbegin(), array.lend(), 5);
	array.barrier();

	if(dash::myid() == 0) {
	stopwatch::run("local read", 11,
		[&]() {
			double res = std::accumulate(array.lbegin(), array.lend(), 0);
			touch::put(res);
			return res;
		}
	);

// 	std::cout << touch::get()/elem_per_unit << std::endl;
	stopwatch::run("local write", 11,
		[&]() {
			return std::fill(array.lbegin(), array.lend(), 0);
		}
	);

	stopwatch::run("global read, local data", 11,
		[&]() {
			return std::accumulate(array.begin(), array.begin()+elem_per_unit, 0);
		}
	);

	stopwatch::run("global write, local data", 11,
		[&]() {
			return std::fill(array.begin(), array.begin()+elem_per_unit, 0);
		}
	);

	stopwatch::run("global read, local data", 11,
		[&]() {
			return std::accumulate(array.begin(), array.begin()+elem_per_unit, 0);
		}
	);

	stopwatch::run("global write, same cpu", 11,
		[&]() {
			return std::fill(array.begin(), array.begin()+elem_per_unit, 0);
		}
	);

	stopwatch::run("global read, same cpu", 11,
		[&]() {
			return std::accumulate(array.begin()+elem_per_unit, array.begin()+2*elem_per_unit, 0);
		}
	);

	stopwatch::run("global write, same socket", 11,
		[&]() {
			return std::fill(array.begin(), array.begin()+elem_per_unit, 0);
		}
	);

	stopwatch::run("global read, same socket", 11,
		[&]() {
			return std::accumulate(array.begin()+12*elem_per_unit, array.begin()+13*elem_per_unit, 0);
		}
	);

	stopwatch::run("global write, different socket", 11,
		[&]() {
			return std::fill(array.begin(), array.begin()+elem_per_unit, 0);
		}
	);

	stopwatch::run("global read, different socket", 11,
		[&]() {
			return std::accumulate(array.begin()+24*elem_per_unit, array.begin()+24*elem_per_unit, 0);
		}
	);

	}


	dash::barrier();
	dash::finalize();

	return 0;
}

/*
#include <random>
#include <libdash.h>
#include <dash/profiling/TracedPointer.h>
#include <dash/Onesided.h>

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
		size_t max_runs = 1,
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
		std::chrono::seconds(3600),
		1,//4096,
		&dash::barrier,
		[]() { return dash::myid()==0; },
		[]() { return dash::size(); }
	);

	const auto myid = dash::myid();
	const auto size = dash::size();
	using value_type = double;
	const size_t dim = 512;
	const auto rows_per_unit = (dim+size-1)/size;


	const auto array_length = rows_per_unit*dim*size;

	dash::Array<value_type> lhs(array_length);
	profiler.container_set_name(lhs, "lhs");
	dash::Array<value_type> rhs(array_length);
	profiler.container_set_name(rhs, "rhs");
// 	dash::Array<value_type> res(array_length);
// 	profiler.container_set_name(res, "res");
	dash::Array<value_type> res3(array_length);
	profiler.container_set_name(res3, "res3");
	dash::Array<value_type> res4(array_length);
	profiler.container_set_name(res4, "res4");
// 	profiler.report("Init");

	dash::fill(lhs.begin(), lhs.end(), 0);
	dash::fill(rhs.begin(), rhs.end(), 0);

	if(myid == 0) {
		for(unsigned int i = 0; i < dim; ++i) {
			lhs[i*dim+i] = 1;
			rhs[i*dim+i] = 1;
		}
	}
	dash::barrier();
	profiler.reset();

// 	if(myid == 0)
// 	{
// 		dash::fill(res.begin(), res.end(), 0);
// 		for(size_t i = 0; i < dim; ++i) {
// 			for(size_t j = 0; j < dim; ++j) {
// 				for(size_t k = 0; k < dim; ++k) {
// 					res[i*dim+j] += lhs[k+i*dim] * rhs[k*dim+j];
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
	std::vector<value_type> rhs_line(dim);
	sw.run(
		(std::string("mul_local_cached_")+std::to_string(dim)).c_str(),
		[&](){
			std::fill(res4.lbegin(), res4.lend(), 0);

			const auto imax = std::min(rows_per_unit, dim-(myid*rows_per_unit));
			for(size_t k_rank = 0; k_rank < size; ++k_rank) {
				const auto kmax = std::min((k_rank+1)*rows_per_unit, dim);
				if(k_rank != myid) {
					for(size_t k = k_rank*rows_per_unit; k < kmax; ++k) {
						dash::internal::get(rhs[k*dim].dart_gptr(), rhs_line.data(), dim);
						for(size_t i = 0; i < imax; ++i) {
							for(size_t j = 0; j < dim; ++j) {
								res4.lbegin()[i*dim+j] += lhs.lbegin()[k+i*dim] * rhs_line[j];
							}
						}
					}
				} else {
					for(size_t i = 0; i < imax; ++i) {
						for(size_t k = k_rank*rows_per_unit; k < kmax; ++k) {
							for(size_t j = 0; j < dim; ++j) {
								res4.lbegin()[i*dim+j] += lhs.lbegin()[k+i*dim] * rhs.lbegin()[(k-k_rank*rows_per_unit)*dim+j];
							}
						}
					}
				}
			}
		}
	);
//	profiler.report("Multiplication_all_local_cached");

	sw.run(
		("mul_local_"+std::to_string(dim)+"^2").c_str(),
		[&](){
			std::fill(res3.lbegin(), res3.lend(), 0);
			const auto imax = std::min(rows_per_unit, dim-(myid*rows_per_unit));
			for(size_t j = 0; j < dim; ++j) {
				for(size_t k_rank = 0; k_rank < size; ++k_rank) {
					const auto kmax = std::min((k_rank+1)*rows_per_unit, dim);

					if(k_rank != myid) {
						for(size_t k = k_rank*rows_per_unit; k < kmax; ++k) {
							value_type r;
							dash::internal::get(rhs[dim*k+j].dart_gptr(), &r, 1);
							for(size_t i = 0; i < imax; ++i) {
								res3.lbegin()[i*dim+j] += lhs.lbegin()[k+i*dim] * r;
							}
						}
					} else {
						for(size_t i = 0; i < imax; ++i) {
							for(size_t k = k_rank*rows_per_unit; k < kmax; ++k) {
								res3.lbegin()[i*dim+j] += lhs.lbegin()[k+i*dim] * rhs.lbegin()[(k-k_rank*rows_per_unit)*dim+j];
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
// 		std::vector<dash::Array<value_type>*> results {&res, &res3, &res4};
// 		for(auto& r : results) {
// 			std::ostream_iterator<value_type> out(std::cout, " ");
// 			for(auto i = 0; i < rows_per_unit*size; ++i) {
// 				auto begin = r->begin() + i*dim;
// 				std::copy(begin, begin+dim, out);
// 				std::cout << std::endl;
// 			}
// 			std::cout << std::endl;
// 		}
// 	}
// 	if(
// 		!(
// 			std::equal(res.begin(), res.end(), res3.begin()) &&
// 			std::equal(res.begin(), res.end(), res4.begin())
// 		)
// 	) {
// 		std::cerr << "ERROR: Matmul implementation is not correct." << std::endl;
// 		return 1;
// 	}
// 	dash::Team::All().barrier();

	dash::finalize();
	return 0;
}

*/
