#include "database/sqlite3/sqlite3ppext.hpp"
#include "test_sqlite3_common.hpp"

#include <gtest/gtest.h>
#include <iostream>
#include <string>

using namespace std;

void step0(sqlite3pp::ext::context& c) {
    int* sum = (int*)c.aggregate_data(sizeof(int));

    *sum += c.get<int>(0);
}
void finalize0(sqlite3pp::ext::context& c) {
    int* sum = (int*)c.aggregate_data(sizeof(int));
    c.result(*sum);
}

void step1(sqlite3pp::ext::context& c) {
    string* sum = (string*)c.aggregate_data(sizeof(string));

    if (c.aggregate_count() == 1) {
        new (sum) string;
    }

    *sum += c.get<string>(0);
}
void finalize1(sqlite3pp::ext::context& c) {
    string* sum = (string*)c.aggregate_data(sizeof(string));
    c.result(*sum);

    sum->~string();
}

template <class T>
struct mysum {
    mysum() {
        s_ = T();
    }
    void step(T s) {
        s_ += s;
    }
    T finish() {
        return s_;
    }
    T s_;
};

struct mycnt {
    void step() {
        ++n_;
    }
    int finish() {
        return n_;
    }
    int n_;
};

struct strcnt {
    void step(string const& s) {
        s_ += s;
    }
    int finish() {
        return static_cast<int>(s_.size());
    }
    string s_;
};

struct plussum {
    void step(int n1, int n2) {
        n_ += n1 + n2;
    }
    int finish() {
        return n_;
    }
    int n_;
};

TEST(test_sqlite3, testaggregate) {
    try {
        auto db = contacts_db();
        default_init_contacts_db(db);

        sqlite3pp::ext::aggregate aggr(db);
        EXPECT_EQ(aggr.create("a0", &step0, &finalize0), 0);
        EXPECT_EQ(aggr.create("a1", &step1, &finalize1), 0);
        EXPECT_EQ((aggr.create<mysum<string>, string>("a2")), 0);
        EXPECT_EQ((aggr.create<mysum<int>, int>("a3")), 0);
        EXPECT_EQ(aggr.create<mycnt>("a4"), 0);
        EXPECT_EQ((aggr.create<strcnt, string>("a5")), 0);
        EXPECT_EQ((aggr.create<plussum, int, int>("a6")), 0);

        sqlite3pp::query qry(
            db, "SELECT a0(id), a1(name), a2(name), a3(id), a4(), a5(name), sum(id), a6(age, level) FROM contacts");

        for (int i = 0; i < qry.column_count(); ++i) {
            cout << qry.column_name(i) << "\t";
        }
        cout << endl;
        for (sqlite3pp::query::iterator i = qry.begin(); i != qry.end(); ++i) {
            for (int j = 0; j < qry.column_count(); ++j) {
                cout << (*i).get<char const*>(j) << "\t";
            }
            cout << endl;
        }
        cout << endl;
    } catch (exception& ex) {
        cout << ex.what() << endl;
    }
}
