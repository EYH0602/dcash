#include "Database.h"

#include <iostream>
#include <string>
#include <mysql/mysql.h>

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
}

vector<vector<string>> Database::applyQuery(string query) {
  vector<vector<string>> res;
  int rnt = mysql_query(this->db, query.c_str());
  if (rnt != 0) {
    throw "MySQL Query ERROR: " + to_string(rnt);
  }
  MYSQL_RES *rst = mysql_use_result(this->db);
  if (!rst) {
    return res;
  }
  MYSQL_ROW sql_row;
  while (sql_row = mysql_fetch_row(rst), sql_row) {
    vector<string> row;
    for (int j = 0; j < mysql_num_fields(rst); j++) {
      row.push_back(string(sql_row[j]));
    }
    res.push_back(row);
  }
  mysql_free_result(rst);
  return res;
}

int Database::getUser(User *user, string username, string password) {
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

void Database::addUser(User *user) {
  string sql = 
  "INSERT INTO users"
    " (username, password, user_id) "
  "VALUE"
    " ('" + user->username + "', '" + user->password + "', '" + user->user_id + "')";
  this->applyQuery(sql);
}
void Database::addTransfer(Transfer *tr) {
  string sql = 
  "INSERT INTO transfers"
    " (from, `to`, amount) "
  "VALUE"
    " ('" + tr->from + "', '" + tr->to+ "', '" + to_string(tr->amount) + "')";
  this->applyQuery(sql);
}

void Database::addDeposit(Deposit *dp) {
  string sql = 
  "INSERT INTO deposits"
    " (`to`, amount, stripe_charge_id) "
  "VALUE"
    " ('" + dp->to + "', '" + to_string(dp->amount) + "', '" + dp->stripe_charge_id + "')";
  this->applyQuery(sql);
}

vector<vector<string>> Database::getTransferHistory(string from) {
  string sql = "SELECT from, `to`, amount FROM transfers WHERE from = '" + from + "'";
  return this->applyQuery(sql);
}

vector<vector<string>> Database::getDepositHistory(string to) {
  string sql = "SELECT `to`, amount, stripe_charge_id FROM deposits WHERE `to` = '" + to + "'";
  return this->applyQuery(sql);
}

bool Database::hasUser(string username) {
  string sql = "SELECT COUNT(username) FROM users WHERE username = '" + username + "'";
  vector<vector<string>> rst = this->applyQuery(sql);
  return rst[0][0] == "1";
}

int Database::updateBalance(std::string username, int amount) {
  string sql = "UPDATE users SET balance = balance + " + to_string(amount) +
  " WHERE username = '" + username + "'";
  this->applyQuery(sql);
  sql = "SELECT balance FROM users WHERE username = '" + username + "'";
  vector<vector<string>> rst = this->applyQuery(sql);
  return atoi(rst[0][0].c_str());
}

void Database::updateEmail(User *user) {
  string sql = "UPDATE users SET email =  '" + user->email + "'"
  " WHERE username = '" + user->username + "'";
  this->applyQuery(sql);
}


Database::~Database() {
  mysql_close(this->db);
}
