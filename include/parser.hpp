#pragma once
#include "ast.hpp"
#include <string>
#include <vector>

// for splitting each
std::vector<std::string> split_statements(const std::string& input);

// for parsing to ast
std::vector<Stmt> parse_all(const std::string& input);
