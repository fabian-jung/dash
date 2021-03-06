#pragma once

#pragma once

#include <memory>
#include <algorithm>
#include <stdexcept>
#include <cmath>

#include <dash/Types.h>
#include <dash/dart/if/dart.h>
#include "allocator/SymmetricAllocator.h"

namespace dash {

/*
*  requirements:
*
* T:
* 	Default constructible
* 	Copy constructible
*/

template <class T, template<class> class allocator = dash::allocator::SymmetricAllocator>
class Vector;

template <class Vector>
class Vector_iterator;

} // End of namespace dash

namespace std {


	template <class Vector>
	struct iterator_traits<dash::Vector_iterator<Vector>> {
		using value_type = typename dash::Vector_iterator<Vector>::value_type;
		using pointer = typename Vector::pointer;
		using reference = typename Vector::reference;
		using difference_type = typename Vector::index_type;
		using iterator_category = std::random_access_iterator_tag;
	};

	// Ugly fix to be able to use std::copy on GlobPtr
// 	template <class T, class mem_type>
// 	struct iterator_traits<dash::GlobPtr<T, mem_type>> {
// 		using self_type = iterator_traits<dash::GlobPtr<T, mem_type>>;
// 		using value_type = T;
//  		using pointer = dash::GlobPtr<T, mem_type>;
// 		using difference_type = decltype(std::declval<pointer>() - std::declval<pointer>());
// // 		using reference = typename Vector::reference;
// 		using iterator_category = std::random_access_iterator_tag;
// 	};

} // End of namespace std

namespace dash {

template <class Vector>
struct Vector_iterator {

	using index_type = dash::default_index_t;
	using value_type = typename Vector::value_type;
	using size_type = typename Vector::size_type;
	using global_unit_type = decltype(dash::myid());

	struct position_index_t {
		global_unit_t position;
		index_type local_index;
	};

	Vector& _vec;
	position_index_t _index;
	size_type _local_size;

	position_index_t get_postion_index(index_type global_index) const {
		position_index_t index { static_cast<global_unit_type>(0), 0 };
// 		std::cout << "globabl index = " << global_index << std::endl;
		bool finished = false;
		for(auto unit = static_cast<global_unit_type >(0); unit < _vec._team.size(); unit++) {
// 			std::cout << "unit = " << unit << std::endl;
			auto local_size = dash::atomic::load(*(_vec._local_sizes.begin()+unit));
// 			std::cout << "local_size(" << unit << ")=" << local_size << " global_index(" << global_index <<")" <<std::endl;
			if(global_index >= local_size) {
				index.position = unit+1;
				global_index -= local_size;
			} else {
				finished = true;
				break;
			}
		}

		if(finished) {
			index.local_index = global_index;
		} else {
// 			std::cout << "not finished" << std::endl;
		}
// 		std::cout << "pos = " << index.position << " index = " << index.local_index << std::endl;

		return index;
	}

	index_type get_global_index(const position_index_t& position_index) const {
		index_type global_index = 0;
		for(index_type i = 0; i < position_index.position; ++i) {
			global_index += dash::atomic::load(*(_vec._local_sizes.begin()+i));
		}

		global_index += position_index.local_index;

		return global_index;
	}

	Vector_iterator(Vector& vec, index_type index) :
		_vec(vec),
		_index(get_postion_index(index)),
		_local_size(_index.position < _vec._team.size() ? dash::atomic::load(*(_vec._local_sizes.begin()+_index.position)) : 0)
	{
// 		std::cout << "Vector_iterator(" << index << ") " << _index.position << " " << _index.local_index << std::endl;
	}

	typename Vector::reference operator*() {
		auto lcapacity = _vec.lcapacity();
// 		std::cout << "op*" << std::endl;
		typename Vector::reference result = *(_vec._data.begin() + _index.position * lcapacity + _index.local_index);
// 		std::cout << "end op*" << std::endl;
		return result;
	}

	void checkIndex() {
		if(_index.local_index >= _local_size) {
// 			std::cout << "checkIndex Failed" << std::endl;
			++_index.position;
			_index.local_index = 0;
			if(_index.position < _vec._team.size()) {
				_local_size = dash::atomic::load(*(_vec._local_sizes.begin() + _index.position));
				checkIndex();
			}
		}
	}

	Vector_iterator& operator++() {
// 		std::cout << "op++" << std::endl;

		_index.local_index++;
		checkIndex();
// 		std::cout << "op++ pos = " << _index.position << " _index = " << _index.local_index << std::endl;
// 		std::cout << "end op++" << std::endl;

		return *this;
	}

	Vector& globmem() {
		return _vec;
	}

	index_type pos() const{
		return get_global_index(_index);
	};

	struct pattern_type {

		const Vector_iterator<Vector>& _iterator;

		using index_type = typename Vector::index_type;
		using size_type = typename Vector::size_type;
		using local_pointer = typename Vector::local_pointer;

		size_type local_size() {
			return _iterator._vec.lsize();
		}

		index_type lbegin() {
			return  _iterator._vec._team.myid() *  _iterator._vec.lcapacity();
		}

		index_type lend() {
			return lbegin() +  _iterator._vec.lsize();
		}

		index_type coords(index_type global_offset) const {
			return global_offset;
		}

		index_type at(index_type global_offset) {
			return global_offset - lbegin();
		}


	};

	pattern_type pattern() const {
		return pattern_type { *this };
	}

	struct has_view {
		static constexpr bool value = false;
	};
};


template <class Vector>
Vector_iterator<Vector> operator+(const Vector_iterator<Vector>& lhs, typename Vector_iterator<Vector>::index_type rhs) {
	const auto global_index = lhs.get_global_index(lhs._index);
	return Vector_iterator<Vector>(lhs._vec, global_index + rhs);
}

template <class Vector>
typename Vector_iterator<Vector>::index_type operator-(const Vector_iterator<Vector>& lhs, const Vector_iterator<Vector>& rhs) {
	const auto global_index_lhs = lhs.get_global_index(lhs._index);
	const auto global_index_rhs = rhs.get_global_index(rhs._index);

	return global_index_lhs - global_index_rhs;
}

// template <class Vector>
// Vector_iterator<Vector>& operator++(Vector_iterator<Vector>& lhs) {
// 	lhs._index++;
// 	return lhs;
// }


template <class Vector>
bool operator==(const Vector_iterator<Vector>& lhs, const Vector_iterator<Vector>& rhs) {
	return lhs._index.position == rhs._index.position && lhs._index.local_index == rhs._index.local_index;
}

template <class Vector>
bool operator!=(const Vector_iterator<Vector>& lhs, const Vector_iterator<Vector>& rhs) {
	return !(lhs == rhs);
}

enum struct vector_strategy_t {
	CACHE,
	HYBRID,
	WRITE_THROUGH
};

template <class T, template<class> class allocator>
class Vector {
public:

	using self_type = Vector<T, allocator>;
	using value_type = T;

	using allocator_type = allocator<value_type>;
	using size_type = dash::default_size_t;
	using index_type = dash::default_index_t;
	using difference_type = ptrdiff_t;
	using reference = dash::GlobRef<T>;
	using local_reference = T&;

	using const_reference = const T&;

	using glob_mem_type = dash::GlobStaticMem<T, allocator<T>>;
 	using shared_local_sizes_mem_type = dash::GlobStaticMem<dash::Atomic<size_type>, allocator<dash::Atomic<size_type>>>;

	friend Vector_iterator<self_type>;
	using iterator = Vector_iterator<self_type>;
// 	using const_iterator = typename glob_mem_type::pointer;

	using pointer = T*;
// 	using const_pointer = const_iterator;

	using local_pointer = typename glob_mem_type::local_pointer;
	using const_local_pointer = typename glob_mem_type::const_local_pointer;

	using team_type = dash::Team;

	Vector(
		size_type local_elements = 0,
		const_reference default_value = value_type(),
		const allocator_type& alloc = allocator_type(),
		team_type& team = dash::Team::All()
	);

	Vector(const Vector& other) = delete;
	Vector(Vector&& other) = default;
// 	Vector( std::initializer_list<T> init, const allocator_type& alloc = allocator_type() );
//
// 	Vector& operator=(const Vector& other) = delete;
	Vector& operator=(Vector&& other) = default;
//
	reference at(size_type pos);
	local_reference lat(size_type pos);
// 	const_reference at(size_type pos) const;
//
	reference operator[](size_type pos);
// 	const_reference operator[](size_type pos) const;
//
 	reference front();
	local_reference lfront();
// 	const_reference front() const;
 	reference back();
	local_reference lback();
// 	const_reference back() const;
//
//
// 	pointer data();
// 	const_pointer data() const;
//
	iterator begin();
	iterator end();
	local_pointer lbegin();
	local_pointer lend();

// 	const_iterator begin() const;
// 	const_iterator end() const;
// 	const_iterator cbegin() const;
// 	const_iterator cend() const;
//
//
	bool empty();
	bool lempty();
	size_type lsize(); //const
	size_type size(); //const
	size_type max_size() const;
//
 	void reserve(size_type cap_per_node);
	size_type lcapacity() const;
	size_type capacity() const;

	// 	void shrink_to_fit();
	void clear();
	void lclear();
// 	void resize(size_type count, const_reference value = value_type());
//

// 	iterator insert(const_iterator pos, const T& value );
// 	iterator insert(const_iterator pos, T&& value );
// 	void insert(const_iterator pos, size_type count, const T& value );
// 	template< class InputIt >
// 	void insert(iterator pos, InputIt first, InputIt last);
// 	template< class InputIt >
// 	iterator insert(const_iterator pos, InputIt first, InputIt last );
// 	iterator insert(const_iterator pos, std::initializer_list<T> ilist );
	template< class InputIt >
	void linsert(InputIt first, InputIt last);
	template< class InputIt >
	void insert(InputIt first, InputIt last);

// 	template <class... Args>
// 	iterator emplace(const_iterator pos, Args&&... args);
// 	iterator erase(iterator pos);
// 	iterator erase(const_iterator pos);
	iterator erase(iterator first, iterator last);
	pointer lerase(pointer first, pointer last);
// 	iterator erase(const_iterator first, const_iterator last);
//


	void lpush_back(const T& value, vector_strategy_t strategy = vector_strategy_t::HYBRID);
	void push_back(const T& value, vector_strategy_t strategy = vector_strategy_t::HYBRID);
// 	void push_back(const T&& value);
//
// 	template <class... Args>
// 	void emplace_back(Args&&... args);
	void pop_back();
	void lpop_back();

	void lresize(size_type count);

	void balance();
	void commit();
	void barrier();

private:

	allocator_type _allocator;
// 	size_type _size;
// 	size_type _capacity;
	glob_mem_type _data;
	shared_local_sizes_mem_type _local_sizes;
	team_type& _team;
	std::vector<value_type> local_queue;
	std::vector<value_type> global_queue;
}; // End of class Vector

template <class T, template<class> class allocator>
Vector<T,allocator>::Vector(
	size_type local_elements,
	const_reference default_value,
	const allocator_type& alloc,
	team_type& team
) :
	_allocator(alloc),
	_data(local_elements, team),
	_local_sizes(1, team),
	_team(team)
{
	dash::atomic::store(*(_local_sizes.begin()+_team.myid()), local_elements);
	_team.barrier();
}

template <class T, template<class> class allocator>
typename Vector<T,allocator>::reference Vector<T,allocator>::at(size_type pos) {
	return begin()[pos];
}

template <class T, template<class> class allocator>
typename Vector<T,allocator>::local_reference Vector<T,allocator>::lat(size_type pos) {
	if(pos < lsize()) {
		return begin()[pos];
	} else {
		throw std::out_of_range();
	}
}


template <class T, template<class> class allocator>
typename Vector<T,allocator>::reference Vector<T,allocator>::operator[](size_type pos) {
	return begin()[pos];
}

template <class T, template<class> class allocator>
typename Vector<T,allocator>::reference Vector<T,allocator>::front() {
	return *(begin());
}


template <class T, template<class> class allocator>
typename Vector<T,allocator>::local_reference Vector<T,allocator>::lfront() {
	return *(lbegin());
}


template <class T, template<class> class allocator>
typename Vector<T,allocator>::reference Vector<T,allocator>::back() {
	return *(begin()+(size()-1));
}

template <class T, template<class> class allocator>
typename Vector<T,allocator>::local_reference Vector<T,allocator>::lback() {
	return *(lbegin()+(lsize()-1));
}

template <class T, template<class> class allocator>
typename Vector<T,allocator>::iterator Vector<T,allocator>::begin() {
	return iterator(*this, 0);
}

template <class T, template<class> class allocator>
typename Vector<T,allocator>::iterator Vector<T,allocator>::end() {
	return  iterator(*this, size());
}

template <class T, template<class> class allocator>
typename Vector<T,allocator>::local_pointer Vector<T,allocator>::lbegin() {
	return _data.lbegin();
}

template <class T, template<class> class allocator>
typename Vector<T,allocator>::local_pointer Vector<T,allocator>::lend() {
	return _data.lend();

}

template <class T, template<class> class allocator>
bool Vector<T,allocator>::empty() {
	return size() > 0;
}

template <class T, template<class> class allocator>
bool Vector<T,allocator>::lempty() {
	return lsize() > 0;
}

template <class T, template<class> class allocator>
typename Vector<T, allocator>::size_type Vector<T, allocator>::lsize() {
	return dash::atomic::load(*(_local_sizes.begin()+_team.myid()));
}

template <class T, template<class> class allocator>
typename Vector<T, allocator>::size_type Vector<T, allocator>::size() {
	size_t size = 0;
	for(auto s =_local_sizes.begin(); s != _local_sizes.begin()+_local_sizes.size(); s++) {
		size += static_cast<size_type>(*s);
	}
	return size;
}

template <class T, template<class> class allocator>
typename Vector<T, allocator>::size_type Vector<T, allocator>::max_size() const {
	return std::numeric_limits<size_type>::max();
}

template <class T, template<class> class allocator>
void Vector<T, allocator>::balance() {
	_team.barrier();

	const auto cap = capacity() / _team.size();
	const auto new_size = (size() + _team.size() - 1) / _team.size(); // Ceiling of size() / _team.size()
	const auto myid = _team.myid();
	glob_mem_type tmp(cap, _team);

	using block_t = std::pair<dart_gptr_t, size_t>;
	std::vector<block_t> blocks;

	index_type begin_index = new_size * myid;
	index_type end_index = std::max(new_size * (myid + 1), size());

	index_type pos = 0;
	index_type ls = dash::atomic::load(*(_local_sizes.begin()));
	index_type id = 0;
	index_type remaining = std::min(new_size, size() - myid * new_size);

	while(pos + ls < begin_index) {
		++id;
		pos += ls;
		ls = dash::atomic::load(*(_local_sizes.begin()+id));
	}

	auto loffset = (begin_index - pos);
	index_type lsize = dash::atomic::load(*(_local_sizes.begin()+id));
	auto tbegin = _data.begin() + (id*lcapacity() + loffset);
	auto tsize = std::min(remaining, static_cast<index_type>(lcapacity() - loffset));
	blocks.emplace_back(tbegin, tsize);


	remaining -= tsize;
	while(remaining > 0) {
		++id;
		lsize = dash::atomic::load(*(_local_sizes.begin()+id));
		tbegin = _data.begin() + (id*lcapacity());
		tsize = std::min(remaining, lsize);
		blocks.emplace_back(tbegin, tsize);
		remaining -= tsize;
	}

	auto dest = tmp.lbegin();
	long unsigned int my_size = 0;
	for(auto& block : blocks) {
		dart_get_blocking(dest, block.first, block.second, dart_datatype<T>::value, dart_datatype<T>::value);
		dest += block.second;
		my_size += block.second;
	}

	_team.barrier();
	dash::atomic::store(*(_local_sizes.begin()+myid), my_size);
	_data = std::move(tmp);
	_team.barrier();
}

template <class T, template<class> class allocator>
void Vector<T, allocator>::commit() {
// 	if(_team.myid() == 0) std::cout << "commit() lsize = "<< lsize() << " lcapacity = " << lcapacity() << std::endl;

	auto outstanding_global_writes = global_queue.size();
	size_type sum_outstanding_global_writes = 0;

// 		if(_team.myid() == 0) std::cout << "calc outstanding_global_writes" << std::endl;
	dart_reduce(&outstanding_global_writes, &sum_outstanding_global_writes, 1, DART_TYPE_ULONG, DART_OP_SUM, 0, _team.dart_id());

// 		if(_team.myid() == 0) std::cout << "calc max_outstanding_local_writes" << std::endl;
	auto outstanding_local_writes = local_queue.size();
	size_type max_outstanding_local_writes = 0;
	dart_reduce(&outstanding_local_writes, &max_outstanding_local_writes, 1, DART_TYPE_ULONG, DART_OP_MAX, 0 ,_team.dart_id());

	auto additional_local_size_needed = max_outstanding_local_writes + sum_outstanding_global_writes;

	dart_bcast(&additional_local_size_needed, 1, DART_TYPE_ULONG, 0, _team.dart_id());

// 	if(_team.myid() == 0) std::cout << "additional_local_size_needed =" << additional_local_size_needed << std::endl;
	if(additional_local_size_needed > 0) {
		const auto node_cap = std::max(capacity() / _team.size(), static_cast<size_type>(1));
		const auto old_cap = node_cap;
		const auto new_cap =
			node_cap * 1 << static_cast<size_type>(
				std::ceil(
					std::log2(
						static_cast<double>(node_cap+additional_local_size_needed) / static_cast<double>(node_cap)
					)
				)
			);
// 		std::cout << "new_cap = " << new_cap << " reserve memory..." << std::endl;
		reserve(new_cap);

// 		std::cout << "memory reserved." << std::endl;
// 		if(_team.myid() == 0) std::cout << "push local_queue elements" << std::endl;
		linsert(local_queue.begin(), local_queue.end());
		local_queue.clear();

// 		if(_team.myid() == 0) std::cout << "push global_queue elements" << std::endl;
		insert(global_queue.begin(), global_queue.end());
		global_queue.clear();
	}
// 	if(_team.myid() == 0) std::cout << "finished commit. lsize = "<< lsize() << " lcapacity = " << lcapacity() << std::endl;
}


template <class T, template<class> class allocator>
void Vector<T, allocator>::barrier() {
// 	commit();
	_team.barrier();
}

template <class T, template<class> class allocator>
typename Vector<T, allocator>::size_type Vector<T, allocator>::lcapacity() const {
	return _data.size() / _team.size();
}

template <class T, template<class> class allocator>
typename Vector<T, allocator>::size_type Vector<T, allocator>::capacity() const {
	return _data.size();
}

template <class T, template<class> class allocator>
void Vector<T, allocator>::lclear() {
	dash::atomic::store(*(_local_sizes.begin()+_team.myid()), 0);
}

template <class T, template<class> class allocator>
void Vector<T, allocator>::clear() {
	for(auto i = 0; i < _team.size(); ++i) {
		dash::atomic::store(*(_local_sizes.begin()+i), 0);
	}
}


template <class T, template<class> class allocator>
void Vector<T, allocator>::reserve(size_type new_cap) {
// 	std::cout << "reserve " << new_cap << std::endl;
	const auto old_cap = capacity() / _team.size();

	if(new_cap > old_cap) {
// 		if(_team.myid() == 0) std::cout << "alloc shared mem." << std::endl;
		glob_mem_type tmp(new_cap, _team);
		const auto i = _team.myid();
// 		if(_team.myid() == 0) std::cout << "copy data" << std::endl;
// 		std::copy(_data.begin() + i*old_cap, _data.begin()+(i*old_cap+lsize()), tmp.begin() + i*new_cap);
		std::copy(_data.lbegin(), _data.lbegin()+lsize(), tmp.lbegin());
// 		auto dest = tmp.begin() + i*new_cap;
// 		auto gptr = static_cast<dart_gptr_t>(_data.begin() + i*old_cap);
// 		dart_get_blocking(dest.local(), gptr, lsize(), dart_datatype<T>::value, dart_datatype<T>::value);
		_data = std::move(tmp);
	}

	_team.barrier();
// 	if(_team.myid() == 0) std::cout << "reserved." << std::endl;
}

template <class T, template<class> class allocator>
template< class InputIt >
void Vector<T, allocator>::linsert(InputIt first, InputIt last) {
	if(first == last) return;

	const auto distance = std::distance(first,last);
// 	if(_team.myid() == 1) std::cout << "lcapacity = " << lcapacity() << std::endl;

	const auto lastSize = dash::atomic::fetch_add(*(_local_sizes.begin()+_team.myid()), static_cast<size_type>(distance));
// 	if(_team.myid() == 1) std::cout << "lastSize = " << lastSize << std::endl;

	const auto direct_fill = std::min(static_cast<size_type>(distance), lcapacity() - lastSize);
// 	if(_team.myid() == 1) std::cout << "direct_fill = " << direct_fill << std::endl;

	const auto direct_fill_end = first + direct_fill;
	std::copy(first, direct_fill_end, _data.lbegin() + lastSize);

	if(direct_fill_end != last) {
		dash::atomic::store(*(_local_sizes.begin()+_team.myid()), lcapacity());
		local_queue.insert(local_queue.end(), direct_fill_end, last);
	}
}

template <class T>
T clamp(const T value, const T lower, const T higher) {
	return std::min(std::max(value, lower), higher);
}

template <class T, template<class> class allocator>
template< class InputIt >
void Vector<T, allocator>::insert(InputIt first, InputIt last) {
	if(first == last) return;

	const auto distance = std::distance(first,last);
// 	if(_team.myid() == 0) std::cout << "distance = " << distance << std::endl;
// 	if(_team.myid() == 0) std::cout << "lcapacity = " << lcapacity() << std::endl;

	const auto lastSize = dash::atomic::fetch_add(*(_local_sizes.begin()+(_local_sizes.size()-1)), static_cast<size_type>(distance));
// 	std::cout << "id("<< _team.myid() << ") "<<  "lastSize = " << lastSize << std::endl;

	const auto direct_fill = clamp(static_cast<decltype(distance)>(lcapacity() - lastSize), static_cast<decltype(distance)>(0), distance);
// 	std::cout << "id("<< _team.myid() << ") "<< "direct_fill = " << direct_fill << std::endl;

	const auto direct_fill_end = first + direct_fill;

// 	std::copy(first, direct_fill_end, _data.begin() + (lcapacity()*(_team.size()-1) + lastSize));
	auto gptr =_data.begin() + (lcapacity()*(_team.size()-1) + lastSize);
	std::vector<value_type> buffer(first, direct_fill_end);

	if(buffer.size() > 0) {
		dart_put_blocking(
			static_cast<dart_gptr_t>(gptr),
			buffer.data(),
			buffer.size(),
			dart_datatype<T>::value,
			dart_datatype<T>::value
		);
	}

	// This is a unneccessary copy to get a void* from a (forward-)iterator. In order to prevent this,
	// you have to either change the interface of dash::vector::insert() or dart_put_blocking() or
	// specialise for every iterator type or write a conversion class from iterator to void* using
	// type traits.
// 	std::vector<T> buffer(first, first+direct_fill);
//
// 	auto gptr = static_cast<dart_gptr_t>(_data.begin() + (lcapacity()*(_team.size()-1) + lastSize));
// 	dart_put_blocking(
// 		gptr,
// 		buffer.data(),
// 		direct_fill,
// 		dart_datatype<T>::value,
// 		dart_datatype<T>::value
// 	);

	if(direct_fill_end != last) {
		dash::atomic::store(*(_local_sizes.begin()+(_team.size()-1)), lcapacity());
		global_queue.insert(local_queue.end(), direct_fill_end, last);
	}
// 	std::cout << "id("<< _team.myid() << ") " << "done" << std::endl;
}

template <class T, template<class> class allocator>
typename Vector<T, allocator>::iterator Vector<T, allocator>::erase(iterator first,iterator  last) {
	const auto dist = std::distance(first, last);
	auto back_fill_it = first + dist;
	for(auto i = first; i !=  end() - dist; ++i) {
		*i = *back_fill_it;
		++back_fill_it;
	}
	dash::atomic::sub(*(_local_sizes.begin() + _team.size() - 1), dist);
}

template <class T, template<class> class allocator>
typename Vector<T, allocator>::pointer Vector<T, allocator>::lerase(pointer first, pointer last) {
	const auto dist = std::distance(first, last);
	auto back_fill_it = first + dist;
	for(auto i = first; i !=  lend() - dist; ++i) {
		*i = *back_fill_it;
		++back_fill_it;
	}
	dash::atomic::sub(*(_local_sizes.begin() + _team.myid()), dist);
}

template <class T, template<class> class allocator>
void Vector<T, allocator>::lpush_back(const T& value, vector_strategy_t strategy) {
	if(strategy == vector_strategy_t::CACHE) {
		local_queue.push_back(value);
		return;
	}

	const auto lastSize = dash::atomic::fetch_add(*(_local_sizes.begin()+_team.myid()), static_cast<size_type>(1));
	if(lastSize < lcapacity()) {
		*(_data.lbegin() + lastSize) = value;
	} else {
		if(strategy == vector_strategy_t::WRITE_THROUGH) throw std::runtime_error("Space not sufficient");
		dash::atomic::sub(*(_local_sizes.begin()+_team.myid()), static_cast<size_type>(1));
		local_queue.push_back(value);
	}
}

template <class T, template<class> class allocator>
void Vector<T, allocator>::push_back(const T& value,  vector_strategy_t strategy) {
	if(strategy == vector_strategy_t::CACHE) {
		global_queue.push_back(value);
		return;
	}

	const auto lastSize = dash::atomic::fetch_add(*(_local_sizes.begin()+(_local_sizes.size()-1)), static_cast<size_type>(1));
// 	if(_team.myid() == 0) std::cout << "push_back() lastSize = " << lastSize << " lcapacity = " << lcapacity() << std::endl;
	if(lastSize < lcapacity()) {
// 		std::cout << "enough space" << std::endl;
		*(_data.begin() + lcapacity()*(_team.size()-1) + lastSize) = value;
// 		std::cout << "written" << std::endl;
	} else {
// 		std::cout << "not enough space" << std::endl;
		if(strategy == vector_strategy_t::WRITE_THROUGH) throw std::runtime_error("Space not sufficient");

		dash::atomic::sub(*(_local_sizes.begin()+(_local_sizes.size()-1)), static_cast<size_type>(1));
		global_queue.push_back(value);
// 		std::cout << "delayed" << std::endl;
	}
}

template <class T, template<class> class allocator>
void Vector<T, allocator>::pop_back() {
	dash::atomic::sub(*(_local_sizes.begin() + _team.size() - 1), 1);
}

template <class T, template<class> class allocator>
void Vector<T, allocator>::lpop_back() {
	dash::atomic::sub(*(_local_sizes.begin()+_team.myid()), 1);
}

template <class T, template<class> class allocator>
void Vector<T, allocator>::lresize(size_type count) {
	const auto lsize = lsize();
	reserve(count);
	if(count > lsize) {
		std::fill(lend(), lend() + count - lsize, T{});
	}
	dash::atomic::store(*(_local_sizes.begin()+_team.myid()), count);
}

// template<class T>
// LocalRange<typename Vector<T>::value_type>
// local_range(
//   /// Iterator to the initial position in the global sequence
//   Vector_iterator<Vector<T>> & first,
//   /// Iterator to the final position in the global sequence
//   Vector_iterator<Vector<T>> & last)
// {
// 	return LocalRange<typename Vector<T>::value_type>(first._vec.lbegin(), first._vec.lend());
// }

namespace detail {
	template<class InputIt1, class InputIt2, class Compare>
	bool lexicographical_compare(InputIt1 first1, InputIt1 last1,
								InputIt2 first2, InputIt2 last2,
								Compare comp)
	{
		for ( ; (first1 != last1) && (first2 != last2); ++first1, (void) ++first2 ) {
			if (comp(*first1, *first2)) return true;
			if (comp(*first2, *first1)) return false;
		}
		return (first1 == last1) && (first2 != last2);
	}
}
template <class T, template<class> class allocator>
bool operator==(const dash::Vector<T, allocator>& lhs, const dash::Vector<T, allocator>& rhs) {
	return detail::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), std::equal_to<T>());
}

template <class T, template<class> class allocator>
bool operator!=(const dash::Vector<T, allocator>& lhs, const dash::Vector<T, allocator>& rhs) {
	return detail::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), std::not_equal_to<T>());
}

template <class T, template<class> class allocator>
bool operator<=(const dash::Vector<T, allocator>& lhs, const dash::Vector<T, allocator>& rhs) {
	return detail::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), std::less_equal<T>());
}

template <class T, template<class> class allocator>
bool operator<(const dash::Vector<T, allocator>& lhs, const dash::Vector<T, allocator>& rhs) {
	return detail::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), std::less<T>());
}

template <class T, template<class> class allocator>
bool operator>=(const dash::Vector<T, allocator>& lhs, const dash::Vector<T, allocator>& rhs) {
	return detail::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), std::greater_equal<T>());
}

template <class T, template<class> class allocator>
bool operator>(const dash::Vector<T, allocator>& lhs, const dash::Vector<T, allocator>& rhs) {
	return detail::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), std::greater<T>());
}

} // End of namespace dash

