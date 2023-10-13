#ifndef SCRIPT_H
#define SCRIPT_H

#include <filesystem>
#include <stdexcept>

namespace script {

class syntax_error : public std::invalid_argument {
public:
	syntax_error(const char* file, const char* func, unsigned int line,
	             const char* msg);
};

void run_from_path(const std::filesystem::path&, const std::filesystem::path&);
void run_from_stdin(const std::filesystem::path& out);

}

#endif
