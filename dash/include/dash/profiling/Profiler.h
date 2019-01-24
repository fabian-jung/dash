#pragma once

#include <iostream>
#include <vector>
#include <dash/Init.h>
// #include <dash/GlobRef.h>

namespace dash {

template<typename T>
class GlobRefImpl;

class Profiler {
public:

	enum class counter_t : size_t {
		REF_READ,
		REF_WRITE,
		SIZE
	};

	static Profiler& get() {
		static Profiler self;
		return self;
	}

	~Profiler() {
		std::cout << "Profiler overview for unit " << myid << ":\n";
		std::cout << "GlobPtr Operations:\n";
		std::cout << "GlobRef Operations:\n";
		std::cout << "Reads:\n";
		for(size_t i = 0; i < globRefCounter.read.size(); ++i) {
			std::cout << "[" << i << "]: " << globRefCounter.read[i] << "\n";
		}
		std::cout << "Writes:\n";
		for(size_t i = 0; i < globRefCounter.read.size(); ++i) {
			std::cout << "[" << i << "]: " << globRefCounter.write[i] << "\n";
		}
		std::cout << std::endl;
	}


	template<class T>
	void trackRead(const GlobRefImpl<T>& ref) {
		const auto uid = ref.dart_gptr().unitid;
		++(globRefCounter.read[uid]);
	}

	template<class T>
	void trackWrite(const GlobRefImpl<T>& ref) {
		const auto uid = ref.dart_gptr().unitid;
		++(globRefCounter.write[uid]);
	}

private:

	const size_t myid;


	Profiler() :
		myid(dash::myid())
	{
		globRefCounter.read.resize(dash::size());
		globRefCounter.write.resize(dash::size());
	}

	struct {
		std::vector<size_t> read;
		std::vector<size_t> write;
	} globRefCounter;

	Profiler(const Profiler& cpy) = delete;
	Profiler& operator=(const Profiler& cpy) = delete;

	Profiler(Profiler&& mov) = delete;
	Profiler& operator=(Profiler&& mov) = delete;

	std::array<size_t, static_cast<size_t>(counter_t::SIZE)> _counter {};

	size_t& counter(counter_t c) {
		return _counter[static_cast<size_t>(c)];
	}

};

}
