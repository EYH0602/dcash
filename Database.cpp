#include "Database.h"

#include <iostream>
#include <string>
#include <stdio.h>
#include <time.h>
#include <mysql/mysql.h>

#include "ClientError.h"
#include "StringUtils.h"

using namespace std;

Database::Database(string un, string pwsd, string host, int port, string name, string key) {
  this->stripe_secret_key = key;

  // init db
  this->db = mysql_init(NULL);
  this->db = mysql_real_connect(
    this->db, host.c_str(), un.c_str(), pwsd.c_str(), name.c_str(), port, NULL, 0
  );
  if (!this->db) {
    throw "DB Connection Faild.";
  }

  // create tables as needed
  this->applyQuery(this->getCreateTableSQL("users"));
  this->applyQuery(this->getCreateTableSQL("transfers"));
  this->applyQuery(this->getCreateTableSQL("deposits"));
}

string Database::getCreateTableSQL(string table_name) {
  string sql;
  if (table_name == "users") {
    sql = "CREATE TABLE IF NOT EXISTS `users` ("
          "  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,"
          "  `username` varchar(255) NOT NULL,"
          "  `password` varchar(255) NOT NULL,"
          "  `user_id` varchar(255) NOT NULL COMMENT 'randomized string',"
          "  `email` varchar(255) NOT NULL DEFAULT '',"
          "  `balance` bigint(20) NOT NULL DEFAULT 0,"
          "  `create_time` timestamp NOT NULL COMMENT 'timestamp when the user was first created',"
          "  `update_time` timestamp NOT NULL COMMENT 'timestamp when the user info is most recently updated',"
          "  PRIMARY KEY (`id`), INDEX(`username`), INDEX(`create_time`)"
          ") ENGINE=InnoDB AUTO_INCREMENT=0 DEFAULT CHARSET=utf8 COMMENT='Account info of users'";
  } else if (table_name == "transfers") {
    sql = "CREATE TABLE IF NOT EXISTS `transfers` ("
          " `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,"
          " `from` varchar(255) NOT NULL,"
          " `to` varchar(255) NOT NULL,"
          " `amount` bigint(20) NOT NULL,"
          " `create_time` timestamp NOT NULL COMMENT 'timestamp when the transfer was made',"
          " PRIMARY KEY (`id`), INDEX(`from`), INDEX(`create_time`)"
          " ) ENGINE=InnoDB AUTO_INCREMENT=0 DEFAULT CHARSET=utf8 COMMENT='Transfer records'";
  } else if (table_name == "deposits") {
    sql = "CREATE TABLE IF NOT EXISTS `deposits` ("
          " `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,"
          " `to` varchar(255) NOT NULL,"
          " `amount` bigint(20) NOT NULL,"
          " `stripe_charge_id` varchar(255) NOT NULL,"
          " `create_time` timestamp NOT NULL COMMENT 'timestamp when the deposit was made',"
          " PRIMARY KEY (`id`), INDEX(`to`), index(`create_time`)"
          " ) ENGINE=InnoDB AUTO_INCREMENT=0 DEFAULT CHARSET=utf8 COMMENT='Deposit records'";
  } else {
    throw ClientError::forbidden();
  }
  return sql;
}

vector<vector<string>> Database::applyQuery(string query) {
  vector<vector<string>> res;
  int rnt = mysql_query(this->db, query.c_str());
  if (rnt != 0) {
    throw "MySQL Query ERROR: " + to_string(rnt);
  }

  // store the results of this query
  MYSQL_RES *rst = mysql_use_result(this->db);
  // if there is not return result for this query,
  // just exist
  if (!rst) {
    return res;
  }

  // fetch the query results
  MYSQL_ROW sql_row;
  while (sql_row = mysql_fetch_row(rst), sql_row) {
    vector<string> row;
    for (size_t j = 0; j < mysql_num_fields(rst); j++) {
      row.push_back(string(sql_row[j]));
    }
    res.push_back(row);
  }
  mysql_free_result(rst);
  return res;
}

int Database::loginUser(User *user, string username, string password) {
  string sql = 
    "SELECT user_id, email, balance, password FROM users WHERE"
    " username = '" + username + "'";
  StringUtils string_util;
  int rnt;
  vector<vector<string>> rst = this->applyQuery(sql);
  if (rst.size() > 1) {
    throw "DB Error: User not unique.";
  }

  // down searching
  user->username = username;
  user->password = password;
  if (rst.size() == 0) {
    rnt = 0;
    user->user_id = string_util.createUserId();
    user->email = "";
    user->balance = 0;
    this->addUser(user);
  } else {
    rnt = 1;
    user->user_id = rst[0][0];
    user->email = rst[0][1];
    user->balance = atoi(rst[0][2].c_str());
  }
  if (rnt != 0 && rst[0][3] != password) {
    rnt = 2;
  }

  return rnt;
}

bool Database::hasUser(string username) {
  string sql = "SELECT COUNT(username) FROM users WHERE username = '" + username + "'";
  vector<vector<string>> rst = this->applyQuery(sql);
  return rst[0][0] == "1";
}

int Database::updateBalance(std::string username, int amount) {
  string time = this->getCurrentTimestamp();
  string sql =
  "UPDATE users SET"
  " update_time = '" + time + "',"
  " balance = balance + " + to_string(amount) +
  " WHERE username = '" + username + "'";
  this->applyQuery(sql);
  sql = "SELECT balance FROM users WHERE username = '" + username + "'";
  vector<vector<string>> rst = this->applyQuery(sql);
  return atoi(rst[0][0].c_str());
}

void Database::updateEmail(User *user) {
  string sql =
  "UPDATE users SET"
  " email =  '" + user->email + "', "
  " update_time = '" + this->getCurrentTimestamp() + "'"
  " WHERE username = '" + user->username + "'";
  this->applyQuery(sql);
}

void Database::addUser(User *user) {
  string time = this->getCurrentTimestamp();
  string sql = 
  "INSERT INTO users"
    " (username, password, user_id, create_time, update_time) "
  "VALUES"
    " ('" + user->username + "', '" + user->password + "', '" + user->user_id + "', '"
    + time + "', '" + time+ "')";
  this->applyQuery(sql);
}

void Database::addTransfer(Transfer *tr) {
  string time = this->getCurrentTimestamp();
  string sql = 
  "INSERT INTO transfers"
    " (`from`, `to`, amount, create_time) "
  "VALUES"
    " ('" + tr->from + "', '" + tr->to+ "', '" + to_string(tr->amount) + "', '" + time + "')";
  this->applyQuery(sql);
}

void Database::addDeposit(Deposit *dp) {
  string time = this->getCurrentTimestamp();
  string sql = 
  "INSERT INTO deposits"
    " (`to`, amount, stripe_charge_id, create_time) "
  "VALUES"
    " ('" + dp->to + "', '" + to_string(dp->amount) + "', '" + dp->stripe_charge_id + "', '" + time + "')";
  this->applyQuery(sql);
}

vector<string> Database::getUserInfo(string username) {
  string sql = "SELECT * FROM users WHERE `username` = '" + username + "'";
  return this->applyQuery(sql)[0];
}

vector<vector<string>> Database::getTransferHistory(string from) {
  string sql = "SELECT `from`, `to`, amount FROM transfers WHERE `from` = '" + from + "'";
  return this->applyQuery(sql);
}

vector<vector<string>> Database::getDepositHistory(string to) {
  string sql = "SELECT `to`, amount, stripe_charge_id FROM deposits WHERE `to` = '" + to + "'";
  return this->applyQuery(sql);
}

string Database::getCurrentTimestamp() {
  time_t now = time(0);
  struct tm tstruct;
  char buf[80];
  tstruct = *localtime(&now);
  // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
  // for more information about date/time format
  strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);

  return string(buf);
}

Database::~Database() {
  mysql_close(this->db);
  for (auto kv: this->auth_tokens) {
    delete kv.second;
  }
}
