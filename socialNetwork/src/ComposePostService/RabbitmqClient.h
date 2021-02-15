#ifndef SOCIAL_NETWORK_MICROSERVICES_SRC_COMPOSEPOSTSERVICE_RABBITMQCLIENT_H_
#define SOCIAL_NETWORK_MICROSERVICES_SRC_COMPOSEPOSTSERVICE_RABBITMQCLIENT_H_

#include <SimpleAmqpClient/SimpleAmqpClient.h>

#include "../GenericClient.h"
#include "../utils.h"

namespace social_network {


class RabbitmqClient : public GenericClient {
 public:
  RabbitmqClient(const std::string &addr, int port);
  RabbitmqClient(const RabbitmqClient &) = delete;
  RabbitmqClient & operator=(const RabbitmqClient &) = delete;
  RabbitmqClient(RabbitmqClient &&) = default;
  RabbitmqClient & operator=(RabbitmqClient &&) = default;

  ~RabbitmqClient() override ;

  void Connect() override ;
  void Disconnect() override ;
  void KeepAlive() override ;
  void KeepAlive(int timeout_ms) override ;
  bool IsConnected() override ;

  AmqpClient::Channel::ptr_t GetChannel();

 private:
  std::string _addr;
  int _port;
  AmqpClient::Channel::ptr_t _channel;
  bool _is_connected;
  std::string _zone;
  std::string _queue;
};

RabbitmqClient::RabbitmqClient(const std::string &addr, int port) {
  _addr = addr;
  _port = port;
  _zone = load_zone();
  _queue = "write-home-timeline-"+_zone;
  _channel = AmqpClient::Channel::Create(addr, port, "admin", "admin");
  _is_connected = false;
}

RabbitmqClient::~RabbitmqClient() {
  Disconnect();
}

void RabbitmqClient::Connect() {
  if (!IsConnected()) {
    try {
      _channel->DeclareQueue(_queue, false, true, false, false);
    } catch (...) {
      throw;
    }
    _is_connected = true;
  }
}

void RabbitmqClient::Disconnect() {
  if (IsConnected()) {
    try {
      _channel->DeleteQueue(_queue);
      _is_connected = false;
    } catch (...) {
      throw;
    }
  }
}

void RabbitmqClient::KeepAlive() {

}

void RabbitmqClient::KeepAlive(int timeout_ms) {

}

bool RabbitmqClient::IsConnected() {
  return _is_connected;
}

AmqpClient::Channel::ptr_t RabbitmqClient::GetChannel() {
  return _channel;
}

} // namespace social_network



#endif //SOCIAL_NETWORK_MICROSERVICES_SRC_COMPOSEPOSTSERVICE_RABBITMQCLIENT_H_
