#include "interpreter.h"
#include <iostream>

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

	while (end < expr.length() && expr[end] != ' ' && expr[end] != ')')
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
				ret_list.emplace_back(std::string{ slice(str, 1, -3) });
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
	//list_t ret_list;

	if (list[0].is_type<statement>())
	{
		//std::cout << "Expanding statement " << list[0].get_ref<statement>() << std::endl;
		auto stmt = list[0].get_ref<statement>();
		auto it = statements.find(stmt);
		if (it == statements.end())
			throw std::runtime_error{ std::string{ "Unknown statement " } +stmt };
		auto ret = it->second(list, variables);
		if (ret.is_type<list_t>())
			return expand_list(ret.get_ref<list_t>());
		else
			return ret;
	}

	return list;
}

object interpreter_t::evaluate_list(const object& obj, variable_map& variables)
{
	if (!obj.is_type<list_t>())
	{
		if (obj.is_type<variable_reference>())
		{
			auto&& var = obj.get_ref<variable_reference>();
			return variables.at(var);
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
		new_list.emplace_back(variables.at(var));
		new_list.insert(new_list.end(), list.begin() + 1, list.end());
		return evaluate_list(new_list, variables);
	}

	if (list[0].is_type<statement>())
	{
		auto stmt = list[0].get_ref<statement>();
		auto it = statements.find(stmt);
		if (it == statements.end())
			throw std::runtime_error{ std::string{ "Unknown statement " } +stmt };
		return it->second(list, variables);
	}

	if (list[0].is_type<lambda_t>())
	{
		auto&& lambda = list[0].get_ref<lambda_t>();
		auto new_variables = variables;

		int i = 1;
		for (auto&& param : lambda.parameters)
		{
			auto pair = new_variables.emplace(std::make_pair(param, evaluate_list(list[i])));
			++i;
		}

		return evaluate_list(lambda.body, new_variables);
	}

	if (list[0].is_type<builtin_func_t>())
	{
		auto&& func = list[0].get_ref<builtin_func_t>();

		return evaluate_list(func(list));
	}

	list_t ret_list;

	for (auto&& item : list)
		if (item.is_type<list_t>())
			ret_list.emplace_back(evaluate_list(item.get_ref<list_t>(), variables));

	return ret_list;
}

object interpreter_t::evaluate_list(const object & list)
{
	return evaluate_list(list, variables);
}

static bool is_truthy(interpreter_t& interpreter, const object& obj)
{
	if (obj.is_type<list_t>())
	{
		auto&& list = obj.get_ref<list_t>();
		if (list.quoted)
			return true;
		else
			return is_truthy(interpreter, interpreter.evaluate_list(list));
	}

	if (obj.is_type<nil_t>())
		return false;
	if (obj.is_type<int64_t>())
		return obj.get_ref<int64_t>() != 0;
	if (obj.is_type<double>())
		return obj.get_ref<double>() != 0;
	if (obj.is_type<std::string>())
		throw std::runtime_error("Can't convert string to bool");

	throw std::runtime_error("Unknown type");
}

interpreter_t::interpreter_t()
{
	auto def = [&](auto&& list, auto&& variables) -> object
	{
		auto variable_name = list[1].template get_ref<std::string>();
		auto value = evaluate_list(list[2]);

		auto it = variables.find(variable_name);
		if (it == variables.end())
			variables.emplace(std::make_pair(variable_name, value));
		else
			it->second = value;
		return nil_t{};
	};

	statements["def"] = def;


	auto cond = [&](auto&& list, auto&&) -> object
	{
		auto conditions = slice(list, 1);

		for (auto&& item : conditions)
		{
			auto&& list_item = item.template get_ref<list_t>();
			if (is_truthy(*this, evaluate_list(list_item[0])))
				return evaluate_list(list_item[1]);
		}

		return nil_t{};
	};

	statements["cond"] = cond;


	auto vars = [&](auto&&, auto&& variables) -> object
	{
		for (auto&& pair : variables)
			std::cout << pair.first << " -> " << pair.second << std::endl;

		return nil_t{};
	};

	statements["vars"] = vars;


	auto print = [&](auto&& list, auto&& variables) -> object
	{
		auto variable_name = list[1].template get_ref<std::string>();
		auto it = variables.find(variable_name);
		if (it == variables.end())
			throw std::runtime_error{ std::string{ "Undefined variable " } +variable_name };

		std::cout << "Print: " << it->second << std::endl;

		return nil_t{};
	};

	statements["print"] = print;


	auto lambda = [&](auto&& list, auto&&) -> object
	{
		auto parameters = list[1].template get_ref<list_t>();
		std::vector<std::string> vec_params;
		for (auto&& param : parameters)
			vec_params.push_back(param.template get_ref<std::string>());
		auto&& body = list[2].template get_ref<list_t>();
		return lambda_t{ vec_params, body };
	};

	statements["lambda"] = lambda;

	auto plus = [&](auto&& list) -> object
	{
		return evaluate_list(list[1]) + evaluate_list(list[2]);
	};

	variables.emplace(std::make_pair("+", builtin_func_t{plus}));
}

void interpreter_t::interpret_line(const std::string & expr)
{
	std::cout << evaluate_list(expand_list(get_abstract_syntax_tree(get_string_list(expr)))) << std::endl;
}