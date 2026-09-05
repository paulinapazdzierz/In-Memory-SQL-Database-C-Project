// lib.hpp
#pragma once
#include <string>

namespace mylib {

std::string run_script(const std::string& sql);

std::string run_file(const std::string& path);

} // namespace mylib
