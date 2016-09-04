#include <string>
#include <vector>
#include <array>
#include <fstream>
#include <iostream>
#include "variant.h"
#include <memory>

template <typename T>
struct list_impl : std::vector<T>
{
	template <typename T>
	void visit(T&& fn)
	{
		for (auto&& item : *this)
			item.visit(fn);
	}

	template <typename T>
	void visit(T&& fn) const
	{
		for (auto&& item : *this)
			item.visit(fn);
	}

	bool quoted = false;
};

using object = recursive_variant<list_impl, int, float, std::string>;

using list = list_impl<object>;

std::ostream& operator <<(std::ostream& lhs, const list& rhs)
{
	rhs.visit([&](auto&& item) { lhs << item << " "; });
	return lhs;
}

std::array<int, 2> find_next_token(const std::string& expr, int pos)
{
	if (expr.empty())
		return{ -1, -1 };

	if (expr[pos] == ')')
		return{ -1, -1 };

	auto start = 0;

	if (expr[0] == '(')
	{
		start = pos + 1;
		while (expr[start] == ' ')
			++start;
	}

	if (expr[start] == ')')
		return{ -1, -1 };

	auto end = start + 1;

	while (end < expr.length() && expr[end] != ' ' && expr[end] != ')')
		++end;

	return{ start, end };
}

auto find_sexpr_end(const std::string& expr, int start_pos)
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

auto slice(const std::string& expr, int start, int end)
{
	return expr.substr(start, end - start);
}

list get_syntax_tree(const std::string& expr)
{
	list ret;

	auto cur_pos = 0;

	while (cur_pos < expr.length())
	{
		auto token = find_next_token(expr, cur_pos);

		if (token[0] == -1 || token[1] == -1)
			break;

		auto make_list = [&](bool is_code)
		{
			auto end = is_code ? find_sexpr_end(expr, token[0]) : token[0] + 1;
			auto sexpr_string = slice(expr, token[0], end);
			auto sexpr_list = get_syntax_tree(sexpr_string);
			sexpr_list.quoted = !is_code;
			ret.emplace_back(std::move(sexpr_list));
			cur_pos = end;
		};

		if (expr[token[0]] == '(')
			make_list(true);
		else if (expr[token[0]] == '\'' && expr[token[0] + 1] == '(')
			make_list(false);
		else
		{
			ret.emplace_back(slice(expr, token[0], token[1]));
			cur_pos = token[1];
		}
	}

	return std::move(ret);
}

int main(int argc, char** argv)
{
	std::ifstream file{ argv[1] };
	if (file.fail())
	{
		std::cout << "Failed to open file " << argv[1] << "!\n";
		std::cin.get();
	}
	std::string line;
	while (std::getline(file, line))
	{
		get_syntax_tree(line).visit([&](auto&& item) { std::cout << item << std::endl; });
	}
	std::cin.get();
}