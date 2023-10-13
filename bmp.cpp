#include <algorithm>
#include <climits>
#include <cstdint>
#include <istream>
#include <limits>
#include <sstream>
#include <stdexcept>

#include "bmp.h"

namespace {

template<class N>
[[nodiscard]] N read_little_endian(std::istream& f)
{
	unsigned char buf[sizeof (N)];
	if (!f.read(reinterpret_cast<char*>(buf), sizeof (N))) {
		std::ostringstream s;
		s << __FILE__ ":" << __func__ << ':' << __LINE__
		  << ": Error when reading " << sizeof (N) << " bytes";
		throw std::istream::failure(s.str());
	}
	N acc = 0;
	for (unsigned b = 0; b < sizeof (N); ++b)
		acc |= static_cast<N>(buf[b]) << (CHAR_BIT * b);
	return acc;
}

}

bmp::file_header::file_header(std::istream& file)
{
	char buffer[4];
	if (!file.read(buffer, 2))
		throw std::runtime_error("Could not read BMP magic number");
	if (!std::equal(buffer, buffer + 2, "BM"))
		throw std::invalid_argument("Invalid BMP magic number");
	file_size = read_little_endian<std::uint32_t>(file);
	if (file_size < 14) {
		std::ostringstream s;
		s << "File size specified in header (" << file_size
		  << ") is too small for a BMP file";
		throw std::invalid_argument(s.str());
	}
	if (!file.read(buffer, 4))
		throw std::runtime_error("Could not read reserved fields");
	offset = read_little_endian<std::uint32_t>(file);
	if (offset >= file_size) {
		std::ostringstream s;
		s << "Image data offset (" << offset
		  << ") is greater than file size (" << file_size << ')';
		throw std::invalid_argument(s.str());
	}
}

bmp::info_header::info_header(std::istream& file)
	: biSize{read_little_endian<std::uint32_t>(file)}
	, biWidth{read_little_endian<std::int32_t>(file)}
	, biHeight{read_little_endian<std::int32_t>(file)}
	, biPlanes{read_little_endian<std::int16_t>(file)}
	, biBitCount{read_little_endian<std::int16_t>(file)}
	, biCompression{read_little_endian<std::uint32_t>(file)}
	, biSizeImage{read_little_endian<std::uint32_t>(file)}
	, biXPelsPerMeter{read_little_endian<std::int32_t>(file)}
	, biYPelsPerMeter{read_little_endian<std::int32_t>(file)}
	, biClrUsed{read_little_endian<std::uint32_t>(file)}
	, biClrImportant{read_little_endian<std::uint32_t>(file)}
{
	static constexpr std::uint32_t bi_rgb = 0x00000000;
	if (biSize > 40)
		file.seekg(biSize - 40, std::ios_base::cur);
	if (biWidth < 0) {
		std::ostringstream s;
		s << "Image width (" << biWidth << ") is negative";
		throw std::invalid_argument(s.str());
	}
	if (biHeight < 0) {
		std::ostringstream s;
		s << "Image height (" << biHeight << ") is negative";
		throw std::invalid_argument(s.str());
	}
	if (biPlanes != 1) {
		std::ostringstream s;
		s << "Expected 1 plane, got " << biPlanes;
		throw std::invalid_argument(s.str());
	}
	if (biBitCount != 8) {
		std::ostringstream s;
		s << "Expected 8 bits, got " << biBitCount;
		throw std::invalid_argument(s.str()); }
	if (biCompression != bi_rgb)
		throw std::invalid_argument("Invalid BMP compression type");

	// Treat default value for palette size
	if (biClrUsed == 0)
		biClrUsed = 256;
}
