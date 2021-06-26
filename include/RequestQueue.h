#ifndef _REQUEST_QUEUE_H_
#define _REQUEST_QUEUE_H_

#include <deque>

#include "MySocket.h"
#include "dthread.h"

class RequestQueue {
 public:
  RequestQueue() = default;
  RequestQueue(int buff_size);

  /**
   * Enter a new client to the waiting queue.
   * If the size of queue reach the max allowed size,
   * keep waiting untile there is room(space).
   * Producer.
   * @param client The new request client to enqueue.
   */
  void enqueue(MySocket *client);

  /**
   * Extract the first waiting request client.
   * If the waiting is empty,
   * keep waiting untile it is not.
   * Comsumer.
   * @return The front request client of queue.
   */
  MySocket *dequeue();

 private:
  pthread_cond_t has_room;
  pthread_cond_t has_req;
  pthread_mutex_t queue_lock;

  std::deque<MySocket*> waiting_queue;
  size_t buff_size = 1;
};

#endif
