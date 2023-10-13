#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "cmd.h"
#include "wad.h"

wad::lump::lump(std::string_view n, const std::byte* dat, std::size_t sz)
	: name_len{n.size()}
	, size{sz + (4 - sz % 4) % 4}
{
	if (name_len > 15) {
		std::ostringstream s;
		s << __FILE__ ":" << __func__ << ':' << __LINE__
		  << ": Lump name '" << n << "' has length " << name_len
		  << " > 15";
		throw std::length_error(s.str());
	} else if (sz > max_size) {
		std::ostringstream s;
		s << __FILE__ ":" << __func__ << ':' << __LINE__
		  << ": Tried to create a lump '" << n << "' of length " << sz
		  << " > " << max_size;
		throw std::length_error(s.str());
	}
	std::fill(std::copy(n.cbegin(), n.cend(), std::begin(name_)),
	          std::end(name_), 0);
	std::fill(std::copy(dat, dat + sz, std::begin(data)),
	          data + size, std::byte{0x00});
}

static std::ofstream::failure
write_failure(std::string_view name, const std::filesystem::path& file)
{
	std::ostringstream s;
	s << __FILE__ ":" << __func__ << ':' << __LINE__
	  << ": Could not write lump '" << name << "' into file :" << file;
	return std::ofstream::failure(s.str());
}

void wad::lump::write(const std::filesystem::path& path) const
{
	const std::filesystem::path expanded{expand(
		(path / name()).replace_extension("lmp")
	)};
	std::ofstream file(expanded, std::ios_base::binary);
	if (!file) {
		std::ostringstream s;
		s << __FILE__ ":" << __func__ << ':' << __LINE__
		  << ": Could not open file '" << expanded << '\'';
		throw std::ofstream::failure(s.str());
	}
	if (!file.write((const char*) data, size))
		throw write_failure(name(), expanded);
	file.close();
	if (!file)
		throw write_failure(name(), expanded);
}
