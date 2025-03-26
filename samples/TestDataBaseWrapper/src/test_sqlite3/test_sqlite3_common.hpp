#pragma once
#include "database/sqlite3/sqlite3ppext.hpp"
#include "fundamental/basic/log.h"
inline sqlite3pp::database contacts_db() {
    sqlite3pp::database db;
    db.execute(R"""(
    CREATE TABLE contacts (
      id INTEGER PRIMARY KEY,
      name TEXT NOT NULL,
      phone TEXT NOT NULL,
      address TEXT,
      age INTEGER,
      level INTEGER,
      UNIQUE(name, phone)
    );
  )""");
    return db;
}

inline void default_init_contacts_db(sqlite3pp::database& db) {
    db.execute("INSERT INTO contacts (name, phone,age,address,level) VALUES ('1', '1',1,'1',1)");
    db.execute("INSERT INTO contacts (name, phone,age,address,level) VALUES ('2', '2',2,'2',2)");
}