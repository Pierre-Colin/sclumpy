#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "image.h"
#include "script.h"
#include "tokenizer.h"
#include "stringutils.h"
#include "wad.h"

using namespace std::literals;

namespace {

class lumpy_state {
public:
	lumpy_state(const std::filesystem::path&);
	lumpy_state(const lumpy_state&) = delete;
	lumpy_state& operator=(const lumpy_state&) = delete;

	lumpy_state(const std::filesystem::path& in,
	            const std::filesystem::path& out);

	~lumpy_state();
	void run();

private:
	[[nodiscard]] std::optional<std::string> read_next_token();
	[[nodiscard]] std::optional<std::string> include_and_read_next_token();
	void run_directive();
	void create_lump();
	script::syntax_error syntax_error(std::string_view msg) const;

	[[nodiscard]]
	bool has_path(const std::filesystem::path& p) const noexcept;

	template<class T> [[nodiscard]] T read_argument();

	[[nodiscard]] std::vector<std::variant<std::int32_t, float>>
	read_colormap2_arguments();

	template<int N>
	[[nodiscard]] std::vector<std::variant<std::int32_t, float>>
	read_integers();

	[[nodiscard]]
	std::vector<std::variant<std::int32_t, float>> read_font_arguments();

	bool singledest = false;
	unsigned int grabbed = 0;
	image img{};
	std::string directive{};
	std::filesystem::path output_path;
	std::vector<script_tokenizer> script_stack;
};

struct command {
	std::string_view name;

	std::vector<std::variant<int32_t, float>> (lumpy_state::*read_args)();

	image::lump_type
	(image::*function)(std::string_view,
			   const std::vector<image::argument_type>&);
};

lumpy_state::lumpy_state(const std::filesystem::path& out)
	: output_path{out}
	, script_stack(1)
{
	std::cout << "Running Lumpy script from standard input" << std::endl;
}

lumpy_state::lumpy_state(const std::filesystem::path& in,
                         const std::filesystem::path& out)
	: output_path{out}
	, script_stack{}
{
	script_stack.emplace_back(in);
	std::cout << "Running Lumpy script from file: " << in << std::endl;
}

lumpy_state::~lumpy_state()
{
	if (std::current_exception() != nullptr)
		return;
	if (singledest) {
		std::cout << grabbed << " lumps written separately"
		          << std::endl;
	} else {
		wad::write();
		std::cout << grabbed << " lumps placed into WAD file: "
		          << output_path << std::endl;
	}
}

void lumpy_state::run()
{
	while (std::optional<std::string> tok = read_next_token()) {
		directive = *std::move(tok);
		run_directive();
	}
}

std::optional<std::string> lumpy_state::include_and_read_next_token()
{
	auto& scr = script_stack.back();
	auto t = scr.read_token();
	if (t.empty())
		throw scr.make_syntax_error("Missing $include operand"sv);
	if (script_stack.size() >= 24)
		throw scr.make_syntax_error("Script stack is full"sv);
	const std::filesystem::path path{t};
	if (has_path(path))
		throw scr.make_syntax_error("Cyclical script inclusions"sv);
	script_stack.emplace_back(path);
	return read_next_token();
}

std::optional<std::string> lumpy_state::read_next_token()
{
	if (script_stack.empty())
		throw std::logic_error("Script stack is empty");
	auto t = script_stack.back().read_token();
	if (t.empty()) {
		script_stack.pop_back();
		return script_stack.empty() ? std::nullopt : read_next_token();
	}
	return !util::compare_nocase(t, "$include"sv) ? t :
	       include_and_read_next_token();
}

script::syntax_error lumpy_state::syntax_error(std::string_view msg) const
{
	return script_stack.back().make_syntax_error(msg);
}

bool lumpy_state::has_path(const std::filesystem::path& p) const noexcept
{
	const auto f = [&p](const script_tokenizer& t) {
		return t.has_path(p);
	};
	return std::find_if(script_stack.cbegin(), script_stack.cend(), f)
		!= script_stack.cend();
}

void lumpy_state::run_directive()
{
	using namespace std::literals;

	if (util::compare_nocase(directive, "$dest"sv)) {
		if (singledest)
			throw syntax_error("Read $dest after $singledest"sv);
		std::optional<std::string> tok = read_next_token();
		if (!tok)
			throw syntax_error("Missing file path after $dest"sv);
		output_path = *std::move(tok);
	} else if (util::compare_nocase(directive, "$load"sv)) {
		std::optional<std::string> tok = read_next_token();
		if (!tok)
			throw syntax_error("Missing file path after $load"sv);
		img = image(*tok, image::load_type::lbm);
	} else if (util::compare_nocase(directive, "$loadbmp"sv)) {
		std::optional<std::string> tok = read_next_token();
		if (!tok)
			throw syntax_error("Missing path after $loadbmp"sv);
		img = image(*tok, image::load_type::bmp);
	} else if (util::compare_nocase(directive, "$singledest"sv)) {
		std::optional<std::string> tok = read_next_token();
		if (!tok)
			throw syntax_error("Missing path after $singledest"sv);
		output_path = *std::move(tok);
		singledest = true;
	} else {
		create_lump();
	}
}

void lumpy_state::create_lump()
{
	using namespace std::literals;

	static constexpr std::array<command, 7> commands {
		command { "palette"sv,
		          &lumpy_state::read_integers<2>,
		          &image::grab_palette }, // arguments optional

		command { "colormap"sv,
		          &lumpy_state::read_integers<2>,
		          &image::grab_colormap },

		command { "qpic"sv,
		          &lumpy_state::read_integers<4>,
		          &image::grab_qpic },

		command { "miptex"sv,
		          &lumpy_state::read_integers<4>,
		          &image::grab_miptex },

		command { "raw"sv,
		          &lumpy_state::read_integers<4>,
		          &image::grab_raw },

		command { "colormap2"sv,
		          &lumpy_state::read_colormap2_arguments,
		          &image::grab_colormap2 },

		command { "font"sv,
		          &lumpy_state::read_font_arguments,
		          &image::grab_font },
	};
	static_assert(wad::type_lumpy + commands.size() <= 127);

	static constexpr auto find_type = [](std::string_view token) {
		auto p = [token](const command& c){ return token == c.name; };
		return std::find_if(commands.cbegin(), commands.cend(), p);
	};

	std::optional<std::string> token = read_next_token();
	if (!token)
		throw syntax_error("Expected lump type specifier"sv);
	const auto it = find_type(*token);
	if (it == commands.cend())
		throw syntax_error("Unknown lump type: "s + *token);
	image::lump_type data;
	try {
		data = (img.*it->function)(directive,
		                           (this->*it->read_args)());
		++grabbed;
	} catch (const std::exception& e) {
		std::ostringstream s;
		s << __FILE__ ":" << __func__ << ':' << __LINE__
		  << ": Could not create lump '" << directive << "'\n"
		  << e.what();
		throw std::runtime_error(s.str());
	}
	const wad::lump l(directive, data.data(), data.size());
	if (singledest) {
		l.write(output_path);
	} else {
		const auto d = it - commands.cbegin();
		const char type = static_cast<char>(wad::type_lumpy + d);
		wad::add(output_path, l, type);
	}
}

template<class T>
T lumpy_state::read_argument()
{
	using namespace std::literals;
	std::optional<std::string> token = read_next_token();
	if (!token)
		throw syntax_error("Expected a lump type argument"sv);
	std::istringstream s(*token);
	T arg;
	s >> arg;
	if (!s.eof())
		throw syntax_error("Invalid lump argument type"sv);
	return arg;
}

std::vector<std::variant<std::int32_t, float>>
lumpy_state::read_colormap2_arguments()
{
	std::vector<std::variant<std::int32_t, float>> args;
	args.reserve(3);
	args.push_back(read_argument<float>());
	for (int i = 1; i < 3; ++i)
		args.push_back(read_argument<std::int32_t>());
	return args;
}

template<int N>
std::vector<std::variant<std::int32_t, float>>
lumpy_state::read_integers()
{
	std::vector<std::variant<std::int32_t, float>> args;
	args.reserve(N);
	for (int i = 0; i < N; ++i)
		args.push_back(read_argument<std::int32_t>());
	return args;
}

std::vector<std::variant<std::int32_t, float>>
lumpy_state::read_font_arguments()
{
	std::vector<std::variant<std::int32_t, float>> args;
	for (;;) {
		const std::int32_t arg = read_argument<std::int32_t>();
		args.push_back(arg);
		if (arg == -1 && args.size() % 5 == 1)
			return args;
	}
}

[[nodiscard]] std::string make_syntax_error(const char* file, const char* func,
                                            unsigned int line, const char* msg)
{
	std::ostringstream s;
	s << file << ':' << func << ':' << line << ": " << msg;
	return s.str();
}

}

script::syntax_error::syntax_error(const char* file, const char* func,
                                   unsigned int line, const char* msg)
	: std::invalid_argument(make_syntax_error(file, func, line, msg))
{}

void script::run_from_path(const std::filesystem::path& in,
                           const std::filesystem::path& out)
{
	lumpy_state(in, out).run();
}

void script::run_from_stdin(const std::filesystem::path& out)
{
	lumpy_state(out).run();
}
