#include "lib.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "db.hpp"
#include "exec.hpp"
#include "parser.hpp"

static void strip_bom(std::string& s) {
    if (s.size() >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB &&
        (unsigned char)s[2] == 0xBF) {
        s.erase(0, 3);
    }
}

int main(int argc, char** argv) {
    try {
        std::string input;
        if (argc > 1) {
            std::ifstream in(argv[1], std::ios::binary);
            if (!in) {
                std::cerr << "Error: cannot open file: " << argv[1] << "\n";
                return 1;
            }
            input.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        } else {
            input.assign(std::istreambuf_iterator<char>(std::cin),
                         std::istreambuf_iterator<char>());
        }

        strip_bom(input);

        Database db;
        auto statements = parse_all(input);

        for (const auto& s : statements) {
            if (auto out = execute(db, s)) {
                std::cout << *out << "\n";
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
