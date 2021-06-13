#include "Database.h"

#include <iostream>
#include <string>
#include <mysql/mysql.h>

#include "StringUtils.h"

using namespace std;

Database::Database(string un, string pwsd, string host, int port, string key) {
  this->stripe_secret_key = key;

  // init db
  mysql_init(this->users);
  this->users = mysql_real_connect(
    this->users, host.c_str(), un.c_str(), pwsd.c_str(), "users", port, NULL, 0
  );
  mysql_init(this->transfers);
  this->transfers = mysql_real_connect(
    this->transfers, host.c_str(), un.c_str(), pwsd.c_str(), "transfers", port, NULL, 0
  );
  mysql_init(this->deposits);
  this->deposits = mysql_real_connect(
    this->deposits, host.c_str(), un.c_str(), pwsd.c_str(), "deposits", port, NULL, 0
  );
  if (!(this->users && this->transfers && this->deposits)) {
    throw "DB Connection Faild.";
  }
}

int Database::getUser(User *user, string username, string password) {
  string sql = 
    "SELECT user_id, email, balance, password FROM users WHERE"
    " username = '" + username + "'";
  int sql_rnt = mysql_query(this->users, sql.c_str());
  int rnt;
  string user_id;
  MYSQL_RES* rst;
  MYSQL_ROW sql_row;
  vector<vector<string> > rows;
  StringUtils string_util;

  if (sql_rnt != 0) {
    throw "MySQL Query Error: " + to_string(sql_rnt);
  }
  rst = mysql_use_result(this->users);
  while (sql_row = mysql_fetch_row(rst), sql_row) {
    vector<string> row;
    for (int j = 0; j < mysql_num_fields(rst); ++j) {
      row.push_back(string(sql_row[j]));
    }
    rows.push_back(row);
  } 
  if (rows.size() > 1) {
    throw "DB Error: User not unique.";
  }

  // down searching
  user->username = username;
  user->password = password;
  if (rows.size() == 0) {
    rnt = 0;
    user->user_id = string_util.createUserId();
    user->email = "";
    user->balance = 0;
    this->addUser(user);
  } else {
    rnt = 1;
    user->user_id = rows[0][0];
    user->email = rows[0][1];
    user->balance = atoi(rows[0][2].c_str());
  }
  if (rows[0][3] != password) {
    rnt = 2;
  }

  mysql_free_result(rst);
  return rnt;
}

void Database::addUser(User *user) {
  string sql = 
  "INSERT INTO users"
    " (username, password, user_id) "
  "VALUE"
    " ('" + user->username + "', '" + user->password + "', '" + user->user_id + "')";
  mysql_query(this->users, sql.c_str());
}

Database::~Database() {
  mysql_close(this->users);
  mysql_close(this->transfers);
  mysql_close(this->deposits);
}
