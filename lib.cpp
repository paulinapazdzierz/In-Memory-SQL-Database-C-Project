// lib.cpp
#include "lib.hpp"

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

#include "db.hpp"
#include "exec.hpp"
#include "parser.hpp"

namespace mylib {

static void strip_bom(std::string& s) {
    if (s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xEF &&
        static_cast<unsigned char>(s[1]) == 0xBB && static_cast<unsigned char>(s[2]) == 0xBF) {
        s.erase(0, 3);
    }
}

std::string run_script(const std::string& sql) {
    std::string input = sql;
    strip_bom(input);

    Database db;
    auto statements = parse_all(input);

    std::string out;
    for (const auto& st : statements) {
        if (auto txt = execute(db, st)) {
            out += *txt;
            out += '\n';
        }
    }
    return out;
}

std::string run_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("Error: cannot open file: " + path);
    std::string input((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return run_script(input);
}

} // namespace mylib
