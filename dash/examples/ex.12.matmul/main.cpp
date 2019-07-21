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
		std::chrono::seconds(3600),
		15,//4096,
		&dash::barrier,
		[]() { return dash::myid()==0; },
		[]() { return dash::size(); }
	);

	const auto myid = dash::myid();
	const auto size = dash::size();
	using value_type = double;
	std::vector<size_t> dims = {
		128,
		181,
		256,
		362,
		512,
		724,
		1024,
		1448,
		2048,
		2896,
		4096,
		5792,
		8096,
		11585,
		16384
	};


/*


	std::random_device rd;  //Will be used to obtain a seed for the random number engine
	std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
	uniform_distribution<int>::type dis(-10, 10);

	dash::generate(lhs.begin(), lhs.end(), std::bind(dis, gen));
	dash::generate(rhs.begin(), rhs.end(), std::bind(dis, gen));*/

	profiler.reset();
	dash::Team::All().barrier();
//
	for(const auto dim : dims) {
		const auto rows_per_unit = (dim+size-1)/size;
		const auto array_length = rows_per_unit*dim*size;

		dash::Array<value_type> lhs(array_length);
		profiler.container_set_name(lhs, "lhs");
		dash::Array<value_type> rhs(array_length);
		profiler.container_set_name(rhs, "rhs");
		dash::Array<value_type> res4(array_length);
		profiler.container_set_name(res4, "res4");

		dash::fill(lhs.begin(), lhs.end(), 0);
		dash::fill(rhs.begin(), rhs.end(), 0);
		if(myid == 0) {
			for(unsigned int i = 0; i < dim; ++i) {
				lhs[i*dim+i] = 1;
				rhs[i*dim+i] = 1;
			}
		}
		dash::barrier();

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
	}
//

	dash::finalize();
	return 0;
}
