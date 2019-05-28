#ifndef SOCIAL_NETWORK_MICROSERVICES_CLIENTPOOL_H
#define SOCIAL_NETWORK_MICROSERVICES_CLIENTPOOL_H

#include <vector>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <chrono>
#include <string>

#include <xtrace/xtrace.h>
#include "logger.h"

namespace social_network {

template<class TClient>
class ClientPool {
 public:
  ClientPool(const std::string &client_type, const std::string &addr,
      int port, int min_size, int max_size, int timeout_ms);
  ~ClientPool();

  ClientPool(const ClientPool&) = delete;
  ClientPool& operator=(const ClientPool&) = delete;
  ClientPool(ClientPool&&) = default;
  ClientPool& operator=(ClientPool&&) = default;

  TClient * Pop();
  void Push(TClient *);
  void Push(TClient *, int);
  void Remove(TClient *);

 private:
  std::deque<TClient *> _pool;
  std::string _addr;
  std::string _client_type;
  int _port;
  int _min_pool_size{};
  int _max_pool_size{};
  int _curr_pool_size{};
  int _timeout_ms;
  std::mutex _mtx;
  std::condition_variable _cv;

};

template<class TClient>
ClientPool<TClient>::ClientPool(const std::string &client_type,
    const std::string &addr, int port, int min_pool_size,
    int max_pool_size, int timeout_ms) {
  _addr = addr;
  _port = port;
  _min_pool_size = min_pool_size;
  _max_pool_size = max_pool_size;
  _timeout_ms = timeout_ms;
  _client_type = client_type;

  for (int i = 0; i < min_pool_size; ++i) {
    TClient *client = new TClient(addr, port);
    _pool.emplace_back(client);
  }
  _curr_pool_size = min_pool_size;
}

template<class TClient>
ClientPool<TClient>::~ClientPool() {
  while (!_pool.empty()) {
    delete _pool.front();
    _pool.pop_front();
  }
}

template<class TClient>
TClient * ClientPool<TClient>::Pop() {
  XTRACE("Popping client from client pool", {{"clientType" , _client_type}});
  TClient * client = nullptr;
  XTRACE("Obtaining lock on client pool mutex");
  std::unique_lock<std::mutex> cv_lock(_mtx); {
    XTRACE("Obtained lock on client pool mutex");
    while (_pool.size() == 0) {
      XTRACE("No client available in client pool");
      // Create a new a client if current pool size is less than
      // the max pool size.
      if (_curr_pool_size < _max_pool_size) {
        try {
          XTRACE("Creating a new client");
          client = new TClient(_addr, _port);
          _curr_pool_size++;
          break;
        } catch (...) {
          XTRACE("Failed to create a new client");
          XTRACE("Releasing lock on client pool mutex");
          cv_lock.unlock();
          return nullptr;
        }
      } else {
        XTRACE("Waiting until a client becomes available to use");
        auto wait_time = std::chrono::system_clock::now() +
            std::chrono::milliseconds(_timeout_ms);
        bool wait_success = _cv.wait_until(cv_lock, wait_time,
            [this] { return _pool.size() > 0; });
        if (!wait_success) {
          XTRACE("ClientPool pop timeout");
          LOG(warning) << "ClientPool pop timeout";
          XTRACE("Releasing lock on client pool mutex");
          cv_lock.unlock();
          return nullptr;
        }
      }
    }
    if (!client){
      XTRACE("Popping client from front of the pool");
      client = _pool.front();
      _pool.pop_front();
    }

  } // cv_lock(_mtx)
  XTRACE("Releasing lock on client pool mutex");
  cv_lock.unlock();

  if (client) {
    try {
      XTRACE("Connecting to client");
      client->Connect();
    } catch (...) {
      LOG(error) << "Failed to connect " + _client_type;
      XTRACE("Failed to connect to client");
      _pool.push_back(client);
      throw;
    }    
  }
  return client;
}

template<class TClient>
void ClientPool<TClient>::Push(TClient *client) {
  XTRACE("Pushing a client into client pool", {{"clientType", _client_type}});
  XTRACE("Acquiring lock on mutex");
  std::unique_lock<std::mutex> cv_lock(_mtx);
  XTRACE("Acquired lock on mutex");
  client->KeepAlive();
  XTRACE("Pushing client back into client pool");
  _pool.push_back(client);
  XTRACE("Releasing lock on mutex");
  cv_lock.unlock();
  _cv.notify_one();
}

template<class TClient>
void ClientPool<TClient>::Push(TClient *client, int timeout_ms) {
  XTRACE("Pushing a client into client pool", {{"clientType", _client_type}});
  XTRACE("Acquiring lock on mutex");
  std::unique_lock<std::mutex> cv_lock(_mtx);
  XTRACE("Acquired lock on mutex");
  client->KeepAlive(timeout_ms);
  XTRACE("Pushing client back into client pool");
  _pool.push_back(client);
  XTRACE("Releasing lock on mutex");
  cv_lock.unlock();
  _cv.notify_one();
}

template<class TClient>
void ClientPool<TClient>::Remove(TClient *client) {
  XTRACE("Removing a client from client pool", {{"clientType", _client_type}});
  XTRACE("Acquiring lock on mutex");
  std::unique_lock<std::mutex> lock(_mtx);
  XTRACE("Acquired lock on mutex");
  delete client;
  _curr_pool_size--;
  XTRACE("Releasing lock on mutex");
  lock.unlock();
}

} // namespace social_network


#endif //SOCIAL_NETWORK_MICROSERVICES_CLIENTPOOL_H
