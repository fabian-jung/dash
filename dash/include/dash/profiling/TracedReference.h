#pragma once

#include <type_traits>

namespace dash {

template<typename T>
class GlobRefImpl;

namespace detail {

template <class ref>
struct get_ref_value_type;

template <class T>
struct get_ref_value_type<GlobRefImpl<T>> {
	using type = typename std::remove_const<T>::type;
};

template <class T>
struct get_ref_value_type<T&> {
	using type = typename std::remove_const<T>::type;
};

} // End of namespace detail

template <class Reference>
class TracedReference {
public:

	using self_t = TracedReference;

	using reference = Reference;
	using value_type = typename detail::get_ref_value_type<reference>::type;

	TracedReference() = delete;

	TracedReference(const TracedReference& cpy) = default;
	TracedReference& operator=(const TracedReference& cpy) = default;

	TracedReference(TracedReference&& mov) = default;
	TracedReference& operator=(TracedReference&& move) = default;

	TracedReference(reference ref) :
		ref(ref)
	{}

	template<class conv_t>
	explicit TracedReference(conv_t conv) :
		ref(conv)
	{}

	operator value_type() const {
		std::cout << "converted GlobRef" << std::endl;
		return static_cast<value_type>(ref);
	}

	bool operator==(const value_type &value) const {
		return static_cast<value_type>(*this) == value;
	}

	bool operator!= (const value_type &value) const {
		return !(*this == value);
	}

	bool operator== (const self_t &other) const noexcept {
		return (*this == static_cast<value_type>(other));
	}

	bool operator!= (const self_t &other) const noexcept {
		return !(*this == other);
	}

	TracedReference& operator= (const value_type val) {
		std::cout << "Traced Ref Assignment" << std::endl;
		ref.operator=(val);
		return *this;
	}

	TracedReference& operator+= (const value_type &ref) {
		return this->ref += ref;
	}

	TracedReference& operator-= (const value_type &ref)  {
		return this->ref -= ref;
	}

	TracedReference& operator++ ()  {
		return ++ref;
	}

	TracedReference	operator++ (int) {
		return ref++;
	}

	TracedReference& operator-- () {
		return --ref;
	}

	TracedReference	operator-- (int) {
		return ref--;
	}

	TracedReference& operator*= (const value_type &ref)  {
		return this->ref *= ref;
	}

	TracedReference& operator/= (const value_type &ref)  {
		return this->ref /= ref;
	}

	TracedReference& operator^= (const value_type &ref)  {
		return this->ref ^= ref;
	}

	template <class fptr_t, class... Args>
	auto traced_call(fptr_t f, Args&&... args) {
		std::cout << "Traced Ref Call" << std::endl;
		return (ref.*f)(std::forward<Args>(args)...);
	}

	template <class fptr_t, class... Args>
	auto traced_call(fptr_t f, Args&&... args) const {
		std::cout << "Traced Ref Call" << std::endl;
		return (ref.*f)(std::forward<Args>(args)...);
	}

private:
	reference ref;
}; // End of class TracedReference

} // End of namespace dash
