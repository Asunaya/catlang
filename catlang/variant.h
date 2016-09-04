#pragma once

#include <utility>
#include <type_traits>
#include <new>
#include <cassert>
#include <memory>

#define MAX_PROPERTY(prop) \
template <typename T, typename... rest> \
struct max_##prop \
{ \
    using type = std::conditional_t<(prop(T) > prop(typename max_##prop<rest...>::type)), T, typename max_##prop<rest...>::type>; \
}; \
 \
template <typename T> \
struct max_##prop<T> \
{ \
	using type = T; \
}; \
 \
template <typename... Ts> \
using max_##prop##_t = typename max_##prop<Ts...>::type

MAX_PROPERTY(alignof);
MAX_PROPERTY(sizeof);
#undef MAX_PROPERTY

template <bool b, bool... rest>
struct any_of
{
	static constexpr bool value = b || any_of<rest...>::value;
};

template <bool b>
struct any_of<b>
{
	static constexpr bool value = b;
};

template <int index, typename T, typename U, typename... Ts>
struct get_index_impl
{
	static constexpr int value = std::conditional_t<std::is_same<T, U>::value,
		std::integral_constant<int, index>,
		get_index_impl<index + 1, T, Ts... >> ::value;
};

template <int index, typename T, typename U>
struct get_index_impl<index, T, U>
{
	static constexpr int value = index;
	static_assert(std::is_same<T, U>::value, "");
};

template <typename T, typename U, typename... Ts>
struct get_index
{
	static constexpr int value = get_index_impl<0, T, U, Ts...>::value;
};

template <size_t alignment, size_t size, typename... Ts>
struct variant_impl
{
	template <typename T, typename = std::enable_if_t<any_of<std::is_same<std::remove_reference_t<T>, Ts>::value...>::value>>
	variant_impl(T&& obj)
	{
		new (buffer) std::remove_reference_t<T>(std::forward<T>(obj));
		type_index = get_index<T, Ts...>::value;
	}

	variant_impl(const variant_impl& src)
	{
		src.visit([&](auto&& val) {
			using real_t = std::remove_const_t<std::remove_reference_t<decltype(val)>>;
			new (buffer) real_t(reinterpret_cast<const real_t&>(src.buffer));
			type_index = get_index<real_t, Ts...>::value;
		});
	}

	variant_impl(variant_impl&& src)
	{
		src.visit([&](auto&& val) {
			using real_t = std::remove_const_t<std::remove_reference_t<decltype(val)>>;
			new (buffer) real_t(reinterpret_cast<real_t&&>(src.buffer));
			type_index = get_index<real_t, Ts...>::value;
		});
	}

	variant_impl& operator =(const variant_impl& src)
	{
		this->~variant_impl();
		new (this) variant_impl(src);
		return *this;
	}

	variant_impl& operator =(variant_impl&& src)
	{
		this->~variant_impl();
		new (this) variant_impl(std::move(src));
		return *this;
	}

	~variant_impl()
	{
		visit([&](auto&& val) {
			using type = std::remove_reference_t<decltype(val)>;
			val.~type();
		});
	}

	template <typename fn_t>
	void visit(fn_t&& fn) { visit_impl<fn_t, 0, Ts...>(std::forward<fn_t>(fn)); }
	template <typename fn_t>
	void visit(fn_t&& fn) const { visit_impl<fn_t, 0, Ts...>(std::forward<fn_t>(fn)); }

	void* get_pointer() { return buffer; }
	const void* get_pointer() const { return buffer; }

	template <typename T, typename = std::enable_if_t<any_of<(std::is_same<T, Ts>::value || std::is_base_of<T, Ts>::value)...>::value>>
	T& get_ref()
	{
		return reinterpret_cast<T&>(buffer);
	}

	auto get_type_index() const { return type_index; }

private:
	template <typename fn_t, int cur_type_index, typename cur_type, typename... rest>
	void visit_impl(fn_t&& fn)
	{
		if (cur_type_index == type_index)
			fn(reinterpret_cast<cur_type&>(buffer));
		else
			visit_impl<fn_t, cur_type_index + 1, rest...>(std::forward<fn_t>(fn));
	}
	template <typename fn_t, int cur_type_index>
	void visit_impl(fn_t&&) { assert(false); }

	template <typename fn_t, int cur_type_index, typename cur_type, typename... rest>
	void visit_impl(fn_t&& fn) const
	{
		if (cur_type_index == type_index)
			fn(reinterpret_cast<const cur_type&>(buffer));
		else
			visit_impl<fn_t, cur_type_index + 1, rest...>(std::forward<fn_t>(fn));
	}
	template <typename fn_t, int cur_type_index>
	void visit_impl(fn_t&&) const { assert(false); }

	int type_index;
	alignas(alignment) char buffer[size];
};

template <typename... Ts>
using variant = variant_impl<alignof(max_alignof_t<Ts...>), sizeof(max_sizeof_t<Ts...>), Ts...>;

template <typename T, typename = void>
struct is_complete : std::false_type {};

template <typename T>
struct is_complete<T, decltype(void(sizeof(T)))> : std::true_type {};

template <bool requires_completeness, template <typename...> class container, typename... Ts>
struct recursive_variant_impl;

template <template <typename...> class container, typename... Ts>
struct recursive_variant_impl<true, container, Ts...>
	: variant_impl<
	alignof(max_alignof_t<container<int>, Ts...>),
	sizeof(max_sizeof_t<container<int>, Ts...>),
	container<recursive_variant_impl<true, container, Ts...>>,
	Ts...>
{
	using base = variant_impl<
		alignof(max_alignof_t<container<int>, Ts...>),
		sizeof(max_sizeof_t<container<int>, Ts...>),
		container<recursive_variant_impl<true, container, Ts...>>,
		Ts...>;
	using base::base;
	using container_of_this = container<recursive_variant_impl<true, container, Ts...>>;
	static_assert(sizeof(container<int>) >= sizeof(container_of_this),
		"container is specialized with a larger size for recursive_variant than it has for int! not good!");
};

template <template <typename...> class container, typename... Ts>
struct recursive_variant_impl<false, container, Ts...>
	: variant_impl<
	alignof(max_alignof_t<std::unique_ptr<container<recursive_variant_impl<false, container, Ts...>>>, Ts...>),
	sizeof(max_sizeof_t<std::unique_ptr<container<recursive_variant_impl<false, container, Ts...>>>, Ts...>),
	std::unique_ptr<container<recursive_variant_impl<false, container, Ts...>>>,
	Ts...>
{
	using base = variant_impl<
		alignof(max_alignof_t<std::unique_ptr<container<recursive_variant_impl<false, container, Ts...>>>, Ts...>),
		sizeof(max_sizeof_t<std::unique_ptr<container<recursive_variant_impl<false, container, Ts...>>>, Ts...>),
		std::unique_ptr<container<recursive_variant_impl<false, container, Ts...>>>,
		Ts...>;
	using base::base;
};

template <template <typename...> class container, typename... Ts>
using recursive_variant = recursive_variant_impl<
	is_complete<container<recursive_variant_impl<true, container, Ts...>>>::value, container, Ts...>;