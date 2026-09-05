#pragma once
#include "ast.hpp"
#include <string>
#include <unordered_map>
#include <vector>

struct Row {
    std::vector<Value> cells;
};

struct Table {
    std::vector<Column> schema;
    std::vector<Row> rows;

    int col_index(const std::string& name) const;
};

class Database {
  public:
    Table& create_table(const std::string& name, const std::vector<Column>& cols);
    Table& table(const std::string& name);
    const Table& table(const std::string& name) const;

  private:
    std::unordered_map<std::string, Table> tables_;
};
