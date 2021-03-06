#pragma once

#include "variant.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
using std::int64_t;

#define TOKENIZE_IMPL(a, b) a##b
#define TOKENIZE(a, b) TOKENIZE_IMPL(a, b)

template <typename T>
struct list_impl : std::vector<T>
{
	using std::vector<T>::vector;

	template <typename U>
	void visit(U&& fn)
	{
		for (auto&& item : *this)
			item.visit(fn);
	}

	template <typename U>
	void visit(U&& fn) const
	{
		for (auto&& item : *this)
			item.visit(fn);
	}

	bool quoted = false;
};

template <typename tag>
struct tagged_string : std::string
{
	using std::string::string;
	using std::string::operator=;

	tagged_string(const std::string& str)
	{
		*this = str;
	}
};

using statement = tagged_string<struct statement_tag>;
using variable_reference = tagged_string<struct variable_reference_tag>;

struct nil_t {};

template <typename T>
struct lambda_impl
{
	std::vector<std::string> parameters;
	list_impl<T> body;
	std::unordered_map<std::string, std::shared_ptr<T>> context;
};

using object = recursive_variant<
	nil_t,
	bool,
	int64_t,
	double,
	std::string,
	statement,
	variable_reference,
	heap_wrapper<lambda_impl<recursive_variant_tag>>,
	std::function<recursive_variant_tag(const list_impl<recursive_variant_tag>&, struct context_t&)>,
	list_impl<recursive_variant_tag>>;

using list_t = list_impl<object>;
using lambda_t = lambda_impl<object>;
using builtin_func_t = std::function<object(const list_t&, struct context_t&)>;

inline std::ostream& operator <<(std::ostream& lhs, nil_t)
{
	lhs << "(nil)";
	return lhs;
}

inline std::ostream& operator <<(std::ostream& lhs, const list_t& rhs);

inline std::ostream& operator <<(std::ostream& lhs, const lambda_t& rhs)
{
	lhs << "lambda: {parameters:";
	for (auto&& item : rhs.parameters)
		lhs << " " << item;
	lhs << std::endl;
	lhs << ", body: " << rhs.body << "}\n";
	return lhs;
}

inline std::ostream& operator <<(std::ostream& lhs, const builtin_func_t&)
{
	lhs << "(builtin func)";
	return lhs;
}

inline std::ostream& operator <<(std::ostream& lhs, const object& rhs)
{
	rhs.visit([&](auto&& item) { lhs << item << " "; });
	return lhs;
}

inline std::ostream& operator <<(std::ostream& lhs, const list_t& rhs)
{
	lhs << "{ ";
	rhs.visit([&](auto&& item) { lhs << item << " "; });
	lhs << "}";
	return lhs;
}

#ifdef _MSC_VER
#define TYPE_CHECK auto check = [&](auto&& val) { return val.is_type<int64_t>() || val.is_type<double>(); };
#else
#define TYPE_CHECK auto check = [&](auto&& val) { return val.template is_type<int64_t>() || val.template is_type<double>(); };
#endif
#define MAKE_SEXPR_OP_OVERLOAD_IMPL(op, op_name) \
template <typename T1, typename T2, typename... types> \
std::enable_if_t<is_part_of<T1, types...>::value && is_part_of<T2, types...>::value, object> op_name(T1&& lhs, T2&& rhs) \
{ \
	return lhs op rhs; \
} \
template <typename T1, typename T2, typename... types> \
std::enable_if_t<!(is_part_of<T1, types...>::value && is_part_of<T2, types...>::value), object> op_name(T1&&, T2&&) \
{ \
	return nil_t{}; \
} \
inline object operator op(const object& lhs, const object& rhs) \
{ \
	TYPE_CHECK \
	if (!check(lhs) || !check(rhs)) \
		throw std::runtime_error{ std::string{"Can't " #op " types "} +std::to_string(lhs.get_type_index()) + " and " + std::to_string(rhs.get_type_index()) }; \
	object ret{ 0ll }; \
	lhs.visit([&](auto&& lhs_item) \
	{ \
		/* Fix for bug with decltype in nested lambdas in GCC 4.9.3 */ \
		using lhs_type = decltype(lhs_item); \
		rhs.visit([&](auto&& rhs_item) \
		{ \
			ret = op_name<lhs_type, decltype(rhs_item), int64_t, double>(lhs_item, rhs_item); \
		}); \
	}); \
	return ret; \
}

#define MAKE_SEXPR_OP_OVERLOAD(op) MAKE_SEXPR_OP_OVERLOAD_IMPL(op, TOKENIZE(do_op, __COUNTER__))

MAKE_SEXPR_OP_OVERLOAD(+)
MAKE_SEXPR_OP_OVERLOAD(-)
MAKE_SEXPR_OP_OVERLOAD(*)
MAKE_SEXPR_OP_OVERLOAD(/)
MAKE_SEXPR_OP_OVERLOAD(<)
MAKE_SEXPR_OP_OVERLOAD(>)
MAKE_SEXPR_OP_OVERLOAD(<=)
MAKE_SEXPR_OP_OVERLOAD(>=)
