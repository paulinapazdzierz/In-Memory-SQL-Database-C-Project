#pragma once
#include <optional>
#include <string>
#include <variant>
#include <vector>

enum class Type { Int, Str };

using Value = std::variant<int, std::string>;

struct Column {
    std::string name;
    Type type;
};

enum class Cmp { EQ, NE, LT, LE, GT, GE };

struct Expr {
    std::string column;
    Cmp op;
    Value literal;
};

struct CreateTableStmt {
    std::string name;
    std::vector<Column> columns;
};

struct InsertStmt {
    std::string table;
    std::vector<std::string> column_order;
    std::vector<std::vector<Value>> rows;
};

struct DeleteStmt {
    std::string table;
    std::optional<Expr> where;
};

struct UpdateStmt {
    std::string table;
    std::vector<std::pair<std::string, Value>> sets;
    std::optional<Expr> where;
};

struct JoinSpec {
    std::string right_table;
    std::string left_col;
    std::string right_col;
};

struct SelectStmt {
    std::string table;
    std::vector<std::string> columns;
    std::optional<Expr> where;
    std::optional<JoinSpec> join;
};

using Stmt = std::variant<CreateTableStmt, InsertStmt, DeleteStmt, UpdateStmt, SelectStmt>;
