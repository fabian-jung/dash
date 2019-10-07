#pragma once

#include <type_traits>
#include <dash/meta/ConditionalType.h>
#include <dash/profiling/Profiler.h>
#include <dash/profiling/traits.h>

namespace dash {

template <class type>
class TracedPointer {
public:
	using pointer = type;
	using const_pointer = const pointer;
	using reference = decltype(*(std::declval<pointer>()));
	using difference_type = decltype(std::declval<pointer>()-std::declval<pointer>());

	TracedPointer() noexcept:
		ptr(nullptr)
	{
	}

	TracedPointer(std::nullptr_t ptr) noexcept:
		ptr(ptr)
	{
	}

	template <
		typename From
		/*

		typename std::enable_if<
			// We always allow GlobPtrImpl<T> -> GlobPtrImpl<void> or the other way)
			// or if From is assignable to To (value_type)
			dash::internal::is_pointer_assignable<
				typename dash::remove_atomic<From>::type,
				typename dash::remove_atomic<pointer>::type
			>::value
		>::type*/
	>
	TracedPointer(const TracedPointer<From>& conv) noexcept :
		ptr(conv.ptr)
	{
	}

	TracedPointer& operator=(const TracedPointer<typename std::remove_const<pointer>::type>& cpy) noexcept {
		ptr = cpy.ptr;
		return *this;
	}

	TracedPointer(const TracedPointer& copy) noexcept :
		ptr(copy.ptr)
	{
	}

	TracedPointer(TracedPointer&& move) noexcept :
		ptr(std::move(move.ptr))
	{
	}

	TracedPointer& operator=(TracedPointer&& move) noexcept {
		ptr = std::move(move.ptr);
		return *this;
	}

	TracedPointer(pointer conv) noexcept :
		ptr(conv)
	{
// 		std::cout << "TracedPointer" << std::endl;
	}

	TracedPointer& operator=(pointer conv) noexcept {
		ptr = conv;
		return *this;
	}

	TracedPointer& operator=(const pointer& conv) noexcept {
		ptr = conv;
		return *this;
	}

	TracedPointer& operator=(pointer&& conv) noexcept {
		ptr = std::move(conv); // Might or might not be useful for GlobPtr
		return *this;
	}

	template <class numerical_t>
	TracedPointer operator+(numerical_t rhs) const noexcept {
		return TracedPointer(ptr + rhs);
	}

	template <class numerical_t>
	TracedPointer operator-(numerical_t rhs) const noexcept {
		return TracedPointer(ptr - rhs);
	}

	TracedPointer& operator++() { // prefix
		++ptr;
		return *this;
	}
	TracedPointer operator++(int) { // postfix
		auto tmp = *this;
		ptr++;
		return tmp;
	}

	TracedPointer& operator--() { // prefix
		--ptr;
		return *this;
	}

	TracedPointer operator--(int) { // postfix
		auto tmp = *this;
		ptr--;
		return tmp;
	}

	bool operator==(const TracedPointer& rhs) const{
		return ptr == rhs.ptr;
	}

	bool operator==(const_pointer rhs) const {
		return ptr == rhs;
	}

	bool operator!=(const TracedPointer& rhs) const {
		return ptr != rhs.ptr;
	}

	bool operator!=(const_pointer rhs) const {
		return ptr != rhs;
	}

	bool operator>(const TracedPointer& rhs) const {
		return ptr > rhs.ptr;
	}

	bool operator>(const_pointer rhs) const {
		return ptr > rhs;
	}

	bool operator>=(const TracedPointer& rhs) const {
		return ptr >= rhs.ptr;
	}
	bool operator>=(const_pointer rhs) const {
		return ptr >= rhs;
	}

	bool operator<(const TracedPointer& rhs) const  {
		return ptr < rhs.ptr;
	}

	bool operator<(const_pointer rhs) const {
		return ptr < rhs;
	}

	bool operator<=(const TracedPointer& rhs) const {
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
	TracedPointer& operator+=(numerical_t rhs) {
		ptr += rhs;
		return *this;
	}

	template <class numerical_t>
	TracedPointer& operator-=(numerical_t rhs) {
		ptr -= rhs;
		return *this;
	}

	template <class numerical_t>
	reference operator[](numerical_t rhs) {//Subscript
// 		std::cout << "subscript" << std::endl;
		Profiler::get().trackDeref(ptr);
		return ptr[rhs];
	}

	reference operator*() { // Indirection
// 		std::cout << "indirection" << std::endl;
		Profiler::get().trackDeref(ptr);
		return *ptr;
	}

	auto operator&() { // Address-of
		return &ptr;
	}

	pointer operator->() {
		Profiler::get().trackDeref(ptr);
		return ptr;
	}

	template <class member_pointer_t>
	reference operator->*(member_pointer_t member_pointer) {
		std::cout << "member_pointer" << std::endl;
		Profiler::get().trackDeref(ptr);
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

	template <class fptr_t, class... Args>
	auto traced_call(fptr_t f, Args&&... args) {
		return (ptr.*f)(std::forward<Args>(args)...);
	}

	template <class fptr_t, class... Args>
	auto traced_call(fptr_t f, Args&&... args) const {
		return (ptr.*f)(std::forward<Args>(args)...);
	}

private:
	pointer ptr;
	const Profiler& profiler = Profiler::get();

	template <class T2>
	friend class TracedPointer;
};

}

namespace std {

template <class T>
struct iterator_traits<dash::TracedPointer<T>> {
	using difference_type = typename dash::TracedPointer<T>::difference_type;
};

}
