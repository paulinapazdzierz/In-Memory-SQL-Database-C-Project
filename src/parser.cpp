#include "parser.hpp"
#include <cctype>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

//  split on ;
std::vector<std::string> split_statements(const std::string& input) {
    std::vector<std::string> out;
    std::string cur;
    bool in_str = false;

    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        if (c == '"') {
            in_str = !in_str;
            cur += c;
            continue;
        }
        if (!in_str && c == ';') {
            if (!cur.empty())
                out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty())
        out.push_back(cur);
    return out;
}

static std::string trim(std::string s) {
    auto issp = [](int ch) { return std::isspace(ch); };
    while (!s.empty() && issp(s.front()))
        s.erase(s.begin());
    while (!s.empty() && issp(s.back()))
        s.pop_back();
    return s;
}
static bool starts_with(const std::string& s, const char* kw) {
    return s.rfind(kw, 0) == 0;
}
static std::string unquote(std::string s) {
    s = trim(s);
    if (!s.empty() && s.front() == '"' && s.back() == '"') {
        s.erase(s.begin());
        s.pop_back();
    }
    return s;
}
static Type parse_type(const std::string& t) {
    if (t == "int")
        return Type::Int;
    if (t == "str")
        return Type::Str;
    throw std::runtime_error("Unknown type: " + t);
}
static Value parse_value(std::string s) {
    s = trim(s);
    if (!s.empty() && s.front() == '"')
        return Value{unquote(s)};
    return Value{std::stoi(s)};
}

static std::vector<std::string> split_commas_ignoring_quotes(const std::string& s) {
    std::vector<std::string> parts;
    bool inq = false;
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || (!inq && s[i] == ',')) {
            parts.push_back(trim(s.substr(start, i - start)));
            start = i + 1;
        } else if (s[i] == '"')
            inq = !inq;
    }
    return parts;
}
// WHERE with =,!=,<,<=,>,>=
static Expr parse_where_expr(std::string cond) {
    cond = trim(cond);

    struct Op {
        const char* tok;
        Cmp cmp;
    } ops[] = {{"!=", Cmp::NE}, {"<=", Cmp::LE}, {">=", Cmp::GE},
               {"<", Cmp::LT},  {">", Cmp::GT},  {"=", Cmp::EQ}};
    size_t op_pos = std::string::npos;
    Cmp cmp = Cmp::EQ;
    size_t rhs_pos = 0;
    for (auto& o : ops) {
        op_pos = cond.find(o.tok);
        if (op_pos != std::string::npos) {
            cmp = o.cmp;
            rhs_pos = op_pos + std::strlen(o.tok);
            break;
        }
    }
    if (op_pos == std::string::npos)
        throw std::runtime_error("WHERE: expected =,!=,<,<=,>,>=");

    std::string col = trim(cond.substr(0, op_pos));
    std::string rhs = trim(cond.substr(rhs_pos));
    return Expr{col, cmp, parse_value(rhs)};
}

static CreateTableStmt parse_create(std::string s) {
    s = trim(s.substr(std::string("CREATE TABLE").size()));
    auto p = s.find('(');
    if (p == std::string::npos)
        throw std::runtime_error("CREATE TABLE: missing '('");
    std::string name = trim(s.substr(0, p));
    std::string cols = s.substr(p + 1);
    if (cols.empty() || cols.back() != ')')
        throw std::runtime_error("CREATE TABLE: missing ')'");
    cols.pop_back();

    std::vector<Column> schema;
    size_t pos = 0;
    while (pos < cols.size()) {
        auto comma = cols.find(',', pos);
        std::string item =
            trim(cols.substr(pos, comma == std::string::npos ? cols.size() - pos : comma - pos));
        if (!item.empty()) {
            auto sp = item.find(' ');
            if (sp == std::string::npos)
                throw std::runtime_error("Bad column: " + item);
            schema.push_back(Column{item.substr(0, sp), parse_type(trim(item.substr(sp + 1)))});
        }
        if (comma == std::string::npos)
            break;
        pos = comma + 1;
    }
    return {name, schema};
}

// insert into
static InsertStmt parse_insert(std::string s) {
    s = trim(s.substr(std::string("INSERT INTO").size()));
    std::string table;
    std::vector<std::string> column_order;
    std::string rest;
    auto p = s.find('(');
    auto values_pos = s.find("VALUES");
    if (p != std::string::npos && (values_pos == std::string::npos || p < values_pos)) {
        // 1 opcja: INSERT INTO table (col1, col2, ...) VALUES (...)
        table = trim(s.substr(0, p));
        auto q = s.find(')', p + 1);
        if (q == std::string::npos)
            throw std::runtime_error("INSERT: missing ')'");
        std::string cols_part = s.substr(p + 1, q - p - 1);
        size_t pos = 0;
        while (pos < cols_part.size()) {
            auto comma = cols_part.find(',', pos);
            std::string name = trim(cols_part.substr(
                pos, comma == std::string::npos ? cols_part.size() - pos : comma - pos));
            if (!name.empty())
                column_order.push_back(name);
            if (comma == std::string::npos)
                break;
            pos = comma + 1;
        }
        rest = trim(s.substr(q + 1));
    } else {
        // 2 opcja: INSERT INTO table VALUES (...)
        auto values_kw = s.find("VALUES");
        if (values_kw == std::string::npos)
            throw std::runtime_error("INSERT: expected VALUES");
        table = trim(s.substr(0, values_kw));
        column_order.clear();
        rest = trim(s.substr(values_kw + std::string("VALUES").size()));
    }
    if (rest.rfind("VALUES", 0) == 0)
        rest = trim(rest.substr(std::string("VALUES").size()));

    std::vector<std::vector<Value>> rows;
    while (!rest.empty()) {
        if (rest.front() != '(')
            throw std::runtime_error("INSERT: expected '('");
        int depth = 0;
        bool in_str = false;
        size_t i = 0;
        for (; i < rest.size(); ++i) {
            char c = rest[i];
            if (c == '"')
                in_str = !in_str;
            if (!in_str) {
                if (c == '(')
                    ++depth;
                if (c == ')') {
                    --depth;
                    if (depth == 0) {
                        ++i;
                        break;
                    }
                }
            }
        }
        if (depth != 0)
            throw std::runtime_error("INSERT: unbalanced parentheses");
        std::string tuple = rest.substr(1, i - 2);

        std::vector<Value> vals;
        for (const auto& piece : split_commas_ignoring_quotes(tuple)) {
            if (!piece.empty() && piece.front() == '"')
                vals.emplace_back(unquote(piece));
            else
                vals.emplace_back(std::stoi(piece));
        }
        rows.push_back(std::move(vals));

        rest = trim(rest.substr(i));
        if (!rest.empty()) {
            if (rest.front() != ',')
                throw std::runtime_error("INSERT: expected ',' between tuples");
            rest = trim(rest.substr(1));
        }
    }
    return {table, column_order, rows};
}

// SELECT
static SelectStmt parse_select(std::string s) {
    s = trim(s.substr(std::string("SELECT").size()));

    auto from_pos = s.find("FROM");
    if (from_pos == std::string::npos)
        throw std::runtime_error("SELECT: expected FROM");

    std::string cols_part = trim(s.substr(0, from_pos));
    std::string rest = trim(s.substr(from_pos + std::string("FROM").size()));

    std::vector<std::string> columns;
    if (cols_part != "*") {
        for (const auto& c : split_commas_ignoring_quotes(cols_part))
            if (!c.empty())
                columns.push_back(c);
    }

    SelectStmt out{};
    out.columns = std::move(columns);

    // checking for JOIN
    auto ij_pos = rest.find("INNER JOIN");
    if (ij_pos == std::string::npos) {
        // no join
        std::optional<Expr> where;
        auto where_pos = rest.find("WHERE");
        if (where_pos == std::string::npos) {
            out.table = trim(rest);
        } else {
            out.table = trim(rest.substr(0, where_pos));
            std::string cond = trim(rest.substr(where_pos + std::string("WHERE").size()));
            where = parse_where_expr(cond);
        }
        out.where = where;
        out.join.reset();
        return out;
    }

    // JOIN
    out.table = trim(rest.substr(0, ij_pos));

    std::string after_ij = trim(rest.substr(ij_pos + std::string("INNER JOIN").size()));
    auto on_pos = after_ij.find("ON");
    if (on_pos == std::string::npos)
        throw std::runtime_error("JOIN: expected ON");
    std::string right_table = trim(after_ij.substr(0, on_pos));
    std::string on_part = trim(after_ij.substr(on_pos + std::string("ON").size()));

    if (on_part.find("WHERE") != std::string::npos)
        throw std::runtime_error("JOIN: WHERE with JOIN not supported");

    auto eq_pos = on_part.find('=');
    if (eq_pos == std::string::npos)
        throw std::runtime_error("JOIN: expected '=' in ON");

    std::string lhs = trim(on_part.substr(0, eq_pos));
    std::string rhs = trim(on_part.substr(eq_pos + 1));

    auto dotL = lhs.find('.');
    auto dotR = rhs.find('.');
    if (dotL == std::string::npos || dotR == std::string::npos)
        throw std::runtime_error("JOIN: columns in ON must be qualified as table.col");

    std::string ltab = trim(lhs.substr(0, dotL));
    std::string lcol = trim(lhs.substr(dotL + 1));
    std::string rtab = trim(rhs.substr(0, dotR));
    std::string rcol = trim(rhs.substr(dotR + 1));

    if (ltab != out.table)
        throw std::runtime_error("JOIN: left side of ON must reference the FROM table");
    if (rtab != right_table)
        throw std::runtime_error("JOIN: right side of ON must reference the INNER JOIN table");

    out.where.reset();
    out.join = JoinSpec{right_table, lcol, rcol};
    return out;
}

// DELETE FROM
static DeleteStmt parse_delete(std::string s) {
    s = trim(s.substr(std::string("DELETE FROM").size()));
    auto wp = s.find("WHERE");
    if (wp == std::string::npos)
        return {trim(s), std::nullopt};
    std::string table = trim(s.substr(0, wp));
    std::string cond = trim(s.substr(wp + std::string("WHERE").size()));
    return {table, parse_where_expr(cond)};
}

//  UPDATE table SET.....
static UpdateStmt parse_update(std::string s) {
    s = trim(s.substr(std::string("UPDATE").size()));
    auto set_pos = s.find("SET");
    if (set_pos == std::string::npos)
        throw std::runtime_error("UPDATE: expected SET");

    std::string table = trim(s.substr(0, set_pos));
    std::string rest = trim(s.substr(set_pos + std::string("SET").size()));

    std::string assigns, cond;
    auto wp = rest.find("WHERE");
    if (wp == std::string::npos)
        assigns = trim(rest);
    else {
        assigns = trim(rest.substr(0, wp));
        cond = trim(rest.substr(wp + std::string("WHERE").size()));
    }

    std::vector<std::pair<std::string, Value>> sets;
    for (const auto& one : split_commas_ignoring_quotes(assigns)) {
        if (one.empty())
            continue;
        auto eq = one.find('=');
        if (eq == std::string::npos)
            throw std::runtime_error("UPDATE: expected '=' in assignment");
        std::string name = trim(one.substr(0, eq));
        std::string rhs = trim(one.substr(eq + 1));
        sets.emplace_back(name, parse_value(rhs));
    }

    std::optional<Expr> where;
    if (!cond.empty())
        where = parse_where_expr(cond);

    return {table, sets, where};
}

std::vector<Stmt> parse_all(const std::string& input) {
    std::vector<Stmt> out;
    for (auto raw : split_statements(input)) {
        std::string s = trim(raw);
        if (s.empty())
            continue;

        if (starts_with(s, "CREATE TABLE")) {
            out.emplace_back(parse_create(s));
        } else if (starts_with(s, "INSERT INTO")) {
            out.emplace_back(parse_insert(s));
        } else if (starts_with(s, "SELECT")) {
            out.emplace_back(parse_select(s));
        } else if (starts_with(s, "DELETE FROM")) {
            out.emplace_back(parse_delete(s));
        } else if (starts_with(s, "UPDATE")) {
            out.emplace_back(parse_update(s));
        } else {
            throw std::runtime_error("Parser: statement not supported");
        }
    }
    return out;
}
