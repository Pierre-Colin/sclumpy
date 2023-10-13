#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <locale>
#include <utility>
#include <vector>

#include "byte.h"
#include "cmd.h"
#include "wad.h"

using lim = std::numeric_limits<std::int32_t>;

namespace {

class wad_info {
public:
	wad_info(std::int32_t nl, std::int32_t ito)
		: number_lumps(nl)
		, info_table_offset(ito)
	{}

	template<class It>
	It write(It it, bool wad3, bool big_end) const {
		static const auto tr = [](const char ch) {
			return std::byte{static_cast<unsigned char>(ch)};
		};

		const char magic_number[] = {'W', 'A', 'D', wad3 ? '3' : '2'};
		const auto f = big_end ? &put_big_endian<It, std::int32_t> :
		                         &put_little_endian<It, std::int32_t>;
		it = std::transform(std::cbegin(magic_number),
		                    std::cend(magic_number), it, tr);
		it = f(it, number_lumps);
		return f(it, info_table_offset);
	}

private:
	const std::int32_t number_lumps;
	const std::int32_t info_table_offset;
};

class lump_info {
public:
	lump_info(const char n[16], std::int32_t fp, std::int32_t len, char t)
		: filepos(fp)
		, disksize(len)
		, size(disksize)
		, type(t)
		, name{0}
	{
		const std::locale loc;
		for (int i = 0; i < 15 && n[i]; i++)
			name[i] = std::toupper(n[i], loc);
	}

	template<class It>
	It write(It it, bool big_end) const {
		static const auto tr = [](const char ch) {
			return std::byte{static_cast<unsigned char>(ch)};
		};

		const auto f = big_end ? &put_big_endian<It, std::int32_t> :
		                         &put_little_endian<It, std::int32_t>;
		it = f(it, filepos);
		it = f(it, disksize);
		it = f(it, size);
		*it++ = std::byte{static_cast<unsigned char>(type)};
		*it++ = std::byte{0x00};
		*it++ = std::byte{0x00};
		*it++ = std::byte{0x00};
		return std::transform(std::cbegin(name), std::end(name), it,
		                      tr);
	}

private:
	const std::int32_t filepos;
	const std::int32_t disksize;
	const std::int32_t size;
	const char type;
	char name[16];
};

bool big_endian;
std::filesystem::path output_path;
std::vector<std::byte> output_buffer;
std::vector<lump_info> outinfo;

void make(const std::filesystem::path &pathname, bool big_end = false)
{
	static const auto wadinfo_size = 12;
	output_path = pathname;
	output_buffer.resize(wadinfo_size, std::byte{0});
	outinfo.clear();
	big_endian = big_end;
}

}

void wad::add(const std::filesystem::path& path, const wad::lump& l, char type)
{
	static bool output_created = false;
	if (!std::exchange(output_created, true))
		make(path);

	if (outinfo.size() >= 4096) {
		std::ostringstream s;
		s << __FILE__ ":" << __func__ << ':' << __LINE__
		  << ": Cannot fit more than 4096 lumps in WAD file";
		throw std::length_error(s.str());
	}
	outinfo.emplace_back(l.name_, output_buffer.size(), l.size, type);
	try {
		if (output_buffer.size() > lim::max() - l.size) {
			std::ostringstream s;
			s << __FILE__ ":" << __func__ << ':' << __LINE__
			  << ": WAD file is too big";
			throw std::length_error(s.str());
		}
		std::copy(l.begin(), l.end(),
		          std::back_inserter(output_buffer));
	} catch (...) {
		outinfo.pop_back();
		throw;
	}
}

void wad::write()
{
	const auto offset = output_buffer.size();

	{
		auto it = std::back_inserter(output_buffer);
		for (const lump_info &x : outinfo)
			it = x.write(it, big_endian);
	}
		
	if (offset > lim::max()) {
		std::ostringstream s;
		s << __FILE__ ":" << __func__ << ':' << __LINE__
		  << ": WAD file exceeded " << lim::max() << " bytes";
		throw std::length_error(s.str());
	}

	const wad_info header(static_cast<std::int32_t>(outinfo.size()),
	                      static_cast<std::int32_t>(offset));
	header.write(output_buffer.begin(), check_wad3(), big_endian);
		
	const auto ptr = reinterpret_cast<const char*>(output_buffer.data());
	std::ofstream outwad(output_path, std::ios::binary);
	if (!outwad.write(ptr, output_buffer.size())) {
		std::ostringstream s;
		s << __FILE__ ":" << __func__ << ':' << __LINE__
		  << ": Could not write WAD file";
		throw std::ofstream::failure(s.str());
	}
	outwad.close();
}
