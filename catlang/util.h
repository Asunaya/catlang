#pragma once

#include <string>
#include "basic_types.h"

std::string slice(const std::string& expr, int start, int end);
list_t slice(const list_t& list, int start, int end);
template <typename T>
auto slice(T&& expr, int start) {
	return slice(expr, start, static_cast<int>(expr.size()));
}

std::pair<bool, int64_t> string_to_int64(const std::string& str);
std::pair<bool, double> string_to_double(const std::string& str);