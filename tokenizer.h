#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

#include "script.h"

class script_tokenizer {
public:
	constexpr script_tokenizer() noexcept : info{} {}
	script_tokenizer(const std::filesystem::path& p);
	script_tokenizer(const script_tokenizer&) = delete;
	script_tokenizer& operator=(const script_tokenizer&) = delete;
	script_tokenizer(script_tokenizer&&) noexcept = default;
	script_tokenizer& operator=(script_tokenizer&&) noexcept = default;
	~script_tokenizer() noexcept;

	[[nodiscard]] std::string read_token();
	[[nodiscard]] bool eof() const;

	[[nodiscard]]
	bool has_path(const std::filesystem::path& p) const noexcept {
		return info && info->path == p;
	}

	[[nodiscard]]
	script::syntax_error make_syntax_error(std::string_view msg) const;

private:
	struct file_info {
		std::filesystem::path path;
		std::ifstream file;
	};

	[[nodiscard]] std::string read_quoted_token();

	std::optional<file_info> info;
	std::uintmax_t line = 1;
};

#endif
