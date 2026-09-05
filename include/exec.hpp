#pragma once
#include "ast.hpp"
#include "db.hpp"
#include <optional>
#include <string>

std::optional<std::string> execute(Database& db, const Stmt& s);
