#include <filesystem>
#include <iostream>
#include <string_view>

#include "image.h"
#include "wad.h"

/*
 * NOTE: Maximum supported dimensions are taken from this chart:
 * https://the303.org/tutorials/goldsrclogoimages/goldsrcchart.png
 */

using namespace std::literals;

static void warn_dimensions(const image& img)
{
	const auto [width, height] = img.dimensions();
	const std::int32_t surface = width * height;
	if (surface > 14336) {
		std::cout << "Warning: image has "sv << surface
		          << " pixels which is too much even for Sven Co-op"sv
		          << std::endl;
	} else if (surface > 12288) {
		std::cout << "Warning: image has "sv << surface
		          << " pixels which is valid only for Sven Co-op"sv
		          << std::endl;
	}
}

void run_spray(const std::filesystem::path& in)
{
	static constexpr char type = wad::type_lumpy + 3;
	image img(in, image::load_type::bmp);
	warn_dimensions(img);
	const auto lump_data = img.grab_miptex("{LOGO"sv, {-1, -1, -1, -1});
	const wad::lump lump("{LOGO"sv, lump_data.data(), lump_data.size());
	wad::add("tempdecal.wad"sv, lump, type);
	wad::write();
}
