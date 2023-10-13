#ifndef CMD_H
#define CMD_H

#include <filesystem>

void set_working_directory(std::filesystem::path project);
[[nodiscard]] std::filesystem::path expand(const std::filesystem::path& p);
void plan_wad2() noexcept;
[[nodiscard]] bool check_wad3() noexcept;

#endif
