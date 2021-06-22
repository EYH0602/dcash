#define RAPIDJSON_HAS_STDSTRING 1

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <string>
#include <sstream>

#include "DepositService.h"
#include "Database.h"
#include "ClientError.h"
#include "HTTPClientResponse.h"
#include "HttpClient.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"

using namespace rapidjson;
using namespace std;

DepositService::DepositService() : HttpService("/deposits") { }

void DepositService::post(HTTPRequest *request, HTTPResponse *response) {
  // check for auth token
  User* user;
  try {
    user = this->getAuthenticatedUser(request);
  } catch (ClientError &ce) {
    throw ce;
  } catch (...) {}

  string auth_token = request->getAuthToken();

  // parse request body
  StringUtils string_util;
  WwwFormEncodedDict decoder;
  vector<string> info_list = string_util.split(request->getBody(), '&');

  Deposit *dp = new Deposit();
  dp->to = user->username;
  dp->amount = 0;
  dp->stripe_charge_id = "";
  string stripe_token = "";

  for (string info: info_list) {
    vector<string> kv = string_util.split(info, '=');
    string key = decoder.decode(kv[0]);
    string value = decoder.decode(kv[1]);
    if (key == "amount") {
      dp->amount = atoi(value.c_str());
    } else if (key == "stripe_token") {
      stripe_token = value;
    } else {
      throw ClientError::badRequest();
    }
  }

  #ifdef _TESTING_
  cout << "amount: " << dp->amount << endl; 
  cout << "stripe_token: " << stripe_token << endl;
  #endif

  // string forbidden amount less than 50 cents
  if (dp->amount < 50) {
    throw ClientError::forbidden();
  }

  // from the gunrock server to Stripe
  HttpClient client("api.stripe.com", 443, true);
  client.set_basic_auth(m_db->stripe_secret_key, "");
  WwwFormEncodedDict body;
  body.set("amount", dp->amount); // unit conversion is done on client side
  body.set("currency", "usd");
  body.set("source", stripe_token);
  string encoded_body = body.encode();
  HTTPClientResponse *client_response = client.post("/v1/charges", encoded_body);

  #ifdef _TESTING_
  cout << "Stripe Status: " << client_response->status() << endl;
  cout << "Stripe Success: " << client_response->success() << endl;
  #endif

  if (!client_response->success()) {
    delete client_response;
    throw ClientError::badRequest();
  }

  Document *d = client_response->jsonBody();
  dp->stripe_charge_id = (*d)["id"].GetString();
  delete client_response;
  delete d;


  #ifdef _TESTING_
  cout << "Stripe Charge ID: " << dp->stripe_charge_id << endl;
  #endif

  this->m_db->addDeposit(dp);
  this->m_db->updateBalance(dp->to, dp->amount);
  user->balance += dp->amount;
  delete dp;

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
  vector<vector<string>> deposits = this->m_db->getDepositHistory(user->username);
  for (vector<string> record: deposits) {
    Value to;
    to.SetObject();
    to.AddMember("to", user->username, a);
    to.AddMember("amount", record[1], a);
    to.AddMember("stripe_charge_id", record[2], a);
    array.PushBack(to, a);
  }
  // and add the array to our return object
  o.AddMember("deposits", array, a);

  // now some rapidjson boilerplate for converting the JSON object to a string
  document.Swap(o);
  StringBuffer buffer;
  PrettyWriter<StringBuffer> writer(buffer);
  document.Accept(writer);

  // set the return object
  response->setContentType("application/json");
  response->setBody(buffer.GetString() + string("\n"));
}
