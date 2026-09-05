#include "exec.hpp"
#include "format.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

static std::string value_to_string(const Value& v) {
    if (std::holds_alternative<int>(v))
        return std::to_string(std::get<int>(v));
    return std::get<std::string>(v);
}

static Value default_for(Type t) {
    return (t == Type::Int) ? Value{0} : Value{std::string("")};
}

static bool equal_values(const Value& a, const Value& b) {
    if (a.index() != b.index())
        return false;
    if (std::holds_alternative<int>(a))
        return std::get<int>(a) == std::get<int>(b);
    else
        return std::get<std::string>(a) == std::get<std::string>(b);
}

static bool less_values(const Value& a, const Value& b) {
    if (!(std::holds_alternative<int>(a) && std::holds_alternative<int>(b)))
        throw std::runtime_error("WHERE: ordering only supported for int");
    return std::get<int>(a) < std::get<int>(b);
}

static std::pair<std::string, std::string> split_qualified(const std::string& name) {
    auto dot = name.find('.');
    if (dot == std::string::npos)
        throw std::runtime_error("JOIN: columns in SELECT must be qualified (table.col)");
    return {name.substr(0, dot), name.substr(dot + 1)};
}

static bool matches_expr(const Table& t, const Row& r, const Expr& e) {
    int ci = t.col_index(e.column);
    if (ci < 0)
        throw std::runtime_error("Unknown column in WHERE: " + e.column);
    const Value& cell = r.cells[static_cast<size_t>(ci)];

    if (cell.index() != e.literal.index()) {
        throw std::runtime_error("Type mismatch in WHERE for column: " +
                                 t.schema[static_cast<size_t>(ci)].name);
    }

    switch (e.op) {
    case Cmp::EQ:
        return equal_values(cell, e.literal);
    case Cmp::NE:
        return !equal_values(cell, e.literal);
    case Cmp::LT:
        return less_values(cell, e.literal);
    case Cmp::LE:
        return less_values(cell, e.literal) || equal_values(cell, e.literal);
    case Cmp::GT:
        return less_values(e.literal, cell);
    case Cmp::GE:
        return less_values(e.literal, cell) || equal_values(cell, e.literal);
    }
    return false;
}

static bool matches_where(const Table& t, const Row& r, const std::optional<Expr>& where) {
    return !where || matches_expr(t, r, *where);
}

std::optional<std::string> execute(Database& db, const Stmt& s) {

    if (auto p = std::get_if<CreateTableStmt>(&s)) {
        db.create_table(p->name, p->columns);
        return std::nullopt;
    }

    if (auto p = std::get_if<InsertStmt>(&s)) {
        Table& t = db.table(p->table);
        std::vector<int> idx;
        if (p->column_order.empty()) {

            idx.resize(t.schema.size());
            for (size_t i = 0; i < t.schema.size(); ++i) {
                idx[i] = static_cast<int>(i);
            }
        } else {
            idx.resize(p->column_order.size(), -1);
            for (size_t i = 0; i < p->column_order.size(); ++i) {
                int ci = t.col_index(p->column_order[i]);
                if (ci < 0)
                    throw std::runtime_error("Unknown column in INSERT: " + p->column_order[i]);
                idx[i] = ci;
            }
        }

        for (const auto& tup : p->rows) {
            if ((p->column_order.empty() && tup.size() != t.schema.size()) ||
                (!p->column_order.empty() && tup.size() != p->column_order.size()))
                throw std::runtime_error("INSERT: tuple size mismatch");

            Row r;
            r.cells.resize(t.schema.size());

            for (size_t c = 0; c < t.schema.size(); ++c)
                r.cells[c] = default_for(t.schema[c].type);

            for (size_t k = 0; k < tup.size(); ++k) {
                size_t ci = static_cast<size_t>(idx[k]);
                const Column& col = t.schema[ci];
                const Value& v = tup[k];

                if (col.type == Type::Int && !std::holds_alternative<int>(v))
                    throw std::runtime_error("INSERT: type mismatch (expected int) for column: " +
                                             col.name);
                if (col.type == Type::Str && !std::holds_alternative<std::string>(v))
                    throw std::runtime_error("INSERT: type mismatch (expected str) for column: " +
                                             col.name);

                r.cells[ci] = v;
            }
            t.rows.push_back(std::move(r));
        }
        return std::nullopt;
    }

    // DELETE FROM
    if (auto p = std::get_if<DeleteStmt>(&s)) {
        Table& t = db.table(p->table);
        if (!p->where) {
            t.rows.clear();
        } else {
            const Expr& e = *p->where;
            auto& rows = t.rows;
            rows.erase(std::remove_if(rows.begin(), rows.end(),
                                      [&](const Row& r) { return matches_expr(t, r, e); }),
                       rows.end());
        }
        return std::nullopt;
    }

    // UPDATE z set where
    if (auto p = std::get_if<UpdateStmt>(&s)) {
        Table& t = db.table(p->table);

        std::vector<std::pair<size_t, Value>> plan;
        plan.reserve(p->sets.size());
        for (const auto& [name, val] : p->sets) {
            int ci = t.col_index(name);
            if (ci < 0)
                throw std::runtime_error("Unknown column in UPDATE: " + name);
            const Column& col = t.schema[static_cast<size_t>(ci)];
            bool ok = (col.type == Type::Int && std::holds_alternative<int>(val)) ||
                      (col.type == Type::Str && std::holds_alternative<std::string>(val));
            if (!ok)
                throw std::runtime_error("Type mismatch in UPDATE for column: " + name);
            plan.emplace_back(static_cast<size_t>(ci), val);
        }

        for (auto& r : t.rows) {
            if (!matches_where(t, r, p->where))
                continue;
            for (auto& [ci, v] : plan)
                r.cells[ci] = v;
        }
        return std::nullopt;
    }

    // SELECT pr, where, j
    if (auto p = std::get_if<SelectStmt>(&s)) {

        if (p->join) {
            const Table& L = db.table(p->table);
            const Table& R = db.table(p->join->right_table);

            int li = L.col_index(p->join->left_col);
            if (li < 0)
                throw std::runtime_error("JOIN: unknown left column: " + p->join->left_col);
            int ri = R.col_index(p->join->right_col);
            if (ri < 0)
                throw std::runtime_error("JOIN: unknown right column: " + p->join->right_col);

            struct Proj {
                bool left;
                size_t idx;
            };
            std::vector<Proj> plan;
            std::vector<std::string> header;

            if (p->columns.empty()) {

                for (size_t i = 0; i < L.schema.size(); ++i) {
                    plan.push_back({true, i});
                    header.push_back(p->table + "." + L.schema[i].name);
                }
                for (size_t i = 0; i < R.schema.size(); ++i) {
                    plan.push_back({false, i});
                    header.push_back(p->join->right_table + "." + R.schema[i].name);
                }
            } else {

                for (const auto& qname : p->columns) {
                    auto [qtbl, qcol] = split_qualified(qname);
                    if (qtbl == p->table) {
                        int ci = L.col_index(qcol);
                        if (ci < 0)
                            throw std::runtime_error("Unknown column: " + qname);
                        plan.push_back({true, static_cast<size_t>(ci)});
                        header.push_back(qname);
                    } else if (qtbl == p->join->right_table) {
                        int ci = R.col_index(qcol);
                        if (ci < 0)
                            throw std::runtime_error("Unknown column: " + qname);
                        plan.push_back({false, static_cast<size_t>(ci)});
                        header.push_back(qname);
                    } else {
                        throw std::runtime_error("Unknown table qualifier in column: " + qname);
                    }
                }
            }

            std::vector<RowText> out;
            for (const auto& lr : L.rows) {
                const Value& lk = lr.cells[static_cast<size_t>(li)];
                for (const auto& rr : R.rows) {
                    const Value& rk = rr.cells[static_cast<size_t>(ri)];
                    if (!equal_values(lk, rk))
                        continue;

                    std::vector<std::string> line;
                    line.reserve(plan.size());
                    for (const auto& pj : plan) {
                        const Value& v = pj.left ? lr.cells[pj.idx] : rr.cells[pj.idx];
                        line.push_back(value_to_string(v));
                    }
                    out.push_back(std::move(line));
                }
            }
            return to_ascii(header, out);
        }

        const Table& t = db.table(p->table);

        // proj
        std::vector<size_t> proj;
        std::vector<std::string> header;
        if (p->columns.empty()) {
            for (size_t i = 0; i < t.schema.size(); ++i) {
                proj.push_back(i);
                header.push_back(t.schema[i].name);
            }
        } else {
            for (const auto& name : p->columns) {
                int ci = t.col_index(name);
                if (ci < 0)
                    throw std::runtime_error("Unknown column in SELECT: " + name);
                proj.push_back(static_cast<size_t>(ci));
                header.push_back(name);
            }
        }

        std::vector<RowText> out;
        out.reserve(t.rows.size());
        for (const auto& r : t.rows) {
            if (!matches_where(t, r, p->where))
                continue;
            std::vector<std::string> line;
            line.reserve(proj.size());
            for (size_t i : proj)
                line.push_back(value_to_string(r.cells[i]));
            out.push_back(std::move(line));
        }

        return to_ascii(header, out);
    }

    // no of the statemnents
    throw std::runtime_error("Executor: statement not implemented");
}
