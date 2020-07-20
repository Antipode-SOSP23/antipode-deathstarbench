#include <signal.h>

#include <thrift/server/TThreadedServer.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>

#include "../utils.h"
#include "AntipodeOracle.h"

using apache::thrift::server::TThreadedServer;
using apache::thrift::transport::TServerSocket;
using apache::thrift::transport::TFramedTransportFactory;
using apache::thrift::protocol::TBinaryProtocolFactory;
using namespace social_network;

void sigintHandler(int sig) {
  exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
  signal(SIGINT, sigintHandler);
  init_logger();
  SetUpTracer("config/jaeger-config.yml", "antipode-oracle");

  json config_json;
  if (load_config_file("config/service-config.json", &config_json) == 0) {

    int port = config_json["antipode-oracle"]["port"];

    TThreadedServer server(
        std::make_shared<AntipodeOracleProcessor>(std::make_shared<AntipodeOracleHandler>()),
        std::make_shared<TServerSocket>("0.0.0.0", port),
        std::make_shared<TFramedTransportFactory>(),
        std::make_shared<TBinaryProtocolFactory>()
    );

    std::cout << "Starting the antipode-oracle server..." << std::endl;
    server.serve();
  } else exit(EXIT_FAILURE);
}