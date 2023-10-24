#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "bmp.h"
#include "byte.h"
#include "cmd.h"
#include "image.h"

image::image(image&& other) noexcept
	: data{std::exchange(other.data, nullptr)}
	, width{other.width}
	, height{other.height}
	, transparent{other.transparent}
{
	std::copy(std::begin(other.palette), std::end(other.palette),
	          std::begin(palette));
}

image& image::operator=(image&& other) noexcept
{
	std::copy(std::begin(other.palette), std::end(other.palette),
	          std::begin(palette));
	if (data)
		delete[] data;
	data = std::exchange(other.data, nullptr);
	width = other.width;
	height = other.height;
	transparent = other.transparent;
	return *this;
}

image::image(const std::filesystem::path& path, image::load_type mode)
	: image()
{
	switch (mode) {
	default:
		throw std::invalid_argument("Unknown image loading mode");
	case image::load_type::bmp:
		load_bmp(path);
		break;
	case image::load_type::lbm:
		load_lbm(path);
	}
}

void image::permute(const std::byte table[256]) noexcept
{
	if (!data)
		return;
	auto tr = [table](std::byte& b) {
		return table[std::to_integer<unsigned char>(b)];
	};
	std::transform(data, data + width * height, data, tr);
}

[[nodiscard]] static std::array<std::byte, 768>
read_palette_data(std::istream& file, const unsigned int num_colors)
{
	if (num_colors > 256) {
		std::ostringstream s;
		s << __FILE__ ":" << __func__ << ':' << __LINE__
		  << ": Number of colors (" << num_colors << ") too large";
		throw std::range_error(s.str());
	}
	std::array<std::byte, 768> palette;
	for (unsigned int c = 0; c < num_colors; ++c) {
		std::byte* const b = &palette[3 * c];
		if (!file.read(reinterpret_cast<char*>(b), 3)) {
			std::ostringstream s;
			s << __FILE__ ":" << __func__ << ':' << __LINE__
			  << ": Could not read color " << c;
			throw std::istream::failure(s.str());
		}
		std::swap(b[0], b[2]);
		if (file.get() == std::istream::traits_type::eof()) {
			std::ostringstream s;
			s << __FILE__ ":" << __func__ << ':' << __LINE__
			  << ": Error after reading color " << c;
			throw std::istream::failure(s.str());
		}
	}
	return palette;
}

[[nodiscard]] static std::unique_ptr<std::byte[]>
read_bitmap_data(std::istream& file, const bmp::file_header& fh,
                 const bmp::info_header& ih)
{
	const auto pos = file.tellg();
	if (pos > std::numeric_limits<std::uint32_t>::max()) {
		std::ostringstream s;
		s << __FILE__ ":" << __func__ << ':' << __LINE__
		  << ": Bitmap file size exceeds "
		  << std::numeric_limits<std::uint32_t>::max();
		throw std::range_error(s.str());
	}
	const std::uint32_t dsz = fh.size() - static_cast<std::uint32_t>(pos);
	auto buf = std::make_unique<std::byte[]>(dsz);
	if (!file.read(reinterpret_cast<char*>(buf.get()), dsz)) {
		std::ostringstream s;
		s << __FILE__ ":" << __func__ << ':' << __LINE__
		  << ": Could not read bitmap data";
		throw std::istream::failure(s.str());
	}

	auto inv_buf = std::make_unique<std::byte[]>(dsz);
	const std::uint32_t width_ru = ih.width() + (4 - ih.width() % 4) % 4;
	for (std::int32_t y = 0; y < ih.height(); ++y) {
		std::copy(&buf[(ih.height() - y - 1) * width_ru],
		          &buf[(ih.height() - y) * width_ru],
		          &inv_buf[y * width_ru]);
	}

	return inv_buf;
}

void image::load_bmp(const std::filesystem::path& path)
{
	const std::filesystem::path exp = expand(path);
	std::ifstream file(exp, std::ios::binary);
	if (!file) {
		std::ostringstream s;
		s << __FILE__ ":" << __func__ << ':' << __LINE__
		  << ": Could not open bitmap file: " << exp;
		throw std::ifstream::failure(s.str());
	}
	const bmp::file_header fh(file);
	const bmp::info_header ih(file);
	{
		const auto pal = read_palette_data(file, ih.colors());
		std::copy(pal.cbegin(), pal.cend(), std::begin(palette));
	}
	data = read_bitmap_data(file, fh, ih).release();
	file.close();
	width = ih.width();
	height = ih.height();
	if (width > std::numeric_limits<std::int16_t>::max()) {
		std::ostringstream s;
		s << __FILE__ ":" << __func__ << ':' << __LINE__
		  << ": Image width (" << width
		  << ") is too big, maximum supported is "
		  << std::numeric_limits<std::int16_t>::max();
		throw std::range_error(s.str());
	} else if (height > std::numeric_limits<std::int16_t>::max()) {
		std::ostringstream s;
		s << __FILE__ ":" << __func__ << ':' << __LINE__
		  << ": Image height (" << width
		  << ") is too big, maximum supported is "
		  << std::numeric_limits<std::int16_t>::max();
		throw std::range_error(s.str());
	}
	if (path.stem().c_str()[0] == '{')
		make_transparent();
}

void image::load_lbm([[maybe_unused]] const std::filesystem::path& path)
{
	throw std::logic_error("LBM loading not implemented");
}

static constexpr std::array<std::byte, 3> transparent_pixel{
	std::byte{0x00}, std::byte{0x00}, std::byte{0xff}
};

static bool is_transparent_color(const std::byte* pixel) noexcept
{
	return std::equal(transparent_pixel.cbegin(), transparent_pixel.cend(),
	                  pixel);
}

void image::make_transparent() noexcept
{
	if (std::exchange(transparent, true))
		return;
	std::array<std::byte, 256> permutation;
	std::optional<unsigned char> first_transparent;

	for (unsigned int c = 0; c < std::size(permutation); ++c) {
		if (is_transparent_color(std::begin(palette) + 3 * c)) {
			permutation[c] = std::byte{255};
			if (!first_transparent)
				first_transparent = c;
		} else {
			using std::byte;
			permutation[c] = byte{static_cast<unsigned char>(c)};
		}
	}

	if (!first_transparent)
		return;

	if (!is_transparent_color(std::end(palette) - 3))
		permutation.back() = std::byte{*first_transparent};
	permute(permutation.data());
	std::copy_n(std::end(palette) - 3, 3,
	            std::begin(palette) + 3 * *first_transparent);
	std::copy(transparent_pixel.cbegin(), transparent_pixel.cend(),
	          std::end(palette) - 3);
}

namespace {

class mipmap_generator {
public:
	mipmap_generator(image& i, std::int32_t w, std::int32_t h,
	                 const std::vector<std::byte>& l) noexcept;

	[[nodiscard]] std::vector<std::byte> generate(int lvl);

private:
	void count_color(std::int32_t x, std::int32_t y);

	[[nodiscard]] constexpr std::tuple<float, float, float>
	get_average_color() const noexcept;

	unsigned char average_pixels();
	unsigned char add_color(float red, float green, float blue);
	int find_unused_color() const noexcept;

	unsigned char
	reduce_pixel(std::int32_t x, std::int32_t y, std::int32_t step,
	             unsigned int test);

	image& img;
	const std::vector<std::byte>& lump;
	const std::int32_t width;
	const std::int32_t height;

	std::array<float, 768> linear_palette{};
	float max_distortion = 0.f;
	unsigned int colors_used = 0;
	std::array<bool, 256> color_used{};

	float distortion_red = 0.f;
	float distortion_green = 0.f;
	float distortion_blue = 0.f;
	std::array<unsigned char, 256> pixel_mask{};
	std::uint_fast8_t pixel_count = 0;
};

void mipmap_generator::count_color(std::int32_t x, std::int32_t y)
{
	const auto offset = 40 + y * width + x;
	const auto c = std::to_integer<unsigned char>(lump[offset]);
	if (!color_used[c]) {
		color_used[c] = true;
		++colors_used;
	}
}

mipmap_generator::mipmap_generator(image& i, std::int32_t w, std::int32_t h,
                                   const std::vector<std::byte>& l)
	noexcept
	: img{i}
	, lump{l}
	, width{w}
	, height{h}
{
	// Linearize the palette
	{
		auto adjust_gamma = [this](unsigned char val) {
			const float f = static_cast<float>(val) / 255.f;
			return std::pow(f, 2.2f);
		};
		img.transform_palette(linear_palette.begin(), adjust_gamma);
	}

	if (img.is_transparent()) {
		// Assume the palette is full
		colors_used = 255;
		std::fill(color_used.begin(), color_used.end(), true);
	} else {
		std::fill(color_used.begin(), color_used.end(), false);
		for (std::int32_t y = 0; y < height; ++y) {
			for (std::int32_t x = 0; x < width; ++x)
				count_color(x, y);
		}
	}
}

int mipmap_generator::find_unused_color() const noexcept
{
	const auto begin = color_used.cbegin();
	const auto end = color_used.cend();
	return static_cast<int>(std::find(begin, end, false) - begin);
}

unsigned char mipmap_generator::add_color(float red, float green, float blue)
{
	static const auto trans = [](float x) {
		if (x < 0.f)
			x = 0.f;
		if (x > 1.f)
			x = 1.f;
		const float y = std::pow(x, 1.f / 2.2f) * 255.f;
		return static_cast<unsigned char>(y);
	};
	const int c = find_unused_color();
	linear_palette[3 * c] = red;
	linear_palette[3 * c + 1] = green;
	linear_palette[3 * c + 2] = blue;
	img.set_color(c, trans(red), trans(green), trans(blue));
	color_used[c] = true;
	++colors_used;
	return static_cast<unsigned char>(c);
}

[[nodiscard]] constexpr float hypotsqr(float x, float y, float z) noexcept
{
	return x * x + y * y + z * z;
}

constexpr std::tuple<float, float, float>
mipmap_generator::get_average_color() const noexcept
{
	float red = 0.f;
	float green = 0.f;
	float blue = 0.f;
	for (unsigned int i = 0; i < pixel_count; ++i) {
		const unsigned int c = pixel_mask[i];
		red += linear_palette[3 * c];
		green += linear_palette[3 * c + 1];
		blue += linear_palette[3 * c + 2];
	}
	const float n = pixel_count;
	return {red / n + distortion_red,
	        green / n + distortion_green,
	        blue / n + distortion_blue};
}

unsigned char mipmap_generator::average_pixels()
{
	const auto [red, green, blue] = get_average_color();

	float best_distortion = 3.f;
	int best_color = -1;
	const auto extract_value = [this](int c, int s) {
		return linear_palette[3 * c + s];
	};
	for (int c = 0; c < 255; ++c) {
		if (!color_used[c])
			continue;
		const float dist_red = red - extract_value(c, 0);
		const float dist_green = green - extract_value(c, 1);
		const float dist_blue = blue - extract_value(c, 2);
		const float dist = hypotsqr(dist_red, dist_green, dist_blue);
		if (dist < best_distortion) {
			if (dist == 0) {
				distortion_red = 0.f;
				distortion_green = 0.f;
				distortion_blue = 0.f;
				return static_cast<unsigned char>(c);
			}
			best_distortion = dist;
			best_color = c;
		}
	}
	if (best_distortion > 0.001f && colors_used < 255) {
		best_color = add_color(red, green, blue);
		distortion_red = distortion_green = distortion_blue = 0.f;
		best_distortion = 0.f;
	} else {
		distortion_red = red - extract_value(best_color, 0);
		distortion_green = green - extract_value(best_color, 1);
		distortion_blue = blue - extract_value(best_color, 2);
	}
	return static_cast<unsigned char>(best_color);
}

unsigned char
mipmap_generator::reduce_pixel(std::int32_t x, std::int32_t y,
                               std::int32_t step, unsigned int test)
{
	pixel_count = 0;
	for (std::int32_t j = 0; j < step; ++j) {
		for (std::int32_t i = 0; i < step; ++i) {
			const std::byte b = lump[40 + (y + j) * width + x + i];
			const auto p = std::to_integer<unsigned char>(b);
			if (!img.is_transparent() || p != 255) {
				pixel_mask.at(pixel_count) = p;
				++pixel_count;
			}
		}
	}
	return pixel_count <= test ? 0xff : average_pixels();
}

std::vector<std::byte> mipmap_generator::generate(const int lvl)
{
	distortion_red = distortion_green = distortion_blue = 0.f;
	std::int32_t step = std::int32_t{1} << lvl;
	const unsigned int test = (step * step * 2) / 5; // 40%
	std::vector<std::byte> mipmap;
	mipmap.reserve((height / lvl) * (width / lvl));
	for (std::int32_t y = 0; y < height; y += step) {
		for (std::int32_t x = 0; x < width; x += step) {
			using std::byte;
			mipmap.push_back(byte{reduce_pixel(x, y, step, test)});
		}
	}
	return mipmap;
}

}

image::lump_type
image::grab_palette(std::string_view,
                    const std::vector<std::variant<std::int32_t, float>>&)
{
	std::ostringstream s;
	s << __FILE__ ":" << __func__ << ':' << __LINE__ << ": unimplemented";
	throw std::logic_error(s.str());
}

image::lump_type
image::grab_colormap(std::string_view,
                     const std::vector<std::variant<std::int32_t, float>>&)
{
	std::ostringstream s;
	s << __FILE__ ":" << __func__ << ':' << __LINE__ << ": unimplemented";
	throw std::logic_error(s.str());
}

image::lump_type
image::grab_qpic(std::string_view,
                 const std::vector<std::variant<std::int32_t, float>>&)
{
	std::ostringstream s;
	s << __FILE__ ":" << __func__ << ':' << __LINE__ << ": unimplemented";
	throw std::logic_error(s.str());
}

template<class It>
It put_lump_name(It it, std::string_view name)
{
	const std::locale loc;
	if (!std::has_facet<std::ctype<char>>(loc)) {
		std::ostringstream s;
		s << __FILE__ ":" << __func__ << ':' << __LINE__ << ": Locale "
		  << loc.name() << " cannot convert characters";
		throw std::logic_error(s.str());
	}
	const auto tr = [&loc](const char c) {
		const char l = std::tolower(c, loc);
		return std::byte{static_cast<unsigned char>(l)};
	};
	it = std::transform(name.cbegin(), name.cend(), it, tr);
	return std::fill_n(it, 16 - name.size(), std::byte{0x00});
}

static void check_miptex_size(std::int32_t w, std::int32_t h)
{
	if (w % 16 != 0) {
		std::ostringstream s;
		s << __FILE__ ":" << __func__ << ':' << __LINE__
		  << ": Image width (" << w << ") is not a multiple of 16";
		throw std::invalid_argument(s.str());
	} else if (h % 16 != 0) {
		std::ostringstream s;
		s << __FILE__ ":" << __func__ << ':' << __LINE__
		  << ": Image height (" << h << ") is not a multiple of 16";
		throw std::invalid_argument(s.str());
	} else if (w > 1 << 15) {
		std::ostringstream s;
		s << __FILE__ ":" << __func__ << ':' << __LINE__
		  << ": Image width (" << w << ") exceeds the maximum of"
		  << (1 << 15);
		throw std::invalid_argument(s.str());
	} else if (h > 1 << 15) {
		std::ostringstream s;
		s << __FILE__ ":" << __func__ << ':' << __LINE__
		  << ": Image height (" << h << ") exceeds the maximum of"
		  << (1 << 15);
		throw std::invalid_argument(s.str());
	} else if (w * h > (0x50000 - 810) / 2) {
		std::ostringstream s;
		s << __FILE__ ":" << __func__ << ':' << __LINE__
		  << ": Image size (" << (w * h) << ") exceeds the maximum of"
		  << ((0x50000 - 810) / 2);
		throw std::invalid_argument(s.str());
	}
}

static std::tuple<std::int32_t, std::int32_t, std::int32_t, std::int32_t>
get_miptex_arguments(const std::vector<std::variant<std::int32_t, float>>& arg)
{
	using std::int32_t;
	if (arg.size() != 4) {
		std::ostringstream s;
		s << __FILE__ ":" << __func__ << ':' << __LINE__
		  << ": Expected 4 arguments, got " << arg.size();
		throw std::invalid_argument(s.str());
	}
	for (int i = 0; i < 4; ++i) {
		if (std::holds_alternative<int32_t>(arg[i]))
			continue;
		std::ostringstream s;
		s << __FILE__ ":" << __func__ << ':' << __LINE__
		  << ": Expected an integer in argument " << i
		  << ", got a floating-point number";
		throw std::invalid_argument(s.str());
	}
	return {std::get<int32_t>(arg[0]), std::get<int32_t>(arg[1]),
	        std::get<int32_t>(arg[2]), std::get<int32_t>(arg[3])};
}

image::lump_type
image::grab_miptex(std::string_view name,
                   const std::vector<std::variant<std::int32_t, float>>& args)
{
	auto [x, y, w, h] = get_miptex_arguments(args);
	if (x < 0 || y < 0 || w < 0 || h < 0) {
		x = y = 0;
		w = width;
		h = height;
	}
	check_miptex_size(w, h);
	if (name.size() >= 16) {
		std::ostringstream s;
		s << __FILE__ ":" << __func__ << ':' << __LINE__
		  << ": Lump name '" << name << "' has length " << name.size()
		  << ", maximum allowed is 15";
		throw std::invalid_argument(s.str());
	}
	std::vector<std::byte> lump;
	lump.reserve(810 + 2 * w * h);
	auto it = std::back_inserter(lump);
	it = put_lump_name(it, name);
	const std::int32_t vals[]{w, h, 40, 0, 0, 0};
	for (const std::int32_t n : vals)
		put_little_endian(it, n);

	// Transfer image lines
	for (std::int32_t j = y; j < y + h; ++j) {
		const std::int32_t left = j * width + x;
		std::copy(&data[left], &data[left + w], it);
		std::fill(&data[left], &data[left + w], std::byte{0x00});
	}

	mipmap_generator generator(*this, w, h, lump);
	for (int lvl = 1; lvl < 4; ++lvl) {
		put_little_endian(lump.begin() + 24 + 4 * lvl,
		                  static_cast<std::int32_t>(lump.size()));
		const auto mipmap = generator.generate(lvl);
		std::copy(mipmap.cbegin(), mipmap.cend(), it);
	}
	if (check_wad3()) {
		put_little_endian(it, std::uint16_t{256});
		std::copy(std::cbegin(palette), std::cend(palette), it);
	}
	return lump;
}

image::lump_type
image::grab_raw(std::string_view,
                const std::vector<std::variant<std::int32_t, float>>&)
{
	std::ostringstream s;
	s << __FILE__ ":" << __func__ << ':' << __LINE__ << ": unimplemented";
	throw std::logic_error(s.str());
}

image::lump_type
image::grab_colormap2(std::string_view,
                      const std::vector<std::variant<std::int32_t, float>>&)
{
	std::ostringstream s;
	s << __FILE__ ":" << __func__ << ':' << __LINE__ << ": unimplemented";
	throw std::logic_error(s.str());
}

image::lump_type
image::grab_font(std::string_view,
                 const std::vector<std::variant<std::int32_t, float>>&)
{
	std::ostringstream s;
	s << __FILE__ ":" << __func__ << ':' << __LINE__ << ": unimplemented";
	throw std::logic_error(s.str());
}
