#pragma once

#include "CSBuffer.h"

namespace dash {


struct ContainerCounter {
	size_t glob_ref_read = 0;
	size_t glob_ref_write = 0;
	size_t glob_ptr_leaked = 0;
	size_t glob_ptr_deref = 0;

	size_t onesided_put_num = 0;
	size_t onesided_put_byte = 0;
	size_t onesided_get_num = 0;
	size_t onesided_get_byte = 0;

	ContainerCounter& operator+=(const ContainerCounter& rhs) {
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

struct ContainerDescription {
	std::string name;
	std::string type;
	size_t elements;
	decltype(dart_gptr_t::teamid) teamid;
	std::vector<ContainerCounter> counter;

	ContainerDescription() :
		counter(dash::size())
	{
	}
};

inline ContigiousStreamBuffer& operator<<(ContigiousStreamBuffer& lhs, const ContainerDescription& rhs) {
	lhs << rhs.name;
	lhs << rhs.type;
	lhs << rhs.elements;
	lhs << rhs.teamid;
	lhs << rhs.counter;
	return lhs;
}

inline ContigiousStreamBuffer& operator<<(ContigiousStreamBuffer& lhs, const ContainerCounter& rhs) {
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

inline ContigiousStreamBuffer& operator>>(ContigiousStreamBuffer& lhs, ContainerCounter& rhs) {
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

inline ContigiousStreamBuffer& operator>>(ContigiousStreamBuffer& lhs, ContainerDescription& rhs) {
	lhs >> rhs.name;
	lhs >> rhs.type;
	lhs >> rhs.elements;
	lhs >> rhs.teamid;
	lhs >> rhs.counter;
	return lhs;
}

}
