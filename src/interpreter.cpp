#include "interpreter.h"
#include <iostream>
#ifdef __MINGW32__
#define __NO_INLINE__
#endif
#include <algorithm>

static std::array<int, 2> find_next_token(const std::string& expr, size_t pos)
{
	if (expr.empty())
		return{ {-1, -1} };

	if (expr[pos] == ')')
		return{ {-1, -1} };

	int start = 0;

	if (expr[0] == '(')
	{
		start = static_cast<int>(pos + 1);
		while (expr[start] == ' ')
			++start;
	}

	if (expr[start] == ')')
		return{ {-1, -1} };

	auto end = start + 1;

	while (end < static_cast<int>(expr.length()) && expr[end] != ' ' && expr[end] != ')')
		++end;

	return{ {start, end} };
}

static int find_sexpr_end(const std::string& expr, int start_pos)
{
	auto parens = 1;
	auto pos = start_pos + 1;

	while (parens)
	{
		if (expr[pos] == '(')
			++parens;
		else if (expr[pos] == ')')
			--parens;

		pos++;
	}

	return pos;
}

list_t interpreter_t::get_string_list(const std::string& expr)
{
	list_t ret_list;

	size_t cur_pos = 0;

	while (cur_pos < expr.length())
	{
		auto token = find_next_token(expr, cur_pos);

		if (token[0] == -1 || token[1] == -1)
			break;

		auto make_list = [&](bool is_code)
		{
			auto end = is_code ? find_sexpr_end(expr, token[0]) : token[0] + 1;
			auto sexpr_string = slice(expr, token[0], end);
			auto sexpr_list = get_string_list(sexpr_string);
			sexpr_list.quoted = !is_code;
			ret_list.emplace_back(std::move(sexpr_list));
			cur_pos = end;
		};

		if (expr[token[0]] == '(')
			make_list(true);
		else if (expr[token[0]] == '\'' && expr[token[0] + 1] == '(')
			make_list(false);
		else
		{
			ret_list.emplace_back(slice(expr, token[0], token[1]));
			cur_pos = token[1];
		}
	}

	return ret_list;
}

list_t interpreter_t::get_abstract_syntax_tree(const list_t& sst)
{
	list_t ret_list;

	for (auto& item : sst)
	{
		if (item.is_type<list_t>())
			ret_list.emplace_back(get_abstract_syntax_tree(item.get_ref<list_t>()));
		else
		{
			assert(item.is_type<std::string>());
			auto&& str = item.get_ref<std::string>();
			auto int64_result = string_to_int64(str);
			auto double_result = string_to_double(str);
			if (int64_result.first)
				ret_list.emplace_back(int64_result.second);
			else if (double_result.first)
				ret_list.emplace_back(double_result.second);
			else if (str[0] == '\"' && str[str.length() - 1] == '\"')
				ret_list.emplace_back(std::string{ slice(str, 1, -1) });
			else
			{
				auto it = statements.find(str);
				if (it == statements.end())
					ret_list.emplace_back(variable_reference{ str });
				else
					ret_list.emplace_back(statement{ str });
			}
		}
	}

	return ret_list;
}

object interpreter_t::expand_list(const list_t & list)
{
	return list;
}

object context_t::evaluate_list(const object& obj)
{
	if (!obj.is_type<list_t>())
	{
		if (obj.is_type<variable_reference>())
		{
			auto&& var = obj.get_ref<variable_reference>();
			auto it = variable_map.find(var);
			if (it == variable_map.end())
				throw std::runtime_error{ std::string{ "Undefined variable " } +var };
			return *it->second;
		}
		return obj;
	}

	auto&& list = obj.get_ref<list_t>();

	if (list.quoted)
		return list;

	if (list.empty())
		return nil_t{};

	if (list[0].is_type<variable_reference>())
	{
		auto&& var = list[0].get_ref<variable_reference>();
		list_t new_list;
		new_list.emplace_back(*variable_map.at(var));
		new_list.insert(new_list.end(), list.begin() + 1, list.end());
		return evaluate_list(new_list);
	}

	if (list[0].is_type<statement>())
	{
		auto stmt = list[0].get_ref<statement>();
		auto it = interpreter.statements.find(stmt);
		if (it == interpreter.statements.end())
			throw std::runtime_error{ std::string{ "Unknown statement " } +stmt };
		return it->second(list, *this);
	}

	if (list[0].is_type<lambda_t>())
	{
		auto&& lambda = list[0].get_ref<lambda_t>();
		auto new_context = context_t{interpreter, lambda.context};

		int i = 1;
		for (auto&& param : lambda.parameters)
		{
			new_context.add_variable(param, std::make_shared<object>(evaluate_list(list[i])));
			++i;
		}

		return new_context.evaluate_list(lambda.body);
	}

	if (list[0].is_type<builtin_func_t>())
	{
		auto&& func = list[0].get_ref<builtin_func_t>();

		return evaluate_list(func(list, *this));
	}

	if (list.size() == 1)
		return evaluate_list(list[0]);

	list_t ret_list;

	for (auto&& item : list)
		ret_list.emplace_back(evaluate_list(item));

	return evaluate_list(ret_list);
}

variable_map_t context_t::get_lambda_context(const std::vector<std::string>& params, const list_t& list) const
{
	variable_map_t ret;

	for (auto&& item : list)
	{
		if (item.is_type<list_t>())
		{
			auto result = get_lambda_context(params, item.get_ref<list_t>());
			ret.insert(result.begin(), result.end());
			continue;
		}
		
		if (item.is_type<variable_reference>())
		{
			auto&& variable_name = item.get_ref<variable_reference>();
			if (std::binary_search(params.begin(), params.end(), variable_name))
				continue;
			auto it = variable_map.find(variable_name);
            if (it != variable_map.end())
            {
                ret.emplace(*it);
            }
		}
	}

	return ret;
}

void context_t::add_variable(const std::string& name, std::shared_ptr<object> obj)
{
		auto it = variable_map.find(name);
		if (it == variable_map.end())
			variable_map.emplace(std::make_pair(name, obj));
		else
			it->second = obj;
	}

static bool is_truthy(context_t& context, const object& obj)
{
	if (obj.is_type<list_t>())
	{
		auto&& list = obj.get_ref<list_t>();
		if (list.quoted)
			return true;
		else
			return is_truthy(context, context.evaluate_list(list));
	}

	if (obj.is_type<nil_t>())
		return false;
	if (obj.is_type<bool>())
		return obj.get_ref<bool>();
	if (obj.is_type<int64_t>())
		return obj.get_ref<int64_t>() != 0;
	if (obj.is_type<double>())
		return obj.get_ref<double>() != 0;
	if (obj.is_type<std::string>())
		throw std::runtime_error("Can't convert string to bool");

	throw std::runtime_error("Unknown type");
}

interpreter_t::interpreter_t()
	: global_context{ *this, variable_map_t{} }
{
	auto make_lambda = [&](const list_t& parameters, const list_t& body, const context_t& context)
	{
		std::vector<std::string> vec_params;
		for (auto&& param : parameters)
			vec_params.push_back(param.get_ref<std::string>());
		return lambda_t{ vec_params, body, context.get_lambda_context(vec_params, body) };
	};

	auto lambda = [&](const list_t& list, context_t& context) -> object
	{
		auto&& parameters = list[1].get_ref<list_t>();
		auto&& body = list[2].get_ref<list_t>();
		return make_lambda(parameters, body, context);
	};

	statements["lambda"] = lambda;


	auto def = [&](const list_t& list, context_t& context) -> object
	{
		if (list[1].is_type<list_t>())
		{
			auto&& list1 = list[1].get_ref<list_t>();
			auto&& name = list1[0].get_ref<std::string>();
			auto value = make_lambda(slice(list1, 1), list[2].get_ref<list_t>(), context);
			context.add_variable(name, std::make_shared<object>(value));
			return nil_t{};
		}

		auto&& name = list[1].get_ref<std::string>();
		auto value = context.evaluate_list(list[2]);
		context.add_variable(name, std::make_shared<object>(value));
		return nil_t{};
	};

	statements["def"] = def;
	statements["set"] = def;


	auto cond = [&](const list_t& list, context_t&) -> object
	{
		auto conditions = slice(list, 1);

		for (auto&& item : conditions)
		{
			auto&& list_item = item.get_ref<list_t>();
			if (is_truthy(global_context, global_context.evaluate_list(list_item[0])))
				return global_context.evaluate_list(list_item[1]);
		}

		return nil_t{};
	};

	statements["cond"] = cond;


	auto while_ = [&](const list_t& list, context_t& context) -> object
	{
		auto condition = list[1];

		while (is_truthy(context, context.evaluate_list(condition)))
			context.evaluate_list(slice(list, 2));

		return nil_t{};
	};

	statements["while"] = while_;


	auto vars = [&](const list_t&, context_t& context) -> object
	{
		for (auto&& pair : context.variable_map)
			std::cout << pair.first << " -> " << *pair.second << std::endl;

		return nil_t{};
	};

	statements["vars"] = vars;


	auto print = [&](const list_t& list, context_t& context) -> object
	{
		std::cout << context.evaluate_list(list[1]) << std::endl;

		return nil_t{};
	};

	statements["print"] = print;


	auto if_ = [&](const list_t& list, context_t& context) -> object
	{
		return is_truthy(context, context.evaluate_list(list[1])) ? context.evaluate_list(list[2]) : context.evaluate_list(list[3]);
	};

	statements["if"] = if_;

#define MAKE_OP_IMPL(op, name) \
	auto name = [&](const list_t& list, context_t& context) -> object \
	{ \
		return context.evaluate_list(list[1]) op context.evaluate_list(list[2]); \
	}; \
	global_context.add_variable(#op, std::make_shared<object>(builtin_func_t{ name }))
#define MAKE_OP(op) MAKE_OP_IMPL(op, TOKENIZE(oper, __COUNTER__))

	MAKE_OP(+);
	MAKE_OP(-);
	MAKE_OP(*);
	MAKE_OP(/);
	MAKE_OP(<);
	MAKE_OP(>);
	MAKE_OP(<=);
	MAKE_OP(>=);

#undef MAKE_OP
#undef MAKE_OP_IMPL

	global_context.add_variable("true", std::make_shared<object>(true));
	global_context.add_variable("false", std::make_shared<object>(false));
	global_context.add_variable("nil", std::make_shared<object>(nil_t{}));
}

void interpreter_t::interpret_line(const std::string & expr)
{
	auto val = global_context.evaluate_list(expand_list(get_abstract_syntax_tree(get_string_list(expr))));
	if (!val.is_type<nil_t>())
		std::cout << val << std::endl;
}
