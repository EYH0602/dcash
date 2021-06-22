#define RAPIDJSON_HAS_STDSTRING 1

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "AccountService.h"
#include "ClientError.h"
#include "StringUtils.h"
#include "WwwFormEncodedDict.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"

using namespace std;
using namespace rapidjson;

AccountService::AccountService() : HttpService("/users") {
  
}

void AccountService::writeHTTPResponse(HTTPResponse *response, User *user) {
  // example JSON API usage from doc
  Document document;
  Document::AllocatorType& a = document.GetAllocator();
  Value o;
  WwwFormEncodedDict decoder;

  o.SetObject();
  o.AddMember("balance", user->balance, a);
  o.AddMember("email", decoder.decode(user->email), a);

  // now some rapidjson boilerplate for converting the JSON object to a string
  document.Swap(o);
  StringBuffer buffer;
  PrettyWriter<StringBuffer> writer(buffer);
  document.Accept(writer);

  // set the return object
  response->setContentType("application/json");
  response->setBody(buffer.GetString() + string("\n"));
}

void AccountService::get(HTTPRequest *request, HTTPResponse *response) {
  // check for auth token
  User* user;
  try {
    user = this->getAuthenticatedUser(request);
  } catch (ClientError &ce) {
    throw ce;
  } catch (...) {}
  string auth_token = request->getAuthToken();

  if (request->getPathComponents().size() != 2) {
    throw ClientError::badRequest();
  }
  this->checkUserID(request);

  // parse request body
  string username = this->m_db->auth_tokens[auth_token]->username;
  this->writeHTTPResponse(response, user);

  #ifdef _TESTING_
  cout << "Set Email: " << endl;
  cout << "    user id: " << user->user_id << endl;
  cout << "    auth token: " << auth_token << endl;
  cout << "    email: " << user->email << endl;
  #endif

}

void AccountService::put(HTTPRequest *request, HTTPResponse *response) {
  // check for auth token
  User* user;
  try {
    user = this->getAuthenticatedUser(request);
  } catch (ClientError &ce) {
    throw ce;
  } catch (...) {}
  string auth_token = request->getAuthToken();
  this->checkUserID(request);

  // try the get the parameter email string
  string email;
  WwwFormEncodedDict decoder;
  try {
    StringUtils string_util;
    vector<string> info = string_util.split(request->getBody(), '=');
    if (info.size() != 2 || info[0] != "email") {
      throw ClientError::badRequest();
    }
    email = info[1];
  } catch (...) {
    throw ClientError::badRequest();
  }
  user->email = decoder.decode(email);

  this->m_db->updateEmail(user);
  this->writeHTTPResponse(response, user);

  #ifdef _TESTING_
  cout << "Set Email: " << endl;
  cout << "    user id: " << user->user_id << endl;
  cout << "    auth token: " << auth_token << endl;
  cout << "    email: " << user->email << endl;
  #endif

}
