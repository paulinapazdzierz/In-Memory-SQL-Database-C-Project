#include "db.hpp"
#include <stdexcept>

int Table::col_index(const std::string& name) const {
    for (size_t i = 0; i < schema.size(); ++i)
        if (schema[i].name == name)
            return static_cast<int>(i);
    return -1;
}

Table& Database::create_table(const std::string& name, const std::vector<Column>& cols) {
    if (tables_.count(name))
        throw std::runtime_error("Table already exists: " + name);
    return tables_.emplace(name, Table{cols, {}}).first->second;
}

Table& Database::table(const std::string& name) {
    auto it = tables_.find(name);
    if (it == tables_.end())
        throw std::runtime_error("Unknown table: " + name);
    return it->second;
}

const Table& Database::table(const std::string& name) const {
    auto it = tables_.find(name);
    if (it == tables_.end())
        throw std::runtime_error("Unknown table: " + name);
    return it->second;
}
