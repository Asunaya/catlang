#pragma once

#include <string>
#include <array>
#include <unordered_map>
#include <functional>
#include "basic_types.h"
#include "util.h"

struct interpreter_t
{
	interpreter_t();

	using variable_map = std::unordered_map<std::string, object>;

	list_t get_string_list(const std::string& expr);
	list_t get_abstract_syntax_tree(const list_t& sst);
	object expand_list(const list_t& list);
	object evaluate_list(const object& list, variable_map& variables);
	object evaluate_list(const object& list);
	void interpret_line(const std::string& expr);

private:
	variable_map variables;
	std::unordered_map<std::string, std::function<object(const list_t&, variable_map&)>> statements;
};