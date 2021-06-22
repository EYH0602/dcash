#define RAPIDJSON_HAS_STDSTRING 1

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <string>
#include <sstream>

#include "TransferService.h"
#include "ClientError.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"

using namespace rapidjson;
using namespace std;

TransferService::TransferService() : HttpService("/transfers") { }


void TransferService::post(HTTPRequest *request, HTTPResponse *response) {
  // check for auth token
  User *user;
  try {
    user = this->getAuthenticatedUser(request);
  } catch (ClientError &ce) {
    throw ce;
  } catch (...) {}
  string auth_token = request->getAuthToken();

  // check if tr->from has sufficient balance
  if (user->balance <= 0) {
    throw ClientError::methodNotAllowed();
  }

  string username = user->username;

  // parse request body
  StringUtils string_util;
  vector<string> info_list = string_util.split(request->getBody(), '&');

  // init a Transfer object for use
  Transfer *tr = new Transfer();
  tr->from = user->username;
  tr->amount = 0;

  for (string info: info_list) {
    vector<string> kv = string_util.split(info, '=');
    if (kv[0] == "amount") {
      int amount = atoi(kv[1].c_str());
      // make sure the sender have enough money
      if (user->balance < amount) {
        throw ClientError::badRequest();
      }
      tr->amount = amount;
    } else if (kv[0] == "to") {
      // check if the tr->to is in db
      if (!this->m_db->hasUser(kv[1])) {
        throw ClientError::notFound();
      }
      tr->to = kv[1];
    } else {
      throw ClientError::badRequest();
    }
  }

  // make change in balance for both account (by refence)
  this->m_db->updateBalance(tr->to, tr->amount);
  this->m_db->updateBalance(tr->from, -1*tr->amount);
  user->balance -= tr->amount;

  // save transfer record to db
  this->m_db->addTransfer(tr);
  delete tr;

  // construct response
  Document document;
  Document::AllocatorType& a = document.GetAllocator();
  Value o;
  o.SetObject();
  o.AddMember("balance", user->balance, a);

  // create an array
  Value array;
  array.SetArray();

  // add an object to our array
  vector<vector<string> > history = this->m_db->getTransferHistory(user->username);
  for (vector<string> row: history) {
    Value to;
    to.SetObject();
    to.AddMember("from", row[0], a);
    to.AddMember("to", row[1], a);
    to.AddMember("amount", atoi(row[2].c_str()), a);
    array.PushBack(to, a);
  }
  // and add the array to our return object
  o.AddMember("transfers", array, a);

  // now some rapidjson boilerplate for converting the JSON object to a string
  document.Swap(o);
  StringBuffer buffer;
  PrettyWriter<StringBuffer> writer(buffer);
  document.Accept(writer);

  // set the return object
  response->setContentType("application/json");
  response->setBody(buffer.GetString() + string("\n"));

}
