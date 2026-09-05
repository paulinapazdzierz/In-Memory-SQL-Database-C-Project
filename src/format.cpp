#include "format.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>

static std::string escape_csv(const std::string& s) {
    bool needs = s.find_first_of(",\"\n") != std::string::npos;
    if (!needs)
        return s;
    std::string out = "\"";
    for (char c : s)
        out += (c == '"') ? "\"\"" : std::string(1, c);
    out += "\"";
    return out;
}

std::string to_csv(const std::vector<std::string>& header, const std::vector<RowText>& rows) {
    std::ostringstream oss;
    auto emit = [&](const std::vector<std::string>& cols) {
        for (size_t i = 0; i < cols.size(); ++i) {
            if (i)
                oss << ',';
            oss << escape_csv(cols[i]);
        }
        oss << '\n';
    };
    emit(header);
    for (auto& r : rows)
        emit(r);
    return oss.str();
}

static std::string line_of(const std::vector<size_t>& w) {
    std::string s = "+";
    for (auto x : w)
        s += std::string(x + 2, '-') + "+";
    return s;
}

std::string to_ascii(const std::vector<std::string>& header, const std::vector<RowText>& rows) {
    std::vector<size_t> width(header.size(), 0);
    for (size_t i = 0; i < header.size(); ++i)
        width[i] = std::max(width[i], header[i].size());
    for (auto& r : rows)
        for (size_t i = 0; i < r.size(); ++i)
            width[i] = std::max(width[i], r[i].size());

    std::ostringstream oss;
    oss << line_of(width) << '\n' << '|';
    for (size_t i = 0; i < header.size(); ++i)
        oss << ' ' << std::setw(static_cast<int>(width[i])) << std::left << header[i] << " |";
    oss << '\n' << line_of(width) << '\n';
    for (auto& r : rows) {
        oss << '|';
        for (size_t i = 0; i < r.size(); ++i)
            oss << ' ' << std::setw(static_cast<int>(width[i])) << std::left << r[i] << " |";
        oss << '\n' << line_of(width) << '\n';
    }
    return oss.str();
}
