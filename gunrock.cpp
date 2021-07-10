#define RAPIDJSON_HAS_STDSTRING 1

#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <fcntl.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include <deque>

#include "ClientError.h"
#include "HTTPRequest.h"
#include "HTTPResponse.h"
#include "HttpService.h"
#include "HttpUtils.h"
#include "AccountService.h"
#include "AuthService.h"
#include "DepositService.h"
#include "FileService.h"
#include "TransferService.h"
#include "MySocket.h"
#include "MyServerSocket.h"
#include "dthread.h"
#include "RequestQueue.h"

#include "rapidjson/document.h"

using namespace std;
using namespace rapidjson;

int PORT = 8080;
int THREAD_POOL_SIZE = 1;
int BUFFER_SIZE = 1;
string BASEDIR = "static";
string SCHEDALG = "FIFO";
string LOGFILE = "/dev/null";
RequestQueue *req_queue;

vector<HttpService *> services;

HttpService *find_service(HTTPRequest *request) {
   // find a service that is registered for this path prefix
  for (unsigned int idx = 0; idx < services.size(); idx++) {
    if (request->getPath().find(services[idx]->pathPrefix()) == 0) {
      return services[idx];
    }
  }

  return NULL;
}


void invoke_service_method(HttpService *service, HTTPRequest *request, HTTPResponse *response) {
  stringstream payload;

  try {
    // invoke the service if we found one
    if (service == NULL) {
      // not found status
      response->setStatus(404);
    } else if (request->isHead()) {
      service->head(request, response);
    } else if (request->isGet()) {
      service->get(request, response);
    } else if (request->isPut()) {
      service->put(request, response);
    } else if (request->isPost()) {
      service->post(request, response);
    } else if (request->isDelete()) {
      service->del(request, response);
    } else if (request->isOptions()) {
      // tell the browser 'x-auth-token' header is allowed
      response->setHeader("Access-Control-Allow-Methods", "*");
      response->setHeader("Access-Control-Allow-Headers", "x-auth-token");
    } else {
      // The server doesn't know about this method
      response->setStatus(501);
    }
  } catch (ClientError &ce) {
    response->setStatus(ce.status_code);
  } catch (...) {
    // reset the response object and return an error
    response->setBody("");
    response->setStatus(500);
  }
}

void handle_request(MySocket *client) {
  HTTPRequest *request = new HTTPRequest(client, PORT);
  HTTPResponse *response = new HTTPResponse();
  stringstream payload;
  
  // read in the request
  bool readResult = false;
  try {
    payload << "client: " << (void *) client;
    sync_print("read_request_enter", payload.str());
    readResult = request->readRequest();
    sync_print("read_request_return", payload.str());
  } catch (...) {
    // swallow it
  }    
    
  if (!readResult) {
    // there was a problem reading in the request, bail
    delete response;
    delete request;
    sync_print("read_request_error", payload.str());
    return;
  }
  
  HttpService *service = find_service(request);
  invoke_service_method(service, request, response);

  // send data back to the client and clean up
  payload.str(""); payload.clear();
  payload << " RESPONSE " << response->getStatus() << " client: " << (void *) client;
  sync_print("write_response", payload.str());
  cout << payload.str() << endl;
  client->write(response->response());
    
  delete response;
  delete request;

  payload.str(""); payload.clear();
  payload << " client: " << (void *) client;
  sync_print("close_connection", payload.str());
  client->close();
  delete client;
}

/**
 * Helper function for handle_request.
 * Keep a worker thread alive unless the server is shut down.
 * @param None
 * @return NULL
 */
void* handle_thread(void* arg) {
  while (true) {
    MySocket* client = req_queue->dequeue();
    if (client) {
      #ifdef _DEBUG_SHOW_STEP_
      cout << "ready to handle request" << endl;
      #endif
      handle_request(client);
    }
  }
  return NULL;
}

int main(int argc, char *argv[]) {

  signal(SIGPIPE, SIG_IGN);
  int option;

  while ((option = getopt(argc, argv, "d:p:t:b:s:l:")) != -1) {
    switch (option) {
    case 'd':
      BASEDIR = string(optarg);
      break;
    case 'p':
      PORT = atoi(optarg);
      break;
    case 't':
      THREAD_POOL_SIZE = atoi(optarg);
      break;
    case 'b':
      BUFFER_SIZE = atoi(optarg);
      break;
    case 's':
      SCHEDALG = string(optarg);
      break;
    case 'l':
      LOGFILE = string(optarg);
      break;
    default:
      cerr<< "usage: " << argv[0] << " [-p port] [-t threads] [-b buffers]" << endl;
      exit(1);
    }
  }

  set_log_file(LOGFILE);

  sync_print("init", "");
  MyServerSocket *server = new MyServerSocket(PORT);
  MySocket *client;

  // The order that you push services dictates the search order
  // for path prefix matching
  services.push_back(new AuthService());
  services.push_back(new TransferService());
  services.push_back(new DepositService());
  services.push_back(new AccountService());
  services.push_back(new FileService(BASEDIR));

  // parse out config information
  stringstream config;
  int fd = open("config.json", O_RDONLY);
  if (fd < 0) {
    cout << "config.json not found" << endl;
    exit(1);
  }
  int ret;
  char buffer[4096];
  while ((ret = read(fd, buffer, sizeof(buffer))) > 0) {
    config << string(buffer, ret);
  }
  Document d;
  d.Parse(config.str());
  string stripe_secret_key = d["stripe_secret_key"].GetString();
  string db_username = d["db_username"].GetString();
  string db_password = d["db_password"].GetString();
  string db_name = d["db_name"].GetString();
  string host = d["host"].GetString();
  int port = d["port"].GetInt();
  
  // Make sure that all services have a pointer to the
  // database object singleton
  Database *db = new Database(
    db_username, db_password, host, port, db_name, stripe_secret_key
  );
  vector<HttpService *>::iterator iter;
  for (iter = services.begin(); iter != services.end(); iter++) {
    (*iter)->m_db = db;
  }

  // init request queue for multi-threading concurrency
  req_queue = new RequestQueue(THREAD_POOL_SIZE);
  // when the server is first started
  // the master thread creates a fixed-size pool of worker threads
  pthread_t thread_pool[THREAD_POOL_SIZE];
  for (int idx = 0; idx < THREAD_POOL_SIZE; idx++) {
    dthread_create(&thread_pool[idx], NULL, handle_thread, NULL);
  }

  while(true) {
    sync_print("waiting_to_accept", "");
    client = server->accept();
    sync_print("client_accepted", "");
    // place the connection descriptor into a fixed-size buffer 
    // and return to accepting more connections.
    req_queue->enqueue(client);
  }

  delete db;
  delete req_queue;
}
