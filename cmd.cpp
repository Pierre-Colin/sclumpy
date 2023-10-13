#include <cstdlib>
#include <filesystem>

#include "cmd.h"

using std::filesystem::path;

static path working_directory;
static bool wad2;

static path get_path_from_environment(const char* const var)
{
	const char* const value = std::getenv(var);
	return value != nullptr ? path{value} : path{};
}

void set_working_directory(path project)
{
	if (project.empty())
		project = get_path_from_environment("QPROJECT");
	working_directory = std::filesystem::current_path() / project;
}

path expand(const path& p)
{
	return working_directory / p;
}

void plan_wad2() noexcept
{
	wad2 = true;
}

bool check_wad3() noexcept
{
	return !wad2;
}
