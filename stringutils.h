#ifndef STRINGUTILS_H
#define STRINGUTILS_H

#include <string_view>

namespace util {

bool compare_nocase(const std::string_view &a, const std::string_view &b);

}

#endif
