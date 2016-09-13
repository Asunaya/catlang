#include <string>
#include <vector>
#include <array>
#include <fstream>
#include <iostream>
#include "variant.h"
#include <memory>
#include "basic_types.h"
#include "interpreter.h"

static auto interpret_lines(interpreter_t& interpreter, std::istream& stream)
{
	std::string line;
	while (std::getline(stream, line))
		interpreter.interpret_line(line);
}

static auto interpret_file(interpreter_t& interpreter, const char* filename)
{
	std::ifstream file{ filename };
	if (file.fail())
	{
		std::cout << "Failed to open file " << filename << "!\n";
		std::cin.get();
		return -1;
	}
	interpret_lines(interpreter, file);
	std::cin.get();
	return 0;
}

int main(int argc, char** argv)
try
{
	interpreter_t interpreter;

	if (argc > 1)
		return interpret_file(interpreter, argv[1]);

	// REPL mode
	interpret_lines(interpreter, std::cin);
	return 0;
}
catch (std::runtime_error& e)
{
	std::cout << "e.what() = " << e.what() << std::endl;
	std::cin.get();
	return -1;
}
