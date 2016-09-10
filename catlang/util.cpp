#include "util.h"

std::string slice(const std::string& expr, int start, int end)
{
	if (end < 0)
		end = static_cast<int>(expr.length()) + end;
	return expr.substr(start, end - start);
}

list_t slice(const list_t& list, int start, int end)
{
	if (end < 0)
		end = static_cast<int>(list.size() - end);

	return list_t{ list.begin() + start, list.end() - (list.size() - end) };
}

std::pair<bool, int64_t> string_to_int64(const std::string& str)
{
	char* endptr = nullptr;
	auto ret = strtoll(str.data(), &endptr, 10);
	if (endptr != str.data() + str.length())
		return{ false, 0 };

	return{ true, ret };
}

std::pair<bool, double> string_to_double(const std::string& str)
{
	char* endptr = nullptr;
	auto ret = strtod(str.data(), &endptr);
	if (endptr != str.data() + str.length())
		return{ false, 0 };

	return{ true, ret };
}