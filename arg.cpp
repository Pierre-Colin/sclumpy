#include <cstring>

#include "arg.h"

int argument_parser::operator()() noexcept
{
	const char* arg = argv[optind];
	if (arg == nullptr || arg[0] != '-' || arg[1] == 0)
		return -1;
	if (arg[1] == '-' && arg[2] == 0) {
		++optind;
		return -1;
	}
	if (argit == nullptr)
		argit = &arg[1];
	const char* const c = std::strchr(optstring, *argit);
	if (c == nullptr) {
		optopt = *argit;
		return '?';
	}
	if (c[1] == ':') {
		if (argit[1] == 0) {
			optarg = argv[++optind];
			++optind;
			argit = nullptr;
			if (optind > argc) {
				optopt = *c;
				return optstring[0] == ':' ? ':' : '?';
			}
		} else {
			optarg = argit + 1;
			++optind;
			argit = nullptr;
		}
		return *c;
	} else {
		if (argit[1] == 0) {
			++optind;
			argit = nullptr;
		} else {
			++argit;
		}
		return *c;
	}
}
