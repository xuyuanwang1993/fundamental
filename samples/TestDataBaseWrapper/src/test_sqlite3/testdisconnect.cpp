#include <iostream>
#include "database/sqlite3/sqlite3pp.hpp"
#include <gtest/gtest.h>

using namespace std;

TEST(test_sqlite3, testdisconnect)
{
  try {
    sqlite3pp::database db("test.db");
    {
      sqlite3pp::transaction xct(db);
      {
	sqlite3pp::command cmd(db, "INSERT INTO contacts (name, phone) VALUES ('AAAA', '1234')");

	cout << cmd.execute() << endl;
      }
    }
    cout << db.disconnect() << endl;

  }
  catch (exception& ex) {
    cout << ex.what() << endl;
  }

}
