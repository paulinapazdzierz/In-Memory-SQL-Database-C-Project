#pragma once
#include <string>
#include <vector>

using RowText = std::vector<std::string>;

std::string to_csv(const std::vector<std::string>& header, const std::vector<RowText>& rows);

std::string to_ascii(const std::vector<std::string>& header, const std::vector<RowText>& rows);
