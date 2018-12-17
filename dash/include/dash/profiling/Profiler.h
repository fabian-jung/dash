#pragma once

#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <iterator>
#include <algorithm>
#include <typeinfo>
#include <fstream>
#include <dash/Init.h>
#include <dash/profiling/traits.h>
#include <dash/Types.h>
#include <dash/Team.h>
// #include <dash/GlobRef.h>

#include <dash/profiling/CSBuffer.h>

namespace dash {

template<typename T>
class GlobRefImpl;

template<typename T, class MemSpaceT>
class GlobPtrImpl;

template <bool enabled>
class ProfilerImpl;

template<>
class ProfilerImpl<false> {
public:
	static ProfilerImpl get() {
		return ProfilerImpl();
	}

	void trackOnesidedPut(const dart_gptr_t& gptr, size_t id) {}
	void trackOnesidedGet(const dart_gptr_t& gptr, size_t id) {}
	template <class T> void trackRead(const GlobRefImpl<T>& ref) {}
	template <class T> void trackWrite(const GlobRefImpl<T>& ref) {}
	template <class T, class MemSpaceT> void trackDeref(const GlobPtrImpl<T, MemSpaceT>& ptr) {}
	template <class T> void trackDeref(const T* ptr) {}
	void report(std::string stage) {}
	void reset() {}

	template <class container_t>
	void container_set_name(const container_t& container, const char* name) {}

	template <class container_t>
	void container_register(const container_t& container) {}

	template <class container_t>
	void container_unregister(const container_t& container) {}

	template <class container_t>
	void container_move(const container_t& old_position, const container_t& new_position) {}

	ProfilerImpl(const ProfilerImpl& cpy) = default;
	ProfilerImpl& operator=(const ProfilerImpl& cpy) = default;

	ProfilerImpl(ProfilerImpl&& mov) = default;
	ProfilerImpl& operator=(ProfilerImpl&& mov) = default;

private:
	ProfilerImpl() = default;
};

namespace detail {

	template <class key_t, class value_t>
	class container_map_t {
	public:

		template <class key_it_t>
		value_t& insert(key_it_t begin, key_it_t end, const void* inner_key) {
			for_each(begin, end, [&](const auto& key){
				gptr_map[key] = inner_key;
			});
			return container[inner_key];
		}

		template <class key_it_t>
		void erase(key_it_t begin, key_it_t end) {
			std::for_each(begin, end, [&](const auto& key){
				gptr_map.erase(key);
			});
		}

		value_t& operator[](const key_t& key) {
			const void* inner_key = gptr_map[key];
			return container[inner_key];
		}

		const value_t& at(const key_t& key) const {
			const void* inner_key = gptr_map.at(key);
			return container.at(inner_key);
		}

		std::map<const void*, value_t>& get_inner() {
			return container;
		}

		void move(const void* old_position, const void* new_position) {
			for(auto& kv : gptr_map) {
				if(kv.second == old_position) {
					kv.second = new_position;
				}
			}
			auto c = std::move(container[old_position]);
			container.erase[old_position];
			container[new_position] = c;
		}
	private:
		std::map<const void*, value_t> container;
		std::map<key_t, const void*> gptr_map;
	};
}

template <>
class ProfilerImpl<true> {
public:

	static ProfilerImpl& get() {
		static ProfilerImpl self;
		return self;
	}

	~ProfilerImpl() {
// 		report("Final");
	}

	void trackOnesidedPut(const dart_gptr_t& gptr, size_t bytes) {
		if(filter.onesided_communication) {
			fetch_counter(gptr, &container_counter_t::onesided_put_byte) += bytes;
			++fetch_counter(gptr, &container_counter_t::onesided_put_num);
		}
	}

	void trackOnesidedGet(const dart_gptr_t& gptr, size_t bytes) {
		if(filter.onesided_communication) {
			fetch_counter(gptr, &container_counter_t::onesided_get_byte) += bytes;
			++fetch_counter(gptr, &container_counter_t::onesided_get_num);
		}
	}

	template<class T>
	void trackRead(const GlobRefImpl<T>& ref) {
		if(filter.global_reference) {
			++fetch_counter(ref.dart_gptr(), &container_counter_t::glob_ref_read);
		}
	}

	template<class T>
	void trackWrite(const GlobRefImpl<T>& ref) {
		if(filter.global_reference) {
			++fetch_counter(ref.dart_gptr(), &container_counter_t::glob_ref_write);
		}
	}

	template <class T, class MemSpaceT>
	void trackDeref(const GlobPtrImpl<T, MemSpaceT>& ptr) {
		if(filter.global_pointer) {
			++fetch_counter(ptr.dart_gptr(), &container_counter_t::glob_ptr_deref);
		}
	}

	template <class T>
	void trackDeref(const T* ptr) {
		if(filter.local_pointer) {
			++local_ptr_deref;
		}
	}

	void report(std::string stage) {
		ContigiousStreamBuffer buffer;
		buffer << container_map.get_inner();
		auto buffer_size = buffer.size();
		std::vector<size_t> sizes;
		sizes.resize(size);
		dart_gather(
			&buffer_size,// sendbuf,
			sizes.data(), //recvbuf
			1, //nelem
			dart_datatype<decltype(buffer_size)>::value, // sendtype,
			0, //root
			dash::Team::All().dart_id() //team
		);

		auto total_size = std::accumulate(sizes.begin(), sizes.end(), 0);
		ContigiousStreamBuffer recvBuffer;
		if(myid==0) {
			recvBuffer.resize(total_size);
		}
		dart_gather(
			buffer.data(),// sendbuf,
			recvBuffer.data(), //recvbuf
			buffer.size(), //nelem
			dart_datatype<char>::value, // sendtype,
			0, //root
			dash::Team::All().dart_id() //team
		);

		if(myid == 0) {
			for(auto unit_id = 0; unit_id < size; ++unit_id) {
				std::remove_reference<decltype(container_map.get_inner())>::type map;
				recvBuffer >> map;
				std::stringstream s;
				s << "Profiler overview for stage [" << stage << "] and unit " << unit_id << ":\n";
				if(filter.local_pointer) {
					s << "	Local Pointer Operations:\n";
					s << "		Dereference Operations: " << local_ptr_deref << "\n";
				}
				for(const auto& kv : map) {
					const auto& container = kv.second;
					if(filter.container_based_logging) {
						s << "Report for container \"" << container.name << "\":\n";
						s << "	type: " << container.type << "\n";
						s << "	elements: " << container.elements << "\n";
					}
					if(filter.container_based_logging || !filter.team_based_logging) {
						print(s, container.counter, unit_id);
					}
				}
				if(filter.team_based_logging) {
					std::map<decltype(dart_gptr_t::teamid), std::vector<container_counter_t>> team_counter;
					for(const auto& inner_kv : map) {
						auto team_id = inner_kv.second.teamid;
						if(team_counter.count(team_id) == 0) {
							team_counter.emplace(team_id, std::vector<container_counter_t>(size));
						}
						auto& tcv = team_counter.at(team_id);
						const auto& cv = inner_kv.second.counter;
						for(auto i = 0; i < size; ++i) {
							tcv[i] += cv[i];
						}
					}
					for(auto& team_kv : team_counter) {
						s << "Report for team " << team_kv.first << "\n";
						print(s, team_kv.second, unit_id);
					}
				}
				std::cout << s.str() << std::endl;
			}
		}
		reset();
	}



	ProfilerImpl(const ProfilerImpl& cpy) = delete;
	ProfilerImpl& operator=(const ProfilerImpl& cpy) = delete;

	ProfilerImpl(ProfilerImpl&& mov) = delete;
	ProfilerImpl& operator=(ProfilerImpl&& mov) = delete;

	template <class container_t>
	void container_set_name(const container_t& container, const char* name) {
		if(filter.container_based_logging) {
			const auto dart_gptr = container.begin().dart_gptr();
			const auto id = std::make_tuple(dart_gptr.segid, dart_gptr.teamid, dart_gptr.unitid);
			container_map[id].name = name;
		}

	}

	template <class container_t>
	void container_register(const container_t& container) {
		if(filter.container_based_logging || filter.team_based_logging) {
			std::set<gptr_identififaction_t> gptrs;
			std::transform(
				container.begin(),
				container.end(),
				std::inserter(gptrs, gptrs.begin()),
				[](auto glob_ref){
					const auto dart_gptr = glob_ref.dart_gptr();
					return std::make_tuple(dart_gptr.segid, dart_gptr.teamid, dart_gptr.unitid);
				}
			);

			auto& entry = container_map.insert(gptrs.begin(), gptrs.end(), &container);
			entry.type = typeid(container_t).name();
			entry.elements = container.size();
			entry.teamid = container.team().dart_id();
		}
	}

	template <class container_t>
	void container_unregister(const container_t& container) {
// 		if(filter.container_based_logging || filter.team_based_logging) {
// 			std::set<gptr_identififaction_t> gptrs;
// 			std::transform(
// 				container.begin(),
// 				container.end(),
// 				std::inserter(gptrs, gptrs.begin()),
// 				[](auto glob_ref){
// 					const auto dart_gptr = glob_ref.dart_gptr();
// 					return std::make_tuple(dart_gptr.segid, dart_gptr.teamid, dart_gptr.unitid);
// 				}
// 			);
// 			container_map.erase(gptrs.begin(), gptrs.end());
// 		}
	}

	template <class container_t>
	void container_move(const container_t& old_position, const container_t& new_position) {
		if(filter.container_based_logging || filter.team_based_logging) {
			container_map.move(&old_position, &new_position);
		}
	}


	struct filter_t {
		bool container_based_logging = true;
		bool team_based_logging = true;
		bool onesided_communication = true;
		bool global_reference = true;
		bool global_pointer = true;
		bool local_pointer = true;

		filter_t() {
			std::ifstream file("filter.txt");
			if(file.is_open()) {
				std::string line;
				std::map<std::string, bool filter_t::*> attributes {
					{"container_based_logging", &filter_t::container_based_logging},
					{"team_based_logging", &filter_t::team_based_logging},
					{"onesided_communication", &filter_t::onesided_communication},
					{"global_reference", &filter_t::global_reference},
					{"global_pointer", &filter_t::global_pointer},
					{"local_pointer", &filter_t::local_pointer}
				};
				while(std::getline(file, line)) {
					line.erase(std::remove(line.begin(), line.end(), ' '), line.end());;
// 					std::cout << line << std::endl;
					if(line[0] == '#') {
						//Line is a comment
						continue;
					};
					for(const auto& attr : attributes) {
						if(line.substr(0, attr.first.size()) == attr.first) {
							bool enable =  std::stoi(line.substr(attr.first.size()+1,1));
// 							std::cout << attr.first << " = " << enable << std::endl;
							this->*attr.second = enable;
						}
					}
				}
			} else {
				if(dash::myid() == 0) {
					std::ofstream default_file("filter.txt");
					default_file << "# This file sets up the runtime filter for the profiler inside dash\n";
					default_file << "container_based_logging =  1;\n";
					default_file << "team_based_logging =  1;\n";
					default_file << "onesided_communication = 1;\n";
					default_file << "global_reference = 1;\n";
					default_file << "global_pointer = 1;\n";
					default_file << "local_pointer = 1;\n";
				}
			}
		}
	};

	void reset() {
		for(auto& cv : container_map.get_inner()) {
			std::fill(
				cv.second.counter.begin(),
				cv.second.counter.end(),
				container_counter_t()
			);
		}
	}

private:

	const size_t myid;
	const size_t size;
	const filter_t filter;

	ProfilerImpl() :
		myid(dash::myid()),
		size(dash::size())
	{
		container_map.get_inner()[nullptr].name = "<Unknown container>";
	}

	using gptr_identififaction_t =
		std::tuple<
			decltype(dart_gptr_t::segid),
			decltype(dart_gptr_t::teamid),
			decltype(dart_gptr_t::unitid)
		>;

	struct container_counter_t {
		size_t glob_ref_read = 0;
		size_t glob_ref_write = 0;
		size_t glob_ptr_leaked = 0;
		size_t glob_ptr_deref = 0;

		size_t onesided_put_num = 0;
		size_t onesided_put_byte = 0;
		size_t onesided_get_num = 0;
		size_t onesided_get_byte = 0;

		container_counter_t& operator+=(const container_counter_t& rhs) {
			glob_ref_read       += rhs.glob_ref_read;
			glob_ref_write      += rhs.glob_ref_write;
			glob_ptr_leaked     += rhs.glob_ptr_leaked;
			glob_ptr_deref      += rhs.glob_ptr_deref;

			onesided_put_num    += rhs.onesided_put_num;
			onesided_put_byte   += rhs.onesided_put_byte;
			onesided_get_num    += rhs.onesided_get_num;
			onesided_get_byte   += rhs.onesided_get_byte;

			return *this;
		}
	};
	friend ContigiousStreamBuffer& operator<<(ContigiousStreamBuffer& lhs, const container_counter_t& rhs);
	friend ContigiousStreamBuffer& operator>>(ContigiousStreamBuffer& lhs, container_counter_t& rhs);



	struct container_t {
		std::string name;
		std::string type;
		size_t elements;
		decltype(dart_gptr_t::teamid) teamid;
		std::vector<container_counter_t> counter;

		container_t() :
			counter(dash::size())
		{
		}
	};
	friend ContigiousStreamBuffer& operator<<(ContigiousStreamBuffer& lhs, const container_t& rhs);
	friend ContigiousStreamBuffer& operator>>(ContigiousStreamBuffer& lhs, container_t& rhs);

	size_t& fetch_counter(const dart_gptr_t& gptr, size_t container_counter_t::* member) {
		return container_map[std::make_tuple(gptr.segid, gptr.teamid, gptr.unitid)].counter[gptr.unitid].*member;
	}

	const size_t& fetch_counter(const dart_gptr_t& gptr, size_t container_counter_t::* member) const {
		return container_map.at(std::make_tuple(gptr.segid, gptr.teamid, gptr.unitid)).counter[gptr.unitid].*member;
	}

	detail::container_map_t<gptr_identififaction_t, container_t> container_map;
	size_t local_ptr_deref;

	void print(std::ostream& stream, const std::vector<container_counter_t>& counter, int unit_id) {
		if(filter.global_pointer) {
			stream <<"	GlobPtr Operations:\n";
			stream <<"		Dereference Operations:\n";
			for(size_t i = 0; i < size; ++i) {
				stream <<"		[" << (i!=unit_id ? std::to_string(i) : "S")  << "]: " << counter[i].glob_ptr_deref << "\n";
			}
		}
		if(filter.global_reference) {
			stream <<"	GlobRef Operations:\n";
			stream <<"		R-Value (Reads):\n";
			for(size_t i = 0; i < size; ++i) {
				stream <<"			[" << (i!=unit_id ? std::to_string(i) : "S")  << "]: " << counter[i].glob_ref_read << "\n";
			}
			stream <<"		L-Value (Writes):\n";
			for(size_t i = 0; i < size; ++i) {
				stream <<"			[" <<  (i!=unit_id ? std::to_string(i) : "S") << "]: " << counter[i].glob_ref_write << "\n";
			}
		}
		if(filter.onesided_communication) {
			stream <<"	Onesided Operations:\n";
			stream <<"		Get:\n";
			for(size_t i = 0; i < size; ++i) {
				stream <<"			[" <<  (i!=unit_id ? std::to_string(i) : "S") << "]: " << counter[i].onesided_get_byte << " Byte in " << counter[i].onesided_get_num << " operations" << "\n";
			}
			stream <<"		Put:\n";
			for(size_t i = 0; i < size; ++i) {
				stream <<"			[" <<  (i!=unit_id ? std::to_string(i) : "S") << "]: " << counter[i].onesided_put_byte << " Byte in " << counter[i].onesided_put_num << " operations" << "\n";
			}
		}
	}

};

inline ContigiousStreamBuffer& operator<<(ContigiousStreamBuffer& lhs, const ProfilerImpl<true>::container_t& rhs) {
	lhs << rhs.name;
	lhs << rhs.type;
	lhs << rhs.elements;
	lhs << rhs.teamid;
	lhs << rhs.counter;
	return lhs;
}

inline ContigiousStreamBuffer& operator<<(ContigiousStreamBuffer& lhs, const ProfilerImpl<true>::container_counter_t& rhs) {
	lhs << rhs.glob_ref_read;
	lhs << rhs.glob_ref_write;
	lhs << rhs.glob_ptr_leaked;
	lhs << rhs.glob_ptr_deref;

	lhs << rhs.onesided_put_num;
	lhs << rhs.onesided_put_byte;
	lhs << rhs.onesided_get_num;
	lhs << rhs.onesided_get_byte;

	return lhs;
}

inline ContigiousStreamBuffer& operator>>(ContigiousStreamBuffer& lhs, ProfilerImpl<true>::container_counter_t& rhs) {
	lhs >> rhs.glob_ref_read;
	lhs >> rhs.glob_ref_write;
	lhs >> rhs.glob_ptr_leaked;
	lhs >> rhs.glob_ptr_deref;

	lhs >> rhs.onesided_put_num;
	lhs >> rhs.onesided_put_byte;
	lhs >> rhs.onesided_get_num;
	lhs >> rhs.onesided_get_byte;

	return lhs;
}

inline ContigiousStreamBuffer& operator>>(ContigiousStreamBuffer& lhs, ProfilerImpl<true>::container_t& rhs) {
	lhs >> rhs.name;
	lhs >> rhs.type;
	lhs >> rhs.elements;
	lhs >> rhs.teamid;
	lhs >> rhs.counter;
	return lhs;
}



using Profiler = ProfilerImpl<dash::profiling_enabled::value>;

}
