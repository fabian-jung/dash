#pragma once

#include <type_traits>
#include <dash/meta/ConditionalType.h>

namespace dash {

struct profiling_enabled {
	constexpr static bool value = true;
};

template <class type>
class TracedPointerImpl {
public:
	using pointer = typename std::remove_cv<type>::type;
	using const_pointer = const pointer;
	using reference = decltype(*(std::declval<pointer>()));

	TracedPointerImpl() :
		ptr(nullptr)
	{
		std::cout << "TracedPointerImpl" << std::endl;
	}

	TracedPointerImpl(std::nullptr_t ptr) :
		ptr(ptr)
	{
		std::cout << "TracedPointerImpl" << std::endl;
	}

	TracedPointerImpl(const TracedPointerImpl& cpy) :
		ptr(cpy.ptr)
	{
		std::cout << "TracedPointerImpl" << std::endl;
	}

	TracedPointerImpl& operator=(const TracedPointerImpl& cpy) {
		ptr = cpy.ptr;
		return *this;
	}

	TracedPointerImpl(TracedPointerImpl&& move) :
		ptr(std::move(move.ptr))
	{
		std::cout << "TracedPointerImpl" << std::endl;
	}

	TracedPointerImpl& operator=(TracedPointerImpl&& move) {
		ptr = std::move(move.ptr);
		return *this;
	}

	TracedPointerImpl(pointer conv) :
		ptr(conv)
	{
		std::cout << "TracedPointerImpl" << std::endl;
	}

	TracedPointerImpl& operator=(pointer conv) {
		ptr = conv;
		return *this;
	}

	TracedPointerImpl& operator=(pointer&& conv) {
		ptr = std::move(conv); // Might or might not be useful for GlobPtr
		return *this;
	}

	template <class numerical_t>
	TracedPointerImpl operator+(numerical_t rhs) const {
		return TracedPointerImpl(ptr + rhs);
	}

	template <class numerical_t>
	TracedPointerImpl operator-(numerical_t rhs) const {
		return TracedPointerImpl(ptr - rhs);
	}

	TracedPointerImpl& operator++() { // prefix
		++ptr;
		return *this;
	}
	TracedPointerImpl operator++(int) { // postfix
		auto tmp = *this;
		ptr++;
		return tmp;
	}

	TracedPointerImpl& operator--() { // prefix
		--ptr;
		return *this;
	}

	TracedPointerImpl operator--(int) { // postfix
		auto tmp = *this;
		ptr--;
		return tmp;
	}

	bool operator==(const TracedPointerImpl& rhs) const{
		return ptr == rhs.ptr;
	}

	bool operator==(const_pointer rhs) const {
		return ptr == rhs;
	}

	bool operator!=(const TracedPointerImpl& rhs) const {
		return ptr != rhs.ptr;
	}

	bool operator!=(const_pointer rhs) const {
		return ptr != rhs;
	}

	bool operator>(const TracedPointerImpl& rhs) const {
		return ptr > rhs.ptr;
	}

	bool operator>(const_pointer rhs) const {
		return ptr > rhs;
	}

	bool operator>=(const TracedPointerImpl& rhs) const {
		return ptr >= rhs.ptr;
	}
	bool operator>=(const_pointer rhs) const {
		return ptr >= rhs;
	}

	bool operator<(const TracedPointerImpl& rhs) const  {
		return ptr < rhs.ptr;
	}

	bool operator<(const_pointer rhs) const {
		return ptr < rhs;
	}

	bool operator<=(const TracedPointerImpl& rhs) const {
		return ptr <= rhs.ptr;
	}

	bool operator<=(const_pointer rhs) const {
		return ptr <= rhs;
	}

	bool operator!() const {
		return !ptr;
	}

	template <class T>
	bool operator&&(T rhs) const {
		return ptr && rhs;
	}

	template <class T>
	bool operator||(T rhs) const {
		return ptr || rhs;
	}

	template <class numerical_t>
	TracedPointerImpl& operator+=(numerical_t rhs) {
		ptr += rhs;
		return *this;
	}

	template <class numerical_t>
	TracedPointerImpl& operator-=(numerical_t rhs) {
		ptr -= rhs;
		return *this;
	}

	template <class numerical_t>
	reference operator[](numerical_t rhs) {//Subscript
		std::cout << "subscript" << std::endl;
		return ptr[rhs];
	}

	reference operator*() { // Indirection
		std::cout << "indirection" << std::endl;
		profiler.track();
		return *ptr;
	}

	auto operator&() { // Address-of
		return &ptr;
	}

	pointer operator->() {
		return ptr;
	}

	template <class member_pointer_t>
	reference operator->*(member_pointer_t member_pointer) {
		std::cout << "member_pointer" << std::endl;
		return ptr->*member_pointer;
	}

	operator pointer() const {
		std::cout << "convert to native" << std::endl;
		return ptr;
	}

	pointer native() const {
		std::cout << "convert to native" << std::endl;
		return ptr;
	}

// 	void* operator new(size_t);
// 	void* operator new[](size_t);
// 	void operator delete(void *);
// 	void operator delete[](void *);

private:
	pointer ptr;
};

template <class pointer_t>
using TracedPointer = typename conditional_type<profiling_enabled::value, TracedPointerImpl<pointer_t>, pointer_t>::type;

}
