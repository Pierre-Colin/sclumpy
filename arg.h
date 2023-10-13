#ifndef ARG_H
#define ARG_H

#include <string_view>

// Class version of POSIX getopt()
class argument_parser {
public:
	argument_parser() = delete;
	argument_parser(const argument_parser&) = delete;
	argument_parser& operator=(const argument_parser&) = delete;

	constexpr argument_parser(int c, char* const v[], const char* s)
		noexcept
		: argc{c}
		, argv{v}
		, optstring{s}
	{}

	[[nodiscard]] int operator()() noexcept;

	[[nodiscard]] constexpr std::string_view argument() const noexcept {
		return {optarg};
	}

	[[nodiscard]] constexpr char error() const noexcept { return optopt; }
	[[nodiscard]] constexpr int operand() const noexcept { return optind; }

private:
	const char* optarg = nullptr;
	int optind = 1;
	char optopt = 0;
	const char* argit = nullptr;

	const int argc;
	char* const * const argv;
	const char* const optstring;
};

#endif
