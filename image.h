#ifndef IMAGE_H
#define IMAGE_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

class image {
public:
	using argument_type = std::variant<std::int32_t, float>;
	using lump_type = std::vector<std::byte>;
	enum class load_type { bmp, lbm };

	constexpr image() noexcept
		: data{nullptr}
		, width{0}
		, height{0}
		, transparent{false}
	{}

	image(const image&) = delete;
	image& operator=(const image&) = delete;
	image(image&& other) noexcept;
	image& operator=(image&& other) noexcept;

	~image() noexcept {
		if (data)
			delete[] data;
	}

	image(const std::filesystem::path& path, load_type mode);

	[[nodiscard]] constexpr std::pair<int32_t, int32_t>
	dimensions() const noexcept { return {width, height}; }

	[[nodiscard]]
	constexpr bool is_transparent() const noexcept { return transparent; }

	constexpr void
	set_color(int c, unsigned char r, unsigned char g, unsigned char b)
		noexcept
	{
		palette[3 * c] = std::byte{r};
		palette[3 * c + 1] = std::byte{g};
		palette[3 * c + 2] = std::byte{b};
	}

	template<class It, class U>
	void transform_palette(It output, U op) const {
		using std::to_integer;
		for (int i = 0; i < 768; ++i)
			*output++ = op(to_integer<unsigned char>(palette[i]));
	}

	lump_type
	grab_palette(std::string_view name,
	             const std::vector<argument_type>& arg);

	lump_type
	grab_colormap(std::string_view name,
	              const std::vector<argument_type>& arg);

	lump_type
	grab_qpic(std::string_view name,
	          const std::vector<argument_type>& arg);

	lump_type
	grab_miptex(std::string_view name,
	            const std::vector<argument_type>& arg);

	lump_type
	grab_raw(std::string_view name,
	         const std::vector<argument_type>& arg);

	lump_type
	grab_colormap2(std::string_view name,
	               const std::vector<argument_type>& arg);

	lump_type
	grab_font(std::string_view name,
	          const std::vector<argument_type>& arg);

private:
	void load_bmp(const std::filesystem::path& path);
	void load_lbm(const std::filesystem::path& path);
	void make_transparent() noexcept; // may become public
	void permute(const std::byte table[256]) noexcept;

	std::byte palette[768]{};
	std::byte* data;
	int32_t width;
	int32_t height;
	bool transparent;
};

#endif
