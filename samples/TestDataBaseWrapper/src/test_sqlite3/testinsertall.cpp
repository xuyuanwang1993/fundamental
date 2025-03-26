#include <iostream>
#include "database/sqlite3/sqlite3pp.hpp"
#include <gtest/gtest.h>
using namespace std;

TEST(test_sqlite3, testinsertall)
{
  try {
    sqlite3pp::database db("test.db");
    {
      sqlite3pp::transaction xct(db);
      {
	sqlite3pp::command cmd(db,
			       "INSERT INTO contacts (name, phone) VALUES (:name, '1234');"
			       "INSERT INTO contacts (name, phone) VALUES (:name, '5678');"
			       "INSERT INTO contacts (name, phone) VALUES (:name, '9012');"
			       );
	{
	  cout << cmd.bind(":name", "user", sqlite3pp::copy) << endl;
	  cout << cmd.execute_all() << endl;
	}
      }
      xct.commit();
    }
  }
  catch (exception& ex) {
    cout << ex.what() << endl;
  }

}
