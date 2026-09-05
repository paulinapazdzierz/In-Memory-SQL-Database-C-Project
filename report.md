This is a in-memory project: that parses and executes the SQL queries. It supports: 
-CREATE TABLE- creates the table
-INSERT INTO- inserts, adds rows, works for both:
    -INSERT INTO temps (city, c) VALUES ("Oslo",-5) syntax 
    -CREATE TABLE temps (city, c)
    -INSERT INTO temps VALUES ("Oslo",-5) syntax
-SELECT- selects, projects the columns,
-DELETE FROM- delets rows,
-INNER JOIN- combines the rows that got the matching keys
-WHERE that works with: =, !=, <, >, <=, >=
Results are formatted as ASCII tables.

Besides, the operations work well with each other, sample test from the tests.cpp file: 
name of the test case: DELETE+JOIN from separate queries
    "CREATE TABLE users  (id int, name str);
    CREATE TABLE scores (uid int, points int);
    INSERT INTO users  (id, name)   VALUES (1,"Alice"), (2,"Bob"), (3,"Charlie");
    INSERT INTO scores (uid,points) VALUES (1,10), (1,15), (2,5), (3,0);
    DELETE FROM users WHERE name = "Charlie";
    SELECT users.name, scores.points
    FROM users
    INNER JOIN scores ON users.id = scores.uid;"

structure of the project:
- all header files are put in the "include" folder
- the actual code is in the "src" folder
    -db.cpp implements the model for the data
    -exec.cpp executes the SQL statements
    -format.cpp resposible for the formating of the results as ASCII table
    -parser.cpp imlements the parser
- there is a library part: lib.cpp/lib.hpp
- cli.cpp is the file run to process the SQL scripts


In the tests.cpp file there are 57 test cases for each of the operators, and for chekcing if they work well used with each other. All of them passed while run.

There are exceptions thrown for parsing and execution for invalid syntax or semantics.