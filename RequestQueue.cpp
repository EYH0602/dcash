#include "RequestQueue.h"

using namespace std;

RequestQueue::RequestQueue(int size) {
  this->buff_size = size;
  this->has_room = PTHREAD_COND_INITIALIZER;
  this->has_req = PTHREAD_COND_INITIALIZER;
  this->queue_lock = PTHREAD_MUTEX_INITIALIZER;
}

void RequestQueue::enqueue(MySocket* client) {
  dthread_mutex_lock(&this->queue_lock);

  while (this->waiting_queue.size() >= this->buff_size) {
    dthread_cond_wait(&this->has_room, &this->queue_lock);
  }

  this->waiting_queue.push_back(client);

  #ifdef _DEBUG_SHOW_STEP_
  std::cout << "enqueue!" << std::endl;
  #endif

  dthread_cond_signal(&this->has_req);
  dthread_mutex_unlock(&this->queue_lock);
}

MySocket* RequestQueue::dequeue() {
  dthread_mutex_lock(&this->queue_lock);
  
  while (this->waiting_queue.empty()) {
    dthread_cond_wait(&this->has_req, &this->queue_lock);
  }

  MySocket* front = this->waiting_queue.front();
  this->waiting_queue.pop_front();

  #ifdef _DEBUG_SHOW_STEP_
  std::cout << "dequeue!" << std::endl;
  #endif

  dthread_cond_signal(&this->has_room);
  dthread_mutex_unlock(&this->queue_lock);
  return front;
}
