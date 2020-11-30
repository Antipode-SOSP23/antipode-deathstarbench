#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <signal.h>

#include "../utils.h"
#include "../utils_memcached.h"
#include "../utils_mongodb.h"
#include "PostStorageHandler.h"

using apache::thrift::server::TThreadedServer;
using apache::thrift::transport::TServerSocket;
using apache::thrift::transport::TFramedTransportFactory;
using apache::thrift::protocol::TBinaryProtocolFactory;
using namespace social_network;

static memcached_pool_st* memcached_client_pool;
static mongoc_client_pool_t* mongodb_client_pool;
static mongoc_client_pool_t* mongodb_client_pool_us;

void sigintHandler(int sig) {
  if (memcached_client_pool != nullptr) {
    memcached_pool_destroy(memcached_client_pool);
  }
  if (mongodb_client_pool != nullptr) {
    mongoc_client_pool_destroy(mongodb_client_pool);
  }
  if (mongodb_client_pool_us != nullptr) {
    mongoc_client_pool_destroy(mongodb_client_pool_us);
  }
  exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
  signal(SIGINT, sigintHandler);
  init_logger();
  SetUpTracer("config/jaeger-config.yml", "post-storage-service");

  json config_json;
  if (load_config_file("config/service-config.json", &config_json) != 0) {
    exit(EXIT_FAILURE);
  }

  memcached_client_pool =
      init_memcached_client_pool(config_json, "post-storage", 32, 1024);

  int port = config_json["post-storage-service"]["port"];
  std::string zone = (std::getenv("ZONE") == NULL) ? "" : std::getenv("ZONE");

  mongodb_client_pool = init_mongodb_client_pool(config_json, "post-storage", zone, 1024);
  if (memcached_client_pool == nullptr || mongodb_client_pool == nullptr) {
    return EXIT_FAILURE;
  }

  mongoc_client_t *mongodb_client = mongoc_client_pool_pop(mongodb_client_pool);
  if (!mongodb_client) {
    LOG(fatal) << "Failed to pop mongoc client";
    return EXIT_FAILURE;
  }

  if (replica_ismaster(mongodb_client)) {
    bool r = false;
    while (!r) {
      r = CreateIndex(mongodb_client, "post", "post_id", true);
      if (!r) {
        LOG(error) << "Failed to create mongodb index, try again";
        sleep(1);
      }
    }
  }

  mongoc_client_pool_push(mongodb_client_pool, mongodb_client);

  // mongoc in US
  mongodb_client_pool_us = init_mongodb_client_pool(config_json, "post-storage", "us", 1024);
  if (mongodb_client_pool_us == nullptr) {
    return EXIT_FAILURE;
  }

  mongoc_client_t *mongodb_client_us = mongoc_client_pool_pop(mongodb_client_pool_us);
  if (!mongodb_client_us) {
    LOG(fatal) << "Failed to pop mongoc client";
    return EXIT_FAILURE;
  }

  mongoc_client_pool_push(mongodb_client_pool_us, mongodb_client_us);
  //

  int antipode_oracle_port = config_json["antipode-oracle"]["port"];
  std::string antipode_oracle_addr = config_json["antipode-oracle"]["addr"];

  ClientPool<ThriftClient<AntipodeOracleClient>>
      antipode_oracle_client_pool("antipode-oracle", antipode_oracle_addr,
                                antipode_oracle_port, 0, 10000, 1000);

  TThreadedServer server (
      std::make_shared<PostStorageServiceProcessor>(
          std::make_shared<PostStorageHandler>(
              memcached_client_pool,
              mongodb_client_pool,
              mongodb_client_pool_us,
              &antipode_oracle_client_pool)),
      std::make_shared<TServerSocket>("0.0.0.0", port),
      std::make_shared<TFramedTransportFactory>(),
      std::make_shared<TBinaryProtocolFactory>()
  );

  std::cout << "Starting the post-storage-service server..." << std::endl;
  server.serve();
}
