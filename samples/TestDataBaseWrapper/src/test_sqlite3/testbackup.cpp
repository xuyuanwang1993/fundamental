#include <iostream>
#include "database/sqlite3/sqlite3pp.hpp"
#include <gtest/gtest.h>
using namespace std;

TEST(test_sqlite3, testbackup)
{
  try {
    sqlite3pp::database db("test.db");
    sqlite3pp::database backupdb("backup.db");

    db.backup(
      backupdb,
      [](int pagecount, int remaining, int rc) {
        cout << pagecount << "/" << remaining << endl;
        if (rc == SQLITE_OK || rc == SQLITE_BUSY || rc == SQLITE_LOCKED) {
          // sleep(250);
        }
      });
  }
  catch (exception& ex) {
    cout << ex.what() << endl;
  }

}
