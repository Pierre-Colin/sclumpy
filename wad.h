#ifndef WAD_H
#define WAD_H

#include <cstddef>
#include <filesystem>
#include <string_view>

namespace wad {

static constexpr char type_lumpy = 64;

class lump {
	friend void add(const std::filesystem::path&, const lump&, char);
public:
	using iterator = const std::byte*;

	static constexpr std::size_t max_size = 0x50000;

	lump() = delete;
	lump(std::string_view n, const std::byte* dat, std::size_t sz);
	void write(const std::filesystem::path& path) const;

	[[nodiscard]] constexpr std::string_view name() const noexcept {
		return {name_, name_len};
	}

	constexpr iterator begin() const noexcept { return data; }
	constexpr iterator end() const noexcept { return data + size; }

private:
	char name_[16]{};
	const std::size_t name_len;
	std::byte data[max_size]{};
	const std::size_t size;
};

void add(const std::filesystem::path& p, const lump& lmp, char type);
void write();

}

#endif
