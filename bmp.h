#ifndef BMP_H
#define BMP_H

#include <cstdint>
#include <istream>

namespace bmp {

class file_header {
public:
	file_header() = delete;
	file_header(std::istream& file);

	[[nodiscard]]
	constexpr std::uint32_t size() const noexcept { return file_size; }

private:
	std::uint32_t file_size = 0;
	std::uint32_t offset = 0;
};

class info_header {
public:
	info_header() = delete;
	info_header(std::istream& file);

	[[nodiscard]]
	constexpr std::uint32_t colors() const noexcept { return biClrUsed; }

	[[nodiscard]]
	constexpr std::int32_t width() const noexcept { return biWidth; }

	[[nodiscard]]
	constexpr std::int32_t height() const noexcept { return biHeight; }

private:
	// Don't optimize those away before implementing LBM
	std::uint32_t biSize;
	std::int32_t  biWidth;
	std::int32_t  biHeight;
	std::int16_t  biPlanes;
	std::int16_t  biBitCount;
	std::uint32_t biCompression;
	std::uint32_t biSizeImage;
	std::int32_t  biXPelsPerMeter;
	std::int32_t  biYPelsPerMeter;
	std::uint32_t biClrUsed;
	std::uint32_t biClrImportant;
};

}

#endif
