#pragma once

namespace dash {

template <bool pred, class T1, class T2>
struct conditional_type;

template <class T1, class T2>
struct conditional_type<true, T1, T2>
{
	using type = T1;
};

template <class T1, class T2>
struct conditional_type<false, T1, T2>
{
	using type = T2;
};

}
