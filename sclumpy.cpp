#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <locale>
#include <stdexcept>

#include "arg.h"
#include "cmd.h"
#include "script.h"
#include "spray.h"

using namespace std::literals;

namespace {

class inconsistent_option : public std::invalid_argument {
public:
	inconsistent_option(const char c) : std::invalid_argument(make(c)) {}

private:
	static std::string make(const char c) {
		std::ostringstream s;
		s << "Option "sv << c << " contradicts previous options"sv;
		return s.str();
	}
};

class missing_operand : public std::invalid_argument {
public:
	missing_operand(const char c) : std::invalid_argument(make(c)) {}

private:
	static std::string make(const char c) {
		std::ostringstream s;
		s << "Option "sv << c << " has no operand"sv;
		return s.str();
	}
};

class unknown_option : public std::invalid_argument {
public:
	unknown_option(const char c) : std::invalid_argument(make(c)) {}

private:
	static std::string make(const char c) {
		std::ostringstream s;
		s << "Option "sv << c << " is unknown"sv;
		return s.str();
	}
};

class bad_operand_number : public std::invalid_argument {
public:
	bad_operand_number(const int c) : std::invalid_argument(make(c)) {}

private:
	static std::string make(const int n) {
		std::ostringstream s;
		s << "Invalid operand number: "sv << n;
		return s.str();
	}
};

}

static std::filesystem::path default_output(const std::filesystem::path& path)
{
	return std::filesystem::path(path).replace_extension("wad");
}

static void parse_arguments_and_run(const int argc, char* const argv[])
{
	argument_parser arg(argc, argv, ":8sp:");
	std::filesystem::path project;
	int c;
	bool lumpy = false, do_spray = false;
	while ((c = arg()) >= 0) {
		switch (c) {
		case '8':
			if (do_spray)
				throw inconsistent_option('8');
			plan_wad2();
			lumpy = true;
			break;
		case 's':
			if (lumpy)
				throw inconsistent_option('s');
			do_spray = true;
			break;
		case 'p':
			if (do_spray)
				throw inconsistent_option('p');
			project = arg.argument();
			lumpy = true;
			break;
		case ':':
			throw missing_operand(arg.error());
		default:
			throw unknown_option(arg.error());
		}
	}
	const int num_op = argc - arg.operand();
	if (do_spray) {
		if (num_op != 1)
			throw bad_operand_number(num_op);
		run_spray(argv[argc - 1]);
	} else {
		if (num_op > 1)
			throw bad_operand_number(num_op);
		const char* operand = argv[argc - 1];
		if (num_op == 0 || std::strcmp(operand, "-") == 0) {
			script::run_from_stdin("out.wad");
		} else {
			set_working_directory(std::move(project));
			std::filesystem::path out = default_output(operand);
			script::run_from_path(operand, out);
		}
	}
}

static std::locale set_locale_facets(std::locale loc)
{
	if (const char* const var = std::getenv("LC_CTYPE")) {
		if (var[0] != 0)
			loc = loc.combine<std::ctype<char>>(std::locale(var));
	}
	if (const char* const var = std::getenv("LC_NUMERIC")) {
		if (var[0] != 0) {
			const std::locale num(var);
			loc = loc.combine<std::num_get<char>>(num);
			loc = loc.combine<std::num_put<char>>(num);
			loc = loc.combine<std::numpunct<char>>(num);
		}
	}
	return loc;
}

static std::locale get_default_locale(const char* const lang)
{
	return lang && lang[0] ? std::locale(lang) : std::locale();
}

static void set_locale()
{
	if (const char* const var = std::getenv("LC_ALL")) {
		if (var[0] != 0) {
			std::locale::global(std::locale(var));
			return;
		}
	}
	const char* const lang = std::getenv("LANG");
	std::locale::global(set_locale_facets(get_default_locale(lang)));
}

int main(int argc, char *argv[])
{
	try {
		set_locale();
		parse_arguments_and_run(argc, argv);
		return EXIT_SUCCESS;
	} catch (const std::exception& e) {
		std::cerr << "\nError:\n" << e.what() << std::endl;
		return EXIT_FAILURE;
	}
}
