#pragma once

#include <vector>
#include <cstdint>
#include <type_traits>

namespace dash
{
class ContigiousStreamBuffer
{
public:
    ContigiousStreamBuffer() = default;

    const std::uint8_t*
    data() const {
		return m_data.data()+m_offset;
	}

    std::uint8_t*
    data()  {
		return m_data.data()+m_offset;
	}

    size_t
    size() const{
		return m_data.size()-m_offset;
	}

	void
	resize(size_t new_size) {
		m_data.resize(new_size);
	}

	ContigiousStreamBuffer&
	append( const void*   data,
									const size_t& size )
	{
		pack();
		std::copy(
			reinterpret_cast<const std::uint8_t*>( data ),       //NOLINT
			reinterpret_cast<const std::uint8_t*>( data ) + size,     //NOLINT
			std::back_inserter( m_data )
			);
		return *this;
	}

    ContigiousStreamBuffer&
    operator+=( const ContigiousStreamBuffer& rhs) {
		pack();
		std::copy( rhs.m_data.begin(), rhs.m_data.end(), std::back_inserter( m_data ) );
		return *this;
	}

    ContigiousStreamBuffer&
    operator-=( size_t bytes ) {
		m_offset += bytes;
		return *this;
	}

    template<class T>
    std::remove_const_t<T>
    get();

private:
    void
    pack() {
		if ( m_offset != 0 ) {
			/* If data has been read for the stream, shift all data to the begining of the contigious memory to avoid
			* further allocation */
			std::copy( m_data.begin() + m_offset, m_data.end(), m_data.begin() );
			// reset the size and offset
			m_data.resize( size() );
			m_offset = 0;
		}
	}

    std::vector<std::uint8_t> m_data;
    size_t m_offset = 0;
};


/**
 * Forward declare serialize functions to make the visible between another
 */
/*
template<
    class T,
    typename std::enable_if<
        std::is_arithmetic<
            typename std::remove_reference<
                T
                >::type
            >::value,
        int
        >::type
    >
ContigiousStreamBuffer&
operator<<( ContigiousStreamBuffer& lhs,
            const T&                rhs );
template<
    class T,
    typename std::enable_if<
        std::is_arithmetic<
			T
		>::value,
        int
        >::type
    >
ContigiousStreamBuffer&
operator>>( ContigiousStreamBuffer& lhs,
            T&                      rhs );*/

template <class T, class ALLOC>
ContigiousStreamBuffer&
operator<<( ContigiousStreamBuffer& lhs,
            const std::vector<T, ALLOC>&      rhs );

template <class T, class ALLOC>
ContigiousStreamBuffer&
operator>>( ContigiousStreamBuffer& lhs,
           const std::vector<T, ALLOC>&      rhs );

template <class KEY, class VALUE, class ALLOC>
ContigiousStreamBuffer&
operator<<( ContigiousStreamBuffer& lhs,
            const std::map<KEY, VALUE, ALLOC>&      rhs );

template <class KEY, class VALUE, class ALLOC>
ContigiousStreamBuffer&
operator>>( ContigiousStreamBuffer& lhs,
            std::map<KEY, VALUE, ALLOC>&      rhs );

ContigiousStreamBuffer&
operator<<( ContigiousStreamBuffer& lhs,
            const std::string&      rhs );

ContigiousStreamBuffer&
operator>>( ContigiousStreamBuffer& lhs,
            std::string&      rhs );

/**
 * Implementation
 */
inline
ContigiousStreamBuffer&
operator<<( ContigiousStreamBuffer& lhs,
            const void*  const               rhs )
{
    return lhs.append( &rhs, sizeof( rhs ) );
}

inline
ContigiousStreamBuffer&
operator>>( ContigiousStreamBuffer& lhs,
            const void*&             rhs )
{
	std::memcpy(&rhs, lhs.data(), sizeof(void*));
	lhs -= sizeof( void* );
	return lhs;
}

template<
    class T,
    typename std::enable_if<
        std::is_arithmetic<
            typename std::remove_reference<
                T
                >::type
            >::value,
        int
        >::type enable = 0
    >
ContigiousStreamBuffer&
operator<<( ContigiousStreamBuffer& lhs,
            T                rhs )
{
    return lhs.append( &rhs, sizeof( rhs ) );
}

template<
    class T,
    typename std::enable_if<
        std::is_arithmetic<
			T
            >::value,
        int
        >::type enable = 0
    >
ContigiousStreamBuffer&
operator>>( ContigiousStreamBuffer& lhs,
            T&                      rhs )
{
    rhs = *reinterpret_cast<const T*>( lhs.data() );
    lhs -= sizeof( T );
    return lhs;
}

template <class T, class ALLOC>
ContigiousStreamBuffer&
operator<<( ContigiousStreamBuffer& lhs,
            const std::vector<T, ALLOC>&      rhs )
{
    lhs << rhs.size();
    for ( const auto& v : rhs ) {
        lhs << v;
    }
    return lhs;
}

template <class T, class ALLOC>
ContigiousStreamBuffer&
operator>>( ContigiousStreamBuffer& lhs,
           std::vector<T, ALLOC>&      rhs )
{
	size_t size;
	lhs >> size;
	rhs.clear();
	rhs.reserve(size);
	for(size_t i = 0; i < size; ++i) {
		T value;
		lhs >> value;
		rhs.emplace_back(std::move(value));
	}
    return lhs;
}

template <class KEY, class VALUE, class ALLOC>
ContigiousStreamBuffer&
operator<<( ContigiousStreamBuffer& lhs,
            const std::map<KEY, VALUE, ALLOC>&      rhs )
{
    lhs << rhs.size();
    for ( const auto& v : rhs ) {
        lhs << v.first;
		lhs << v.second;
    }
    return lhs;
}

template <class KEY, class VALUE, class ALLOC>
ContigiousStreamBuffer&
operator>>( ContigiousStreamBuffer& lhs,
            std::map<KEY, VALUE, ALLOC>&      rhs )
{
	size_t size;
	lhs >> size;
	rhs.clear();
	for(size_t i = 0; i < size; ++i) {
		KEY key;
		lhs >> key;
		lhs >> rhs[key];
	}
    return lhs;
}

inline
ContigiousStreamBuffer&
operator<<( ContigiousStreamBuffer& lhs,
            const std::string&      rhs )
{
	size_t size = rhs.size();
    lhs << size;
	for ( const auto& c : rhs ) {
        lhs << c;
    }
    return lhs;
}

inline
ContigiousStreamBuffer&
operator>>( ContigiousStreamBuffer& lhs,
            std::string&      rhs )
{
	size_t size;
	lhs >> size;
	rhs.resize(size);
	for(auto& c : rhs) {
		lhs >> c;
	}
    return lhs;
}

}
