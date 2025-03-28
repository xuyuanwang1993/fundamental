#include "database/sqlite3/sqlite.hpp"
#include "test_sqlite3_common.hpp"
#include <gtest/gtest.h>
#include <iostream>
using namespace std;

TEST(test_sqlite3, test_custom) {
    try {
        sqlite::database db;
        db.execute(R"""(
            CREATE TABLE IF NOT EXISTS pir (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            h1 INTEGER NOT NULL,
            h2 INTEGER NOT NULL,
            data BLOB NOT NULL,
            UNIQUE(h1, h2) 
        );
  )""");
        {
            sqlite::command cmd(db, "INSERT  INTO pir (h1, h2, data) VALUES (?, ?, ?)");
            std::int32_t h1 = 0;
            std::int32_t h2 = 1;
            std::string v   = "test1";
            cmd.bind(1, h1);
            cmd.bind(2, h2);
            cmd.bind(3, v.data(), v.size(), sqlite::copy_semantic::copy);
            EXPECT_EQ(cmd.execute(), 0);
            EXPECT_EQ(db.changes(), 1);
        }
        {
            sqlite::command cmd(db, "INSERT INTO pir (h1, h2, data) VALUES (?, ?, ?)");
            std::int32_t h1 = 0;
            std::int32_t h2 = 1;
            std::string v   = "test1";
            cmd.bind(1, h1);
            cmd.bind(2, h2);
            cmd.bind(3, v.data(), v.size(), sqlite::copy_semantic::copy);
            EXPECT_EQ(cmd.execute(), SQLITE_CONSTRAINT);
            EXPECT_EQ(db.changes(), 0);
        }
        {
            sqlite::command cmd(db, "INSERT  INTO pir (h1, h2, data) VALUES (?, ?, ?)");
            std::int32_t h1 = 0;
            std::int32_t h2 = 2;
            std::string v   = "test2";
            cmd.bind(1, h1);
            cmd.bind(2, h2);
            cmd.bind(3, v.data(), v.size(), sqlite::copy_semantic::copy);
            EXPECT_EQ(cmd.execute(), 0);
            EXPECT_EQ(db.changes(), 1);
        }
        {
            sqlite::command cmd(db, "INSERT  INTO pir (h1, h2, data) VALUES (?, ?, ?)");
            std::int32_t h1 = 1;
            std::int32_t h2 = 2;
            std::string v   = "test3";
            cmd.bind(1, h1);
            cmd.bind(2, h2);
            cmd.bind(3, v.data(), v.size(), sqlite::copy_semantic::copy);
            EXPECT_EQ(cmd.execute(), 0);
            EXPECT_EQ(db.changes(), 1);
        }
        {
            sqlite::query qry(db, "SELECT * from pir");
            auto iter = qry.begin();
            while (iter != qry.end()) {
                auto row = *iter;
                EXPECT_EQ(row.data_count(), 4);
                auto id        = row.get<std::int32_t>(0);
                auto h1        = row.get<std::int32_t>(1);
                auto h2        = row.get<std::int32_t>(2);
                auto data_size = row.column_bytes(3);
                auto data      = row.get<const void*>(3);
                std::string data_str;
                data_str.resize(data_size);
                std::memcpy(data_str.data(), data, data_size);
                FINFO("{} {} {} {}", id, h1, h2, data_str);
                ++iter;
            }
        }
        {
            sqlite::query qry(db, "SELECT COUNT(*) from pir");
            auto iter = qry.begin();
            EXPECT_TRUE(iter != qry.end());
            auto row = *iter;
            EXPECT_EQ((row.get<std::int32_t>(0)), 3);
        }
        {
            sqlite::query qry(db, "SELECT data FROM pir WHERE id BETWEEN ? AND ?;");
            qry.bind(1, 2);
            qry.bind(2, 3);

            auto iter = qry.begin();
            while (iter != qry.end()) {
                auto row = *iter;
                EXPECT_EQ(row.data_count(), 1);
                auto data_size = row.column_bytes(0);
                auto data      = row.get<const void*>(0);
                std::string data_str;
                data_str.resize(data_size);
                std::memcpy(data_str.data(), data, data_size);
                FINFO("data->{}", data_str);
                ++iter;
            }
        }
        {
            sqlite::query qry(db, "SELECT data FROM pir WHERE h1 = ? AND h2 = ?;");
            qry.bind(1, 1);
            qry.bind(2, 2);

            auto iter = qry.begin();
            while (iter != qry.end()) {
                auto row = *iter;
                EXPECT_EQ(row.data_count(), 1);
                auto data_size = row.column_bytes(0);
                auto data      = row.get<const void*>(0);
                std::string data_str;
                data_str.resize(data_size);
                std::memcpy(data_str.data(), data, data_size);
                EXPECT_EQ("test3", data_str);
                ++iter;
            }
        }

    } catch (exception& ex) {
        FERR("ex:{}", ex.what());
        EXPECT_TRUE(false);
    }
}
