#pragma once

#include <iostream>
#include <sstream>
#include <vector>
#include <dash/Init.h>
#include <dash/profiling/traits.h>
// #include <dash/GlobRef.h>

namespace dash {

template<typename T>
class GlobRefImpl;

template<typename T, class MemSpaceT>
class GlobPtrImpl;

template <bool enabled>
class ProfilerImpl;

template<>
class ProfilerImpl<false> {


};

template <>
class ProfilerImpl<true> {
public:

	enum class counter_t : size_t {
		GLOB_REF_READ,
		GLOB_REF_WRITE,
		ONESIDEDED_PUT_NUM,
		ONESIDEDED_PUT_BYTE,
		ONESIDEDED_GET_NUM,
		ONESIDEDED_GET_BYTE,
		GLOB_PTR_LEAKED,
		GLOB_PTR_DEREF,
		LOC_PTR_DEREF,
		SIZE
	};

	static ProfilerImpl& get() {
		static ProfilerImpl self;
		return self;
	}

	~ProfilerImpl() {
		report("Final");
	}

	void trackOnesidedPut(size_t bytes, size_t id) {
		counter(counter_t::ONESIDEDED_PUT_BYTE, id) += bytes;
		++counter(counter_t::ONESIDEDED_PUT_NUM, id);
	}

	void trackOnesidedGet(size_t bytes, size_t id) {
		counter(counter_t::ONESIDEDED_GET_BYTE, id) += bytes;
		++counter(counter_t::ONESIDEDED_GET_NUM, id);
	}

	template<class T>
	void trackRead(const GlobRefImpl<T>& ref) {
		const auto uid = ref.dart_gptr().unitid;
		++counter(counter_t::GLOB_REF_READ, uid);
	}

	template<class T>
	void trackWrite(const GlobRefImpl<T>& ref) {
		const auto uid = ref.dart_gptr().unitid;
		++counter(counter_t::GLOB_REF_WRITE, uid);
	}

	template <class T, class MemSpaceT>
	void trackDeref(const GlobPtrImpl<T, MemSpaceT>& ptr) {
		const auto uid = ptr.dart_gptr().unitid;
		++counter(counter_t::GLOB_REF_WRITE, uid);
	}

	template <class T>
	void trackDeref(const T* ptr) {
		// Native/local Ptr deref
		++counter(counter_t::LOC_PTR_DEREF, myid);
	}

	void report(std::string stage) {
		std::stringstream s;
		s << "Profiler overview for stage [" << stage << "] and unit " << myid << ":\n";
		s << "GlobPtr Operations:\n";
		s << "	Dereference Operations:\n";
		for(size_t i = 0; i < size; ++i) {
			s << "		[" << (i!=myid ? std::to_string(i) : "S")  << "]: " << counter(counter_t::GLOB_PTR_DEREF, i) << "\n";
		}
		s << "Local Pointer Operations:\n";
		s << "	Dereference Operations: " << counter(counter_t::LOC_PTR_DEREF, myid) << "\n";
		s << "GlobRef Operations:\n";
		s << "	Reads:\n";
		for(size_t i = 0; i < size; ++i) {
			s << "		[" << (i!=myid ? std::to_string(i) : "S")  << "]: " << counter(counter_t::GLOB_REF_READ, i) << "\n";
		}
		s << "	Writes:\n";
		for(size_t i = 0; i < size; ++i) {
			s << "		[" <<  (i!=myid ? std::to_string(i) : "S") << "]: " << counter(counter_t::GLOB_REF_WRITE, i) << "\n";
		}
		s << "Onesided Operations:\n";
		s << "	Put:\n";
		for(size_t i = 0; i < size; ++i) {
			s << "		[" <<  (i!=myid ? std::to_string(i) : "S") << "]: " << counter(counter_t::ONESIDEDED_PUT_BYTE, i) << " Byte in " << counter(counter_t::ONESIDEDED_PUT_NUM, i) << " operations" << "\n";
		}
		s << "	Get:\n";
		for(size_t i = 0; i < size; ++i) {
			s << "		[" <<  (i!=myid ? std::to_string(i) : "S") << "]: " << counter(counter_t::ONESIDEDED_GET_BYTE, i) << " Byte in " << counter(counter_t::ONESIDEDED_GET_NUM, i) << " operations" << "\n";
		}
		std::cout << s.str() << std::endl;
		reset();
	}
private:

	const size_t myid;
	const size_t size;

	ProfilerImpl() :
		myid(dash::myid()),
		size(dash::size())
	{
		for(auto& v : _counter) {
			v.resize(size);
		}
	}

	ProfilerImpl(const ProfilerImpl& cpy) = delete;
	ProfilerImpl& operator=(const ProfilerImpl& cpy) = delete;

	ProfilerImpl(ProfilerImpl&& mov) = delete;
	ProfilerImpl& operator=(ProfilerImpl&& mov) = delete;

	std::array<std::vector<size_t>, static_cast<size_t>(counter_t::SIZE)> _counter;

	void reset() {
		for(auto& v : _counter) {
			std::fill(v.begin(), v.end(), 0);
		}
	}

	size_t& counter(counter_t c, size_t id) {
		return _counter[static_cast<size_t>(c)][id];
	}

};

using Profiler = ProfilerImpl<dash::profiling_enabled::value>;

}
