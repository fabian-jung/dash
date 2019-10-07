#pragma once

#include <iostream>
#include <array>
#include <map>
#include <cstring>
#include <vector>
#include <dash/Init.h>

namespace dash {
	enum class papi_counter_t : size_t {
		glob_ref_read = 0,
		glob_ref_write = 1,
// 		glob_ptr_leaked = 2;
		glob_ptr_deref = 2,
		onesided_put_num = 3,
// 		onesided_put_byte = 0;
		onesided_get_num = 4,
// 		onesided_get_byte = 0;
		local_ptr_deref = 5,

		size = 6
	};
}


#ifdef DASH_ENABLE_PAPI
extern "C" {
#include "papi.h"
}
namespace dash {

class PapiBackendWrapper {
public:
	PapiBackendWrapper() :
		values(_papi_dash_values())
	{}

	long long& operator[](papi_counter_t counter) {
		return values[static_cast<size_t>(counter)];
	}

	const long long& operator[](papi_counter_t counter) const {
		return values[static_cast<size_t>(counter)];
	}
private:
	long long* values;
};

class PapiEventsetWrapper {
public:
	PapiEventsetWrapper()
	{
		std::cout << "Initialise profiler with papi_enabled = " << true << std::endl;
		std::map<std::string, size_t> papi_map {
			{"dash:::DASH_GLOB_REF_LVALUE_ACCESS", static_cast<size_t>(papi_counter_t::glob_ref_read)},
			{"dash:::DASH_GLOB_REF_RVALUE_ACCESS", static_cast<size_t>(papi_counter_t::glob_ref_write)},
			{"dash:::DASH_GLOB_PTR_ACCESS",        static_cast<size_t>(papi_counter_t::glob_ptr_deref)},
			{"dash:::DASH_ONESIDED_PUT",           static_cast<size_t>(papi_counter_t::onesided_put_num)},
			{"dash:::DASH_ONESIDED_GET",           static_cast<size_t>(papi_counter_t::onesided_get_num)},
			{"dash:::DASH_LOCAL_PTR_ACCESS",       static_cast<size_t>(papi_counter_t::local_ptr_deref)}
		};
		if(PAPI_library_init(PAPI_VER_CURRENT) != PAPI_VER_CURRENT ) {
			throw std::runtime_error("Could not initialise PAPI with current version. This is probably caused by a broken installation.");
		}
		int dash_cix = papi_find_component("dash");
		if(dash_cix == -1) {
			std::cerr << "Could not initialise the dash component in the PAPI library." << std::endl;
		}
		const PAPI_component_info_t* dash_info = PAPI_get_component_info(dash_cix);
		std::array<size_t, static_cast<size_t>(papi_counter_t::size)> papi_event_codes;

		int EventCode = PAPI_NATIVE_MASK;
		if(PAPI_enum_cmp_event(&EventCode, PAPI_ENUM_FIRST, dash_cix) != PAPI_OK) {
			throw std::runtime_error("Could not get EventCodes for dash component in PAPI.");
		}

		do {
			std::string name;
			name.resize(PAPI_MAX_STR_LEN);
			if(PAPI_event_code_to_name(EventCode, &name[0]) != PAPI_OK) {
				throw std::runtime_error("Could not resolve PAPI counter name");
			}
			name.resize(std::strlen(name.c_str()));
			std::cout << "Resolved event name " << name << " for code " << EventCode << std::endl;
			const auto pos = papi_map.at(name);
			papi_event_codes[pos] = EventCode;
		} while(PAPI_enum_cmp_event(&EventCode, PAPI_ENUM_EVENTS, dash_cix) == PAPI_OK);
		if ( PAPI_create_eventset(&eventset) != PAPI_OK) {
			throw std::runtime_error("Could not create a PAPI eventset.");
		}
		// Assign dash as component to the eventset
		if ( PAPI_assign_eventset_component(eventset, dash_cix) != PAPI_OK) {
			throw std::runtime_error("Could not assign dash as PAPI eventset component.");
		}

		for(const auto c : papi_event_codes) {
			std::string name;
			name.resize(PAPI_MAX_STR_LEN);
			PAPI_event_code_to_name(c, &name[0]);
			std::cout << "Add event " << name << "[" << c << "] to eventset." << std::endl;
			if (PAPI_add_event(eventset,c) != PAPI_OK) {
				std::runtime_error("Could not add papi counter to eventset");
			}
		}

		auto err = PAPI_start(eventset);
		if ( err != PAPI_OK ) {
			throw std::runtime_error("Papi could not start recording eventset. " + std::to_string(err));
		}

		reset();
	}

	~PapiEventsetWrapper() {
		std::array<long long, static_cast<size_t>(papi_counter_t::size)> values;
		if ( PAPI_stop(eventset, values.data()) != PAPI_OK) {
			std::cerr << "Papi could not stop recording eventset." << std::endl;
		}

		std::cout << "PAPI values:";
		for(auto c : values ) {
			std::cout << c << " ";
		}
		std::cout << std::endl;

		PAPI_cleanup_eventset( eventset );
		PAPI_destroy_eventset( &eventset );

		PAPI_shutdown();

	}
	void reset() {
		std::vector<long long> values(static_cast<size_t>(papi_counter_t::size));
		std::fill(values.begin(), values.end(), 0);
		if(PAPI_write(eventset, values.data()) != PAPI_OK) {
			throw std::runtime_error("Could not write values to eventset");
		}
	}

	void increment(papi_counter_t counter) {
		std::array<long long, static_cast<size_t>(papi_counter_t::size)> values;
		if(PAPI_read(eventset, values.data()) != PAPI_OK) {
			throw std::runtime_error("Could not read values from eventset");
		}
		++(values[static_cast<size_t>(counter)]);
		std::cout << "PAPI(" << myid() << "): inc value(" << static_cast<size_t>(counter) << ") to " << values[static_cast<size_t>(counter)] << std::endl;
		if(PAPI_write(eventset, values.data()) != PAPI_OK) {
			throw std::runtime_error("Could not write values to eventset");
		}
	}

	long long read(papi_counter_t counter) {
		std::array<long long, static_cast<size_t>(papi_counter_t::size)> values;
		if(PAPI_read(eventset, values.data()) != PAPI_OK) {
			throw std::runtime_error("Could not read values from eventset");
		}
		return values[static_cast<size_t>(counter)];
	};


private:
	int papi_find_component(const char* name) {
		int numCmps = PAPI_num_components();
		std::cout << "Number of PAPI components:" << numCmps << std::endl;
		for(int i = 0; i < numCmps; ++i) {
			const PAPI_component_info_t* info = PAPI_get_component_info(i);
			std::cout << info->name << std::endl;
			if(strcmp(info->name, name) == 0) {
				std::cout << "Component " << info->name << " found id = " << i << std::endl;
				return i;
			} else {
	// 			printf("Component %s\n", info->name);
			}
		}
		std::cout << "Component %s could not be found\n" << name << std::endl;
		return -1;
	}

	int eventset = PAPI_NULL;
};

}
#else

namespace dash {

class PapiEventsetWrapper {
public:
	PapiEventsetWrapper() = default;

	void reset() {}
	long long read(papi_counter_t counter) { return 0 };
	void increment(papi_counter_t counter) {}
};

}

#endif
