#include "ast.hpp"
#include "db.hpp"
#include "exec.hpp"
#include "parser.hpp"
#include <catch2/catch_all.hpp>

// test cases for DELETE

TEST_CASE("DELETE clears table and SELECT returns no rows") {
    Database db;

    const char* sql = R"(CREATE TABLE users (name str, age int);
INSERT INTO users (name, age) VALUES ("Alice",30), ("Bob",25), ("Charlie",20);
DELETE FROM users;
SELECT * FROM users;)";

    auto statements = parse_all(sql);
    std::optional<std::string> last_out;
    for (const auto& st : statements) {
        last_out = execute(db, st);
    }

    REQUIRE(db.table("users").rows.empty());
    REQUIRE(last_out.has_value());
    REQUIRE(last_out->find("Alice") == std::string::npos);
    REQUIRE(last_out->find("Bob") == std::string::npos);
    REQUIRE(last_out->find("Charlie") == std::string::npos);
}

static std::vector<std::optional<std::string>> runSQL(Database& db, const std::string& sql) {
    auto stmts = parse_all(sql);
    std::vector<std::optional<std::string>> outs;
    outs.reserve(stmts.size());
    for (const auto& s : stmts)
        outs.push_back(execute(db, s));
    return outs;
}

static std::string lastSelectOutput(const std::vector<std::optional<std::string>>& outs) {
    for (auto it = outs.rbegin(); it != outs.rend(); ++it)
        if (it->has_value())
            return **it;
    return {};
}

TEST_CASE("DELETE all rows") {
    Database db;
    auto outs = runSQL(db, R"(CREATE TABLE users (name str, age int);
INSERT INTO users (name, age) VALUES ("Alice",30), ("Bob",25), ("Charlie",20);
DELETE FROM users;
SELECT * FROM users;)");
    REQUIRE(db.table("users").rows.empty());

    auto out = lastSelectOutput(outs);
    REQUIRE(out.find("Alice") == std::string::npos);
    REQUIRE(out.find("Bob") == std::string::npos);
    REQUIRE(out.find("Charlie") == std::string::npos);
}

TEST_CASE("DELETE chain: age>=25 then name='Dora'") {
    Database db;
    auto outs = runSQL(db, R"(CREATE TABLE users (name str, age int);
INSERT INTO users (name, age) VALUES ("Alice",30), ("Bob",25), ("Charlie",20), ("Dora",19);
DELETE FROM users WHERE age >= 25;
DELETE FROM users WHERE name = "Dora";
SELECT * FROM users;)");
    auto out = lastSelectOutput(outs);
    REQUIRE(out.find("Charlie") != std::string::npos);
    REQUIRE(out.find("Alice") == std::string::npos);
    REQUIRE(out.find("Bob") == std::string::npos);
    REQUIRE(out.find("Dora") == std::string::npos);
}

TEST_CASE("DELETE WHERE name = 'Bob'") {
    Database db;
    auto outs = runSQL(db, R"(CREATE TABLE users (name str, age int);
INSERT INTO users (name, age) VALUES ("Alice",30), ("Bob",25), ("Charlie",20);
DELETE FROM users WHERE name = "Bob";
SELECT * FROM users;)");
    auto out = lastSelectOutput(outs);
    REQUIRE(out.find("Alice") != std::string::npos);
    REQUIRE(out.find("Charlie") != std::string::npos);
    REQUIRE(out.find("Bob") == std::string::npos);
}

TEST_CASE("DELETE WHERE age >= 25") {
    Database db;
    auto outs = runSQL(db, R"(CREATE TABLE users (name str, age int);
INSERT INTO users (name, age) VALUES ("Alice",30), ("Bob",25), ("Charlie",20);
DELETE FROM users WHERE age >= 25;
SELECT * FROM users;)");
    auto out = lastSelectOutput(outs);
    REQUIRE(out.find("Charlie") != std::string::npos);
    REQUIRE(out.find("Alice") == std::string::npos);
    REQUIRE(out.find("Bob") == std::string::npos);
}

TEST_CASE("DELETE WHERE name != 'Alice'") {
    Database db;
    auto outs = runSQL(db, R"(CREATE TABLE users (name str, age int);
INSERT INTO users (name, age) VALUES ("Alice",30), ("Bob",25), ("Charlie",20);
DELETE FROM users WHERE name != "Alice";
SELECT * FROM users;)");
    auto out = lastSelectOutput(outs);
    REQUIRE(out.find("Alice") != std::string::npos);
    REQUIRE(out.find("Bob") == std::string::npos);
    REQUIRE(out.find("Charlie") == std::string::npos);
}

TEST_CASE("DELETE WHERE no match") {
    Database db;
    auto outs = runSQL(db, R"(CREATE TABLE users (name str, age int);
INSERT INTO users (name, age) VALUES ("Alice",30), ("Bob",25);
DELETE FROM users WHERE age < 0;
SELECT * FROM users;)");
    auto out = lastSelectOutput(outs);
    REQUIRE(out.find("Alice") != std::string::npos);
    REQUIRE(out.find("Bob") != std::string::npos);
}

//  DELETE + JOIN tests

TEST_CASE("DELETE then JOIN (remove Charlie, keep Alice/Bob scores)") {
    Database db;
    auto outs = runSQL(db, R"(CREATE TABLE users  (id int, name str);
CREATE TABLE scores (uid int, points int);
INSERT INTO users  (id, name)   VALUES (1,"Alice"), (2,"Bob"), (3,"Charlie");
INSERT INTO scores (uid,points) VALUES (1,10), (1,15), (2,5), (3,0);
DELETE FROM users WHERE name = "Charlie";
SELECT users.name, scores.points
FROM users
INNER JOIN scores ON users.id = scores.uid;)");
    auto out = lastSelectOutput(outs);

    REQUIRE(out.find("Alice") != std::string::npos);
    REQUIRE(out.find("10") != std::string::npos);
    REQUIRE(out.find("15") != std::string::npos);

    REQUIRE(out.find("Bob") != std::string::npos);
    REQUIRE(out.find("5") != std::string::npos);

    REQUIRE(out.find("Charlie") == std::string::npos);
    REQUIRE(out.find(" 0") == std::string::npos);
}

TEST_CASE("DELETE on LEFT side before JOIN") {
    Database db;
    auto outs = runSQL(db, R"(CREATE TABLE users (id int, name str);
CREATE TABLE scores (uid int, points int);
INSERT INTO users  (id, name)   VALUES (1,"Alice"), (2,"Bob"), (3,"Charlie");
INSERT INTO scores (uid,points) VALUES (1,10), (1,15), (2,5), (3,0);
DELETE FROM users WHERE name = "Charlie";
SELECT users.name, scores.points
FROM users
INNER JOIN scores ON users.id = scores.uid;)");
    auto out = lastSelectOutput(outs);
    REQUIRE(out.find("Alice") != std::string::npos);
    REQUIRE(out.find("Bob") != std::string::npos);
    REQUIRE(out.find("Charlie") == std::string::npos);
}

TEST_CASE("DELETE on RIGHT side before JOIN") {
    Database db;
    auto outs = runSQL(db, R"(CREATE TABLE users (id int, name str);
CREATE TABLE scores (uid int, points int);
INSERT INTO users  (id, name)   VALUES (1,"Alice"), (2,"Bob"), (3,"Charlie");
INSERT INTO scores (uid,points) VALUES (1,10), (1,15), (2,5), (3,0);
DELETE FROM scores WHERE uid = 2;
SELECT users.name, scores.points
FROM users
INNER JOIN scores ON users.id = scores.uid;)");
    auto out = lastSelectOutput(outs);

    REQUIRE(out.find("Alice") != std::string::npos);
    REQUIRE(out.find("10") != std::string::npos);
    REQUIRE(out.find("15") != std::string::npos);

    REQUIRE(out.find("Bob") == std::string::npos);
}

TEST_CASE("DELETE all scores (RIGHT side) -> empty INNER JOIN") {
    Database db;
    auto outs = runSQL(db, R"(CREATE TABLE users (id int, name str);
CREATE TABLE scores (uid int, points int);
INSERT INTO users  (id, name)   VALUES (1,"Alice"), (2,"Bob"), (3,"Charlie");
INSERT INTO scores (uid,points) VALUES (1,10), (1,15), (2,5), (3,0);
DELETE FROM scores;
SELECT users.name, scores.points
FROM users
INNER JOIN scores ON users.id = scores.uid;)");
    auto out = lastSelectOutput(outs);
    REQUIRE(out.find("Alice") == std::string::npos);
    REQUIRE(out.find("Bob") == std::string::npos);
    REQUIRE(out.find("Charlie") == std::string::npos);
}

TEST_CASE("DELETE on RIGHT side (filter points < 10) and LEFT side (id=3) then JOIN") {
    Database db;
    auto outs = runSQL(db, R"(CREATE TABLE users (id int, name str);
CREATE TABLE scores (uid int, points int);
INSERT INTO users  (id, name)   VALUES (1,"Alice"), (2,"Bob"), (3,"Charlie");
INSERT INTO scores (uid,points) VALUES (1,10), (1,15), (2,5), (3,0);
DELETE FROM users WHERE id = 3;
DELETE FROM scores WHERE points < 10;
SELECT users.name, scores.points
FROM users
INNER JOIN scores ON users.id = scores.uid;)");
    auto out = lastSelectOutput(outs);

    REQUIRE(out.find("Alice") != std::string::npos);
    REQUIRE(out.find("10") != std::string::npos);
    REQUIRE(out.find("15") != std::string::npos);
    REQUIRE(out.find("Bob") == std::string::npos);
    REQUIRE(out.find("Charlie") == std::string::npos);
    REQUIRE(out.find(" 5") == std::string::npos);
    REQUIRE(out.find(" 0") == std::string::npos);
}

TEST_CASE("DELETE on RIGHT side then JOIN") {
    Database db;
    auto outs = runSQL(db, R"(CREATE TABLE users (id int, name str);
CREATE TABLE scores (uid int, points int);
INSERT INTO users  (id, name)   VALUES (1,"Alice"), (2,"Bob"), (3,"Charlie");
INSERT INTO scores (uid,points) VALUES (1,10), (1,15), (2,5), (3,0);
DELETE FROM scores WHERE uid != 1;
SELECT users.name, scores.points
FROM users
INNER JOIN scores ON users.id = scores.uid;)");
    auto out = lastSelectOutput(outs);

    REQUIRE(out.find("Alice") != std::string::npos);
    REQUIRE(out.find("10") != std::string::npos);
    REQUIRE(out.find("15") != std::string::npos);
    REQUIRE(out.find("Bob") == std::string::npos);
    REQUIRE(out.find("Charlie") == std::string::npos);
}

TEST_CASE("DELETE with string join keys") {
    Database db;
    auto outs = runSQL(db, R"(CREATE TABLE users  (name str, city str);
CREATE TABLE badges (user_name str, badge str);
INSERT INTO users  (name, city) VALUES ("Alice","Paris"), ("Bob","Berlin");
INSERT INTO badges (user_name, badge) VALUES ("Alice","gold"), ("Bob","silver");
DELETE FROM badges WHERE badge = "silver";
SELECT users.name, badges.badge
FROM users
INNER JOIN badges ON users.name = badges.user_name;)");
    auto out = lastSelectOutput(outs);
    REQUIRE(out.find("Alice") != std::string::npos);
    REQUIRE(out.find("gold") != std::string::npos);
    REQUIRE(out.find("Bob") == std::string::npos);
    REQUIRE(out.find("silver") == std::string::npos);
}

TEST_CASE("DELETE+JOIN from separate queries") {
    Database db;
    auto outs = runSQL(db, R"(CREATE TABLE users  (id int, name str);
CREATE TABLE scores (uid int, points int);
INSERT INTO users  (id, name)   VALUES (1,"Alice"), (2,"Bob"), (3,"Charlie");
INSERT INTO scores (uid,points) VALUES (1,10), (1,15), (2,5), (3,0);
DELETE FROM users WHERE name = "Charlie";
SELECT users.name, scores.points
FROM users
INNER JOIN scores ON users.id = scores.uid;)");
    auto out = lastSelectOutput(outs);
    REQUIRE(out.find("Charlie") == std::string::npos);
    REQUIRE(out.find("Alice") != std::string::npos);
    REQUIRE(out.find("Bob") != std::string::npos);
}

TEST_CASE("DELETE affects only targeted table; other unchaned") {
    Database db;
    (void)runSQL(db, R"(CREATE TABLE users (id int, name str);
CREATE TABLE scores (uid int, points int);
INSERT INTO users  (id, name)   VALUES (1,"Alice"), (2,"Bob"), (3,"Charlie");
INSERT INTO scores (uid,points) VALUES (1,10), (2,5), (3,0);
DELETE FROM users WHERE id = 2;
SELECT * FROM users;
SELECT * FROM scores;)");

    const Table& users = db.table("users");
    REQUIRE(users.rows.size() == 2);

    bool hasAlice = false, hasCharlie = false, hasBob = false;
    for (auto& r : users.rows) {
        const auto& v = r.cells[1];
        if (std::holds_alternative<std::string>(v)) {
            auto s = std::get<std::string>(v);
            if (s == "Alice")
                hasAlice = true;
            if (s == "Charlie")
                hasCharlie = true;
            if (s == "Bob")
                hasBob = true;
        }
    }
    REQUIRE(hasAlice);
    REQUIRE(hasCharlie);
    REQUIRE_FALSE(hasBob);

    const Table& scores = db.table("scores");
    REQUIRE(scores.rows.size() == 3);
}

//  Error cases

TEST_CASE("DELETE from unknown table throws") {
    Database db;
    auto stmts = parse_all("DELETE FROM ghost;");
    REQUIRE(stmts.size() == 1);
    REQUIRE_THROWS(execute(db, stmts[0]));
}

TEST_CASE("DELETE WHERE unknown column throws") {
    Database db;

    (void)runSQL(db, R"(CREATE TABLE users (name str, age int);
INSERT INTO users (name, age) VALUES ("Alice",30);)");

    auto stmts = parse_all("DELETE FROM users WHERE nope = 1;");
    REQUIRE(stmts.size() == 1);
    REQUIRE_THROWS(execute(db, stmts[0]));
}

// JOIN tests

static std::vector<std::optional<std::string>> join_runSQL(Database& db, const std::string& sql) {
    auto stmts = parse_all(sql);
    std::vector<std::optional<std::string>> outs;
    outs.reserve(stmts.size());
    for (const auto& s : stmts)
        outs.push_back(execute(db, s));
    return outs;
}

static std::string join_lastSelectOutput(const std::vector<std::optional<std::string>>& outs) {
    for (auto it = outs.rbegin(); it != outs.rend(); ++it)
        if (it->has_value())
            return **it;
    return {};
}

TEST_CASE("JOIN basic: users and scores by id=uid, projection with qualified columns") {
    Database db;
    auto outs = join_runSQL(db, R"(CREATE TABLE users  (id int, name str);
CREATE TABLE scores (uid int, points int);
INSERT INTO users  (id, name)   VALUES (1,"Alice"), (2,"Bob");
INSERT INTO scores (uid,points) VALUES (1,10), (1,15), (2,5);
SELECT users.name, scores.points
FROM users
INNER JOIN scores ON users.id = scores.uid;)");
    auto out = join_lastSelectOutput(outs);

    REQUIRE(out.find("Alice") != std::string::npos);
    REQUIRE(out.find("10") != std::string::npos);
    REQUIRE(out.find("15") != std::string::npos);
    REQUIRE(out.find("Bob") != std::string::npos);
    REQUIRE(out.find("5") != std::string::npos);
}

TEST_CASE("JOIN with *: outputs all columns from both tables with qualified column names") {
    Database db;
    auto outs = join_runSQL(db, R"(CREATE TABLE users  (id int, name str);
CREATE TABLE scores (uid int, points int);
INSERT INTO users  (id, name)   VALUES (1,"Alice");
INSERT INTO scores (uid,points) VALUES (1,10);
SELECT *
FROM users
INNER JOIN scores ON users.id = scores.uid;)");
    auto out = join_lastSelectOutput(outs);

    REQUIRE(out.find("users.id") != std::string::npos);
    REQUIRE(out.find("users.name") != std::string::npos);
    REQUIRE(out.find("scores.uid") != std::string::npos);
    REQUIRE(out.find("scores.points") != std::string::npos);
}

TEST_CASE("JOIN requires qualified column names in SELECT list") {
    Database db;

    (void)join_runSQL(db, R"(CREATE TABLE users  (id int, name str);
CREATE TABLE scores (uid int, points int);
INSERT INTO users  (id, name)   VALUES (1,"Alice");
INSERT INTO scores (uid,points) VALUES (1,10);)");

    auto stmts = parse_all(R"(SELECT name, points
FROM users
INNER JOIN scores ON users.id = scores.uid;)");
    REQUIRE(stmts.size() == 1);
    REQUIRE_THROWS(execute(db, stmts[0]));
}

TEST_CASE("JOIN with no matching keys returns empty result") {
    Database db;
    auto outs = join_runSQL(db, R"(CREATE TABLE users  (id int, name str);
CREATE TABLE scores (uid int, points int);
INSERT INTO users  (id, name)   VALUES (1,"Alice");
INSERT INTO scores (uid,points) VALUES (2,99);
SELECT users.name, scores.points
FROM users
INNER JOIN scores ON users.id = scores.uid;)");
    auto out = join_lastSelectOutput(outs);

    REQUIRE(out.find("Alice") == std::string::npos);
    REQUIRE(out.find("99") == std::string::npos);
}

TEST_CASE("JOIN with string keys works") {
    Database db;
    auto outs = join_runSQL(db, R"(CREATE TABLE users  (name str, city str);
CREATE TABLE badges (user_name str, badge str);
INSERT INTO users  (name, city) VALUES ("Alice","Paris"), ("Bob","Berlin");
INSERT INTO badges (user_name, badge) VALUES ("Alice","gold"), ("Bob","silver");
SELECT users.name, badges.badge
FROM users
INNER JOIN badges ON users.name = badges.user_name;)");
    auto out = join_lastSelectOutput(outs);
    REQUIRE(out.find("Alice") != std::string::npos);
    REQUIRE(out.find("gold") != std::string::npos);
    REQUIRE(out.find("Bob") != std::string::npos);
    REQUIRE(out.find("silver") != std::string::npos);
}

TEST_CASE("JOIN after DELETE on one side filters rows seen by JOIN") {
    Database db;
    auto outs = join_runSQL(db, R"(CREATE TABLE users  (id int, name str);
CREATE TABLE scores (uid int, points int);
INSERT INTO users  (id, name)   VALUES (1,"Alice"), (2,"Bob"), (3,"Charlie");
INSERT INTO scores (uid,points) VALUES (1,10), (1,15), (2,5), (3,0);
DELETE FROM users WHERE name = "Charlie";
SELECT users.name, scores.points
FROM users
INNER JOIN scores ON users.id = scores.uid;)");
    auto out = join_lastSelectOutput(outs);
    REQUIRE(out.find("Alice") != std::string::npos);
    REQUIRE(out.find("Bob") != std::string::npos);
    REQUIRE(out.find("Charlie") == std::string::npos);
    REQUIRE(out.find(" 0") == std::string::npos);
}

TEST_CASE("JOIN unknown right table throws") {
    Database db;
    (void)join_runSQL(db, R"(CREATE TABLE users (id int, name str);
INSERT INTO users (id, name) VALUES (1,"Alice");)");
    auto stmts = parse_all(R"(SELECT users.name, ghost.points
FROM users
INNER JOIN ghost ON users.id = ghost.uid;)");
    REQUIRE(stmts.size() == 1);
    REQUIRE_THROWS(execute(db, stmts[0]));
}

TEST_CASE("JOIN unknown columns in ON clause throw") {
    Database db;
    (void)join_runSQL(db, R"(CREATE TABLE users  (id int, name str);
CREATE TABLE scores (uid int, points int);
INSERT INTO users  (id, name)   VALUES (1,"Alice");
INSERT INTO scores (uid,points) VALUES (1,10);)");

    auto s1 = parse_all(R"(SELECT users.name, scores.points
FROM users
INNER JOIN scores ON users.nope = scores.uid;)");
    REQUIRE(s1.size() == 1);
    REQUIRE_THROWS(execute(db, s1[0]));

    auto s2 = parse_all(R"(SELECT users.name, scores.points
FROM users
INNER JOIN scores ON users.id = scores.nope;)");
    REQUIRE(s2.size() == 1);
    REQUIRE_THROWS(execute(db, s2[0]));
}

TEST_CASE("JOIN with type mismatch in keys gives empty result (no crash)") {
    Database db;
    auto outs = join_runSQL(db, R"(CREATE TABLE L (id int);
CREATE TABLE R (id str);
INSERT INTO L (id) VALUES (1);
INSERT INTO R (id) VALUES ("1");
SELECT *
FROM L
INNER JOIN R ON L.id = R.id;)");
    auto out = join_lastSelectOutput(outs);

    REQUIRE(out.find("1") == std::string::npos);
}

TEST_CASE("JOIN preserves one-to-many multiplicity on right side") {
    Database db;
    auto outs = join_runSQL(db, R"(CREATE TABLE users  (id int, name str);
CREATE TABLE scores (uid int, points int);
INSERT INTO users  (id, name)   VALUES (1,"Alice");
INSERT INTO scores (uid,points) VALUES (1,10), (1,11), (1,12);
SELECT users.name, scores.points
FROM users
INNER JOIN scores ON users.id = scores.uid;)");
    auto out = join_lastSelectOutput(outs);
    REQUIRE(out.find("Alice") != std::string::npos);
    REQUIRE(out.find("10") != std::string::npos);
    REQUIRE(out.find("11") != std::string::npos);
    REQUIRE(out.find("12") != std::string::npos);
}

// WHERE tests
static std::vector<std::optional<std::string>> where_runSQL(Database& db, const std::string& sql) {
    auto stmts = parse_all(sql);
    std::vector<std::optional<std::string>> outs;
    outs.reserve(stmts.size());
    for (const auto& s : stmts)
        outs.push_back(execute(db, s));
    return outs;
}

static std::string where_lastSelectOutput(const std::vector<std::optional<std::string>>& outs) {
    for (auto it = outs.rbegin(); it != outs.rend(); ++it)
        if (it->has_value())
            return **it;
    return {};
}

static void seed_users(Database& db) {
    where_runSQL(db, R"(CREATE TABLE users (name str, age int);
INSERT INTO users (name, age) VALUES ("Alice",30), ("Bob",25), ("Charlie",20);)");
}

TEST_CASE("WHERE int equality (=)") {
    Database db;
    seed_users(db);
    auto outs = where_runSQL(db, R"(SELECT name FROM users WHERE age = 25;)");
    auto out = where_lastSelectOutput(outs);
    REQUIRE(out.find("Bob") != std::string::npos);
    REQUIRE(out.find("Alice") == std::string::npos);
    REQUIRE(out.find("Charlie") == std::string::npos);
}

TEST_CASE("WHERE string equality (name = \"Alice\")") {
    Database db;
    seed_users(db);
    auto outs = where_runSQL(db, R"(SELECT age FROM users WHERE name = "Alice";)");
    auto out = where_lastSelectOutput(outs);
    REQUIRE(out.find("30") != std::string::npos);
    REQUIRE(out.find("25") == std::string::npos);
    REQUIRE(out.find("20") == std::string::npos);
}

TEST_CASE("WHERE string inequality (name != \"Alice\")") {
    Database db;
    seed_users(db);
    auto outs = where_runSQL(db, R"(SELECT name FROM users WHERE name != "Alice";)");
    auto out = where_lastSelectOutput(outs);
    REQUIRE(out.find("Bob") != std::string::npos);
    REQUIRE(out.find("Charlie") != std::string::npos);
    REQUIRE(out.find("Alice") == std::string::npos);
}

TEST_CASE("WHERE int ordering: <, <=, >, >=") {
    Database db;
    seed_users(db);

    auto o1 = where_lastSelectOutput(where_runSQL(db, R"(SELECT name FROM users WHERE age < 25;)"));
    REQUIRE(o1.find("Charlie") != std::string::npos);
    REQUIRE(o1.find("Alice") == std::string::npos);
    REQUIRE(o1.find("Bob") == std::string::npos);

    auto o2 =
        where_lastSelectOutput(where_runSQL(db, R"(SELECT name FROM users WHERE age <= 25;)"));
    REQUIRE(o2.find("Charlie") != std::string::npos);
    REQUIRE(o2.find("Bob") != std::string::npos);
    REQUIRE(o2.find("Alice") == std::string::npos);

    auto o3 = where_lastSelectOutput(where_runSQL(db, R"(SELECT name FROM users WHERE age > 25;)"));
    REQUIRE(o3.find("Alice") != std::string::npos);
    REQUIRE(o3.find("Bob") == std::string::npos);
    REQUIRE(o3.find("Charlie") == std::string::npos);

    auto o4 =
        where_lastSelectOutput(where_runSQL(db, R"(SELECT name FROM users WHERE age >= 25;)"));
    REQUIRE(o4.find("Alice") != std::string::npos);
    REQUIRE(o4.find("Bob") != std::string::npos);
    REQUIRE(o4.find("Charlie") == std::string::npos);
}

TEST_CASE("DELETE with WHERE") {
    Database db;
    seed_users(db);
    where_runSQL(db, R"(DELETE FROM users WHERE age < 25;)");
    auto out = where_lastSelectOutput(where_runSQL(db, R"(SELECT * FROM users;)"));
    REQUIRE(out.find("Charlie") == std::string::npos);
    REQUIRE(out.find("Alice") != std::string::npos);
    REQUIRE(out.find("Bob") != std::string::npos);
}

TEST_CASE("UPDATE with WHERE") {
    Database db;
    seed_users(db);
    where_runSQL(db, R"(UPDATE users SET age = 99 WHERE name = "Bob";)");
    auto out = where_lastSelectOutput(where_runSQL(db, R"(SELECT * FROM users;)"));

    REQUIRE(out.find("Bob") != std::string::npos);
    REQUIRE(out.find("99") != std::string::npos);
    REQUIRE(out.find("Alice") != std::string::npos);
    REQUIRE(out.find("30") != std::string::npos);
}

TEST_CASE("WHERE type mismatch throws (age = \"30\")") {
    Database db;
    seed_users(db);
    auto stmts = parse_all(R"(SELECT * FROM users WHERE age = "30";)");
    REQUIRE(stmts.size() == 1);
    REQUIRE_THROWS(execute(db, stmts[0]));
}

TEST_CASE("WHERE ordering on strings (<, <=, >, >=) is not allowed") {
    Database db;
    seed_users(db);

    auto s1 = parse_all(R"(SELECT * FROM users WHERE name < "M";)");
    REQUIRE(s1.size() == 1);
    REQUIRE_THROWS(execute(db, s1[0]));

    auto s2 = parse_all(R"(SELECT * FROM users WHERE name >= "A";)");
    REQUIRE(s2.size() == 1);
    REQUIRE_THROWS(execute(db, s2[0]));
}

TEST_CASE("WHERE unknown column throws") {
    Database db;
    seed_users(db);
    auto stmts = parse_all(R"(SELECT * FROM users WHERE nope = 1;)");
    REQUIRE(stmts.size() == 1);
    REQUIRE_THROWS(execute(db, stmts[0]));
}

TEST_CASE("Quoted semicolons do not split statements; WHERE can match such strings") {
    Database db;
    auto outs = where_runSQL(db, R"(CREATE TABLE t (name str, age int);
INSERT INTO t (name, age) VALUES ("Al;ice", 31), ("Bob", 22);
SELECT * FROM t WHERE name = "Al;ice";)");
    auto out = where_lastSelectOutput(outs);
    REQUIRE(out.find("Al;ice") != std::string::npos);
    REQUIRE(out.find("31") != std::string::npos);
    REQUIRE(out.find("Bob") == std::string::npos);
}

static void seed_users_scores(Database& db) {
    auto outs = parse_all(R"(CREATE TABLE users (id int, name str);
CREATE TABLE scores (uid int, points int);
INSERT INTO users (id, name) VALUES (1,"Alice"), (2,"Bob"), (3,"Charlie");
INSERT INTO scores (uid, points) VALUES (1,10), (1,15), (2,5), (3,0);
)");
    for (const auto& st : outs)
        execute(db, st);
}

//  UPDATE tests

TEST_CASE("UPDATE all rows: SET int literal") {
    Database db;
    seed_users(db);
    auto outs = parse_all(R"(UPDATE users SET age=99;
SELECT name, age FROM users;)");
    std::optional<std::string> last_out;
    for (const auto& st : outs)
        last_out = execute(db, st);
    REQUIRE(last_out.has_value());
    REQUIRE(last_out->find("99") != std::string::npos);
}

TEST_CASE("UPDATE chain of multiple statements") {
    Database db;
    seed_users(db);
    auto outs = parse_all(R"(UPDATE users SET age=26 WHERE name="Bob";
UPDATE users SET name="Charles" WHERE name="Charlie";
SELECT name, age FROM users;)");
    std::optional<std::string> last_out;
    for (const auto& st : outs)
        last_out = execute(db, st);
    REQUIRE(last_out.has_value());
    REQUIRE(last_out->find("Alice") != std::string::npos);
    REQUIRE(last_out->find("26") != std::string::npos);
    REQUIRE(last_out->find("Charles") != std::string::npos);
}

TEST_CASE("UPDATE multiple columns in one statement") {
    Database db;
    seed_users(db);
    auto outs = parse_all(R"(UPDATE users SET name="Alicia", age=31 WHERE name="Alice";
SELECT name, age FROM users;)");
    std::optional<std::string> last_out;
    for (const auto& st : outs)
        last_out = execute(db, st);
    REQUIRE(last_out.has_value());
    REQUIRE(last_out->find("Alicia") != std::string::npos);
    REQUIRE(last_out->find("31") != std::string::npos);
}

TEST_CASE("UPDATE type mismatch throws") {
    Database db;
    seed_users(db);
    auto outs = parse_all(R"(UPDATE users SET age="oops";)");
    REQUIRE_THROWS(execute(db, outs[0]));
}

TEST_CASE("UPDATE unknown column in SET throws") {
    Database db;
    seed_users(db);
    auto outs = parse_all(R"(UPDATE users SET nope=1;)");
    REQUIRE_THROWS(execute(db, outs[0]));
}

TEST_CASE("UPDATE unknown column in WHERE throws") {
    Database db;
    seed_users(db);
    auto outs = parse_all(R"(UPDATE users SET age=20 WHERE nope=1;)");
    REQUIRE_THROWS(execute(db, outs[0]));
}

TEST_CASE("UPDATE WHERE eq (string)") {
    Database db;
    seed_users(db);
    auto outs = parse_all(R"(UPDATE users SET name="Robert" WHERE name="Bob";
SELECT name FROM users;)");
    std::optional<std::string> last_out;
    for (const auto& st : outs)
        last_out = execute(db, st);
    REQUIRE(last_out.has_value());
    REQUIRE(last_out->find("Robert") != std::string::npos);
    REQUIRE(last_out->find("Bob") == std::string::npos);
}

TEST_CASE("UPDATE WHERE >= (int)") {
    Database db;
    seed_users(db);
    auto outs = parse_all(R"(UPDATE users SET age=40 WHERE age >= 30;
SELECT name, age FROM users;)");
    std::optional<std::string> last_out;
    for (const auto& st : outs)
        last_out = execute(db, st);
    REQUIRE(last_out.has_value());
    REQUIRE(last_out->find("Alice") != std::string::npos);
    REQUIRE(last_out->find("40") != std::string::npos);
    REQUIRE(last_out->find("Bob") != std::string::npos);
    REQUIRE(last_out->find("Charlie") != std::string::npos);
}

TEST_CASE("UPDATE WHERE no match leaves table unchanged") {
    Database db;
    seed_users(db);
    auto outs = parse_all(R"(UPDATE users SET age=123 WHERE age < 10;
SELECT name, age FROM users;)");
    std::optional<std::string> last_out;
    for (const auto& st : outs)
        last_out = execute(db, st);
    REQUIRE(last_out.has_value());
    REQUIRE(last_out->find("30") != std::string::npos);
    REQUIRE(last_out->find("25") != std::string::npos);
    REQUIRE(last_out->find("20") != std::string::npos);
}

TEST_CASE("UPDATE on right table affects JOIN result") {
    Database db;
    seed_users_scores(db);
    auto outs = parse_all(R"(UPDATE scores SET points=50 WHERE uid=2;
SELECT users.name, scores.points FROM users INNER JOIN scores ON users.id = scores.uid;)");
    std::optional<std::string> last_out;
    for (const auto& st : outs)
        last_out = execute(db, st);
    REQUIRE(last_out.has_value());
    REQUIRE(last_out->find("Alice") != std::string::npos);
    REQUIRE(last_out->find("10") != std::string::npos);
    REQUIRE(last_out->find("15") != std::string::npos);
    REQUIRE(last_out->find("Bob") != std::string::npos);
    REQUIRE(last_out->find("50") != std::string::npos);
}

TEST_CASE("UPDATE with string key on joined table") {
    Database db;
    seed_users(db);
    auto outs = parse_all(R"(CREATE TABLE badges (uid str, badge str);
INSERT INTO badges(uid,badge) VALUES ("Alice","gold"), ("Bob","silver");
UPDATE badges SET badge="platinum" WHERE uid="Bob";
SELECT users.name, badges.badge FROM users INNER JOIN badges ON users.name = badges.uid;)");
    std::optional<std::string> last_out;
    for (const auto& st : outs)
        last_out = execute(db, st);
    REQUIRE(last_out.has_value());
    REQUIRE(last_out->find("gold") != std::string::npos);
    REQUIRE(last_out->find("platinum") != std::string::npos);
}

TEST_CASE("SELECT WHERE with negative numbers") {
    Database db;
    auto stmts = parse_all(R"(
        CREATE TABLE temps (city str, c int);
        INSERT INTO temps (city, c) VALUES ("Oslo",-5), ("Cairo",25), ("Minsk",-1);
        SELECT * FROM temps WHERE c < 0;
    )");
    std::optional<std::string> last_out;
    for (const auto& st : stmts)
        last_out = execute(db, st);
    REQUIRE(last_out.has_value());
    REQUIRE(last_out->find("Oslo") != std::string::npos);
    REQUIRE(last_out->find("Minsk") != std::string::npos);
    REQUIRE(last_out->find("Cairo") == std::string::npos);
}

TEST_CASE("SELECT WHERE with quoted string containing spaces") {
    Database db;
    auto stmts = parse_all(R"(
        CREATE TABLE users (name str, age int);
        INSERT INTO users (name, age) VALUES ("Alice Smith",31), ("Bob",25);
        SELECT * FROM users WHERE name = "Alice Smith";
    )");
    std::optional<std::string> last_out;
    for (const auto& st : stmts)
        last_out = execute(db, st);
    REQUIRE(last_out.has_value());
    REQUIRE(last_out->find("Alice Smith") != std::string::npos);
    REQUIRE(last_out->find("31") != std::string::npos);
}

TEST_CASE("UPDATE no-op is valid and leaves values unchanged") {
    Database db;
    auto stmts = parse_all(R"(
        CREATE TABLE u_noop (name str, age int);
        INSERT INTO u_noop VALUES ("Alice",30), ("Bob",25);
        UPDATE u_noop SET age = 25 WHERE name = "Bob";
        SELECT * FROM u_noop;
    )");
    std::optional<std::string> last_out;
    for (const auto& st : stmts)
        last_out = execute(db, st);
    REQUIRE(last_out.has_value());
    REQUIRE(last_out->find("Alice") != std::string::npos);
    REQUIRE(last_out->find("30") != std::string::npos);
    REQUIRE(last_out->find("Bob") != std::string::npos);
    REQUIRE(last_out->find("25") != std::string::npos);
}

TEST_CASE("UPDATE with wrong value type throws") {
    Database db;
    auto stmts = parse_all(R"(
        CREATE TABLE u_type (name str, age int);
        INSERT INTO u_type VALUES ("Alice",30);
        UPDATE u_type SET age = "x";
    )");

    REQUIRE_THROWS(execute(db, stmts[2]));
}

TEST_CASE("UPDATE WHERE type mismatch throws") {
    Database db;
    auto stmts = parse_all(R"(
        CREATE TABLE u_where_type (age int);
        INSERT INTO u_where_type VALUES (20), (25);
        UPDATE u_where_type SET age = 99 WHERE age = "25";
    )");
    REQUIRE_THROWS(execute(db, stmts[2]));
}

TEST_CASE("SELECT unknown column in projection throws") {
    Database db;
    auto stmts = parse_all(R"(
        CREATE TABLE s_proj (id int, name str);
        INSERT INTO s_proj VALUES (1,"alice");
        SELECT nope FROM s_proj;
    )");
    REQUIRE_THROWS(execute(db, stmts[2]));
}

TEST_CASE("JOIN ambiguous unqualified projection throws") {
    Database db;
    auto stmts = parse_all(R"(
        CREATE TABLE a2 (id int, name str);
        CREATE TABLE b2 (id int, city str);
        INSERT INTO a2 VALUES (1,"alice");
        INSERT INTO b2 VALUES (1,"oslo");
        SELECT id FROM a2 INNER JOIN b2 ON a2.id = b2.id;
    )");
    REQUIRE_THROWS(execute(db, stmts[4]));
}

TEST_CASE("JOIN unknown column in projection throws") {
    Database db;
    auto stmts = parse_all(R"(
        CREATE TABLE a3 (id int, name str);
        CREATE TABLE b3 (uid int, city str);
        INSERT INTO a3 VALUES (1,"alice");
        INSERT INTO b3 VALUES (1,"oslo");
        SELECT a3.nope, b3.city FROM a3 INNER JOIN b3 ON a3.id = b3.uid;
    )");
    REQUIRE_THROWS(execute(db, stmts[4]));
}
