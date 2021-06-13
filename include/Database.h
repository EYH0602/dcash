#ifndef _DATABASE_H_
#define _DATABASE_H_

#include <string>
#include <map>
#include <vector>
#include <mysql/mysql.h>

class User {
 public:
  std::string email;
  std::string username;
  std::string password;
  std::string user_id;
  int balance;
};

class Transfer {
 public:
  std::string from;
  std::string to;
  int amount;
};

class Deposit {
 public:
  std::string to;
  std::string stripe_charge_id;
  int amount;
};

// Note: you must use a single User object and hold pointers to it
// in the `users` and `auth_tokens` databases
class Database {
 public:
  std::map<std::string, User *> auth_tokens;
  std::string stripe_secret_key;
  Database();

  Database(std::string db_username, std::string db_password, 
    std::string host, int port, std::string db_name,
    std::string stripe_secret_key);

  /**
   * @brief Get the User object given account info from HTTP request.
   * 
   * @param username Username of the target user.
   * @param password The user's password to create/varify the user.
   * @return int status code:
   *    0 not in db
   *    1 find the user
   *    2 user in db but password does not match
   */
  int getUser(User *user, std::string username, std::string password);
  bool hasUser(std::string username);
  int updateBalance(std::string username, int amount);
  void addTransfer(Transfer *tr);
  void addDeposit(Deposit *dp);
  std::vector<std::vector<std::string>> getTransferHistory(std::string from);
  std::vector<std::vector<std::string>> getDepositHistory(std::string to);
  ~Database();

 private:
  // set by config.json
  MYSQL *db;

  /**
   * @brief add the input user to table.
   * 
   * @param user the user to be added.
   */
  void addUser(User *user);
  std::vector<std::vector<std::string>> applyQuery(std::string query);

};

#endif
