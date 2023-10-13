#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "script.h"
#include "tokenizer.h"

using traits_type = std::char_traits<char>;

script_tokenizer::script_tokenizer(const std::filesystem::path& p)
	: info(std::in_place, file_info{p, std::ifstream(p)})
{
	if (info->file.fail()) {
		std::ostringstream s;
		s << __FILE__ ":" << __func__ << ':' << __LINE__
		  << ": Could not open lump script file " << p;
		throw std::ifstream::failure(s.str());
	}
}

script_tokenizer::~script_tokenizer() noexcept
{
	using namespace std;
	const auto ex = cout.exceptions();
	cout.exceptions(ios_base::goodbit);
	if (info) {
		if (current_exception() == nullptr)
			cout << "Finished script file: " << info->path << endl;
	} else {
		if (current_exception() == nullptr)
			cout << "Finished standard-input script" << endl;
	}
	cout.exceptions(ex);
}

[[nodiscard]]
static int get_processed_char(std::istream& stream, std::uintmax_t& line)
{
	int ch = stream.get();
	if (ch == '\n')
		++line;
	if (ch != '\\')
		return ch;
	if (stream.peek() == '\n') {
		stream.get();
		++line;
		return stream.get();
	}
	return '\\';
}

std::string script_tokenizer::read_quoted_token()
{
	std::istream& stream = info ? info->file : std::cin;
	std::string token;
	for (;;) {
		const int c = get_processed_char(stream, line);
		if (c == traits_type::eof()) {
			std::ostringstream s;
			s << __FILE__ ":" << __func__ << ':'
			  << __LINE__;
			if (info)
				s << ": Script file " << info->path;
			else
				s << ": Standard-input script";
			s << " ended prematurely on line " << line;
			throw std::ifstream::failure(s.str());
		} else if (c == '"') {
			return token;
		} else {
			token.push_back(traits_type::to_char_type(c));
		}
	}
}

std::string script_tokenizer::read_token()
{
	const std::locale loc;
	if (!std::has_facet<std::ctype<char>>(loc)) {
		std::ostringstream s;
		s << __FILE__ ":" << __func__ << ':' << __LINE__ << ": Locale "
		  << loc.name()
		  << " cannot classify characters found in ";
		if (info)
			s << "script file: " << info->path;
		else
			s << "standard input";
		throw std::logic_error(s.str());
	}
	std::istream& stream = info ? info->file : std::cin;
	std::string token;
	// Mealy-machine design
	int c;
start:
	do
		c = get_processed_char(stream, line);
	while (c != traits_type::eof()
	       && std::isspace(traits_type::to_char_type(c), loc));
	switch (c) {
	case traits_type::eof():
		return token;
	case '/':
		goto slash;
	case ';':
	case '#':
		goto comment;
	case '"':
		return read_quoted_token();
	default:
		token.push_back(traits_type::to_char_type(c));
		goto in_token;
	}
comment:
	do
		c = get_processed_char(stream, line);
	while (c != traits_type::eof() && c != '\n');
	if (c == '\n' && token.empty())
		goto start;
	return token;
slash:
	c = get_processed_char(stream, line);
	if (c == '/')
		goto comment;
	token.push_back(traits_type::to_char_type(c));
	if (c == ';' || c == '#')
		goto comment;
	// FALLTHROUGH
in_token:
	for (;;) {
		c = get_processed_char(stream, line);
		if (c == traits_type::eof()
		    || std::isspace(traits_type::to_char_type(c), loc))
			return token;
		if (c == ';' || c == '#')
			goto comment;
		if (c == '/')
			goto slash;
		token.push_back(traits_type::to_char_type(c));
	}
}

bool script_tokenizer::eof() const
{
	return info ? info->file.eof() : std::cin.eof();
}

script::syntax_error
script_tokenizer::make_syntax_error(std::string_view msg) const
{
	std::ostringstream s;
	if (info)
		s << "Syntax error in script file " << info->path;
	else
		s << "Syntax error in standard-input script";
	s << " on line " << line << ": " << msg;
	return script::syntax_error(__FILE__, __func__, __LINE__,
	                            s.str().c_str());
}
