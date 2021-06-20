#define RAPIDJSON_HAS_STDSTRING 1

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "AuthService.h"
#include "StringUtils.h"
#include "ClientError.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"

using namespace std;
using namespace rapidjson;


AuthService::AuthService() : HttpService("/auth-tokens") {
  
}

void AuthService::post(HTTPRequest *request, HTTPResponse *response) {
  
  // create the user
  StringUtils string_util;
  string auth_token;
  string username;
  string password;
  vector<string> info = string_util.split(request->getBody(), '&');

  // parse the request
  // user->user_id = string_util.createUserId();
  for (string kv_str: info) {
    vector<string> kv = string_util.split(kv_str, '=');
    if (kv[0] == "username") {
      username = kv[1];
    } else if (kv[0] == "password") {
      password = kv[1];
    } else {
      throw ClientError::badRequest();
    }
  }

  // check if there is upper case letter in username
  for (char ch: username) {
    if (isupper(ch)) {
      throw ClientError::forbidden();
    }
  }

  User *user = new User();
  int rnt = this->m_db->loginUser(user, username, password);
  switch (rnt) {
    case 0:
      response->setStatus(201);
      break;
    case 2:
      throw ClientError::forbidden();
    default:
      break;
  }

  // login the user
  auth_token = string_util.createAuthToken();
  this->m_db->auth_tokens[auth_token] = user;

  // example JSON API usage from doc
  Document document;
  Document::AllocatorType& a = document.GetAllocator();
  Value o;
  o.SetObject();
  o.AddMember("auth_token", auth_token, a);
  o.AddMember("user_id", user->user_id, a);

  // now some rapidjson boilerplate for converting the JSON object to a string
  document.Swap(o);
  StringBuffer buffer;
  PrettyWriter<StringBuffer> writer(buffer);
  document.Accept(writer);

  // set the return object
  response->setContentType("application/json");
  response->setBody(buffer.GetString() + string("\n"));

  #ifdef _TESTING_
  cout << "Login: " << user->username << endl;
  cout << "    user id: " << user->user_id << endl;
  cout << "    token: " << auth_token << endl;
  #endif
}

void AuthService::del(HTTPRequest *request, HTTPResponse *response) {
  // varify user
  string my_auth_token;
  if (request->hasAuthToken()) {
    my_auth_token = request->getAuthToken();
  } else {
    throw ClientError::notFound();
  }

  vector<string> path = request->getPathComponents();
  if (path.size() != 2) {
    throw ClientError::badRequest();
  }
  string target_auth_token = path[1];
  if (this->m_db->auth_tokens.count(target_auth_token) == 0) {
    throw ClientError::notFound();
  }
  if (this->m_db->auth_tokens[my_auth_token]->user_id 
      != this->m_db->auth_tokens[target_auth_token]->user_id) {
    throw ClientError::forbidden();
  }

  this->m_db->auth_tokens.erase(target_auth_token);

  #ifdef _TESTING_
  cout << "Logout: " << target_auth_token << endl;
  #endif
}
