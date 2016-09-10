#pragma once

#include <string>
#include <array>
#include <unordered_map>
#include <functional>
#include <memory>
#include "basic_types.h"
#include "util.h"

using variable_map_t = std::unordered_map<std::string, std::shared_ptr<object>>;

struct context_t
{
	object evaluate_list(const object& list);

	void add_variable(const std::string& name, std::shared_ptr<object> obj) {
		variable_map.insert_or_assign(name, obj);
	}

	struct interpreter_t& interpreter;
	variable_map_t variable_map;
};

struct interpreter_t
{
	interpreter_t();

	list_t get_string_list(const std::string& expr);
	list_t get_abstract_syntax_tree(const list_t& sst);
	object expand_list(const list_t& list);
	void interpret_line(const std::string& expr);

	std::unordered_map<std::string, builtin_func_t> statements;
	context_t global_context;
};