#include <algorithm>
#include <locale>
#include <stdexcept>
#include <sstream>
#include <string_view>

#include "stringutils.h"

template<class charT>
bool comp_nc(const charT a, const charT b, const std::locale& loc) noexcept
{
	return std::tolower(a, loc) == std::tolower(b, loc);
}

bool util::compare_nocase(const std::string_view& a, const std::string_view& b)
{
	const std::locale loc;
	if (!std::has_facet<std::ctype<char>>(loc)) {
		std::ostringstream s;
		s << __FILE__ ":" << __func__ << ':' << __LINE__ << ": Locale "
		  << loc.name() << " cannot convert characters";
		throw std::logic_error(s.str());
	}
	auto cmp = [&loc](char x, char y){ return comp_nc(x, y, loc); };
	return std::equal(a.cbegin(), a.cend(), b.cbegin(), cmp);
}
