# In-Memory SQL Database (C++)

A small in-memory SQL engine written in C++23. It reads an SQL script, parses each statement into an AST, executes it against tables held in memory, and prints `SELECT` results as ASCII tables.

## Features

| Statement | Supported syntax |
|---|---|
| `CREATE TABLE` | `CREATE TABLE users (id int, name str);` |
| `INSERT INTO` | With a column list: `INSERT INTO users (id, name) VALUES (1,"Alice"), (2,"Bob");`<br>Without one: `INSERT INTO users VALUES (3,"Charlie");` |
| `SELECT` | `SELECT * FROM users;`<br>`SELECT name FROM users WHERE id >= 2;` |
| `INNER JOIN` | `SELECT users.name, scores.points FROM users INNER JOIN scores ON users.id = scores.uid;` |
| `UPDATE` | `UPDATE users SET name = "Bobby" WHERE id = 2;` |
| `DELETE FROM` | `DELETE FROM users WHERE name = "Charlie";` or `DELETE FROM users;` |

`WHERE` accepts one condition using `=`, `!=`, `<`, `>`, `<=` or `>=`.

### Types and rules

- Column types are `int` and `str`.
- String literals use double quotes: `"Alice"`.
- Statements are separated by `;`.
- Keywords must be written in uppercase.
- When you `INSERT` with a column list, columns you leave out get a default value: `0` for `int`, `""` for `str`.
- Values are type-checked on `INSERT`, `UPDATE` and `WHERE`. The ordering operators (`<`, `>`, `<=`, `>=`) work only on `int` columns.
- In a `JOIN`, the `ON` columns and the selected columns must be qualified as `table.column`. `SELECT *` with a join returns every column from both tables.
- Invalid syntax and invalid semantics (unknown table or column, type mismatch, duplicate table, and so on) throw an exception. The CLI prints the message as `Error: ...` and exits with code `1`.

### Current limitations

- `WHERE` takes a single condition. `AND` and `OR` are not supported.
- `WHERE` cannot be combined with `INNER JOIN`.
- `ORDER BY`, `GROUP BY`, aggregates, `DROP TABLE` and persistence to disk are not implemented.

## Example

`example.sql`:

```sql
CREATE TABLE users  (id int, name str);
CREATE TABLE scores (uid int, points int);
INSERT INTO users  (id, name)   VALUES (1,"Alice"), (2,"Bob"), (3,"Charlie");
INSERT INTO scores (uid,points) VALUES (1,10), (1,15), (2,5), (3,0);
DELETE FROM users WHERE name = "Charlie";
SELECT users.name, scores.points
FROM users
INNER JOIN scores ON users.id = scores.uid;
```

Output:

```
+------------+---------------+
| users.name | scores.points |
+------------+---------------+
| Alice      | 10            |
+------------+---------------+
| Alice      | 15            |
+------------+---------------+
| Bob        | 5             |
+------------+---------------+
```

## Project structure

```
.
├── include/            Headers
│   ├── ast.hpp         AST: statements, values, types, WHERE expressions
│   ├── db.hpp          Data model: Database, Table, Row
│   ├── exec.hpp        Statement executor
│   ├── format.hpp      ASCII table and CSV formatting
│   └── parser.hpp      SQL parser
├── src/
│   ├── db.cpp          Table storage and lookup
│   ├── exec.cpp        Runs statements against the database
│   ├── format.cpp      Renders results as ASCII tables (CSV helper included)
│   └── parser.cpp      Splits the script into statements and parses them into the AST
├── lib.cpp / lib.hpp   Library API: mylib::run_script(sql), mylib::run_file(path)
├── cli.cpp             Command-line tool
├── gui.cpp             Qt GUI stub (optional)
├── tests.cpp           Catch2 unit tests
├── tests2.cpp          GoogleTest placeholder
├── CMakeLists.txt
└── CMakePresets.json   Visual Studio 2022 + vcpkg preset
```

## Building

Requirements:

- CMake 3.21 or newer
- A C++23 compiler (MSVC 2022, GCC 13+ or Clang 17+)
- [Catch2 v3](https://github.com/catchorg/Catch2) and [GoogleTest](https://github.com/google/googletest), only if you build the tests (on by default)
- Qt 6, only if you build the GUI (off by default)

### Windows (Visual Studio 2022 + vcpkg)

The preset expects vcpkg at `C:/vcpkg`. If yours is elsewhere, change `CMAKE_TOOLCHAIN_FILE` in `CMakePresets.json`.

```powershell
vcpkg install catch2 gtest --triplet x64-windows
cmake --preset vs2022-x64-vcpkg
cmake --build --preset debug
```

### Linux / macOS

```bash
cmake -S . -B build
cmake --build build
```

To build without tests, or without the sanitizers (ASan/UBSan are on by default for GCC and Clang):

```bash
cmake -S . -B build -DBUILD_TESTS=OFF -DPROJECT_USE_SANITIZERS=OFF
```

CMake options:

| Option | Default | Description |
|---|---|---|
| `BUILD_TESTS` | `ON` | Build the Catch2 and GoogleTest targets |
| `BUILD_GUI` | `OFF` | Build the Qt GUI target |
| `PROJECT_USE_SANITIZERS` | `ON` | Turn on AddressSanitizer and UndefinedBehaviorSanitizer (not used with MSVC) |

## Usage

The CLI executable is `my_cli_tool`. Pass it an SQL file:

```bash
./build/my_cli_tool example.sql
```

Or pipe the script through standard input:

```bash
./build/my_cli_tool < example.sql
```

A UTF-8 BOM at the start of the file is stripped automatically.

### Using the library

```cpp
#include "lib.hpp"
#include <iostream>

int main() {
    std::cout << mylib::run_script(R"(
        CREATE TABLE t (id int, name str);
        INSERT INTO t VALUES (1, "Alice");
        SELECT * FROM t;
    )");
}
```

## Tests

`tests.cpp` holds 57 Catch2 test cases. They cover each statement on its own, statements combined (for example `DELETE` followed by `JOIN`), and error handling.

```bash
ctest --test-dir build --output-on-failure
```

On Windows with the preset:

```powershell
ctest --preset ctest-debug
```
