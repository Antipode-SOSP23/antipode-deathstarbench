#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <signal.h>
#include <nlohmann/json.hpp>

#include "../utils.h"
#include "../utils_memcached.h"
#include "../utils_mongodb.h"
#include "PostStorageHandler.h"

using apache::thrift::server::TThreadedServer;
using apache::thrift::transport::TServerSocket;
using apache::thrift::transport::TFramedTransportFactory;
using apache::thrift::protocol::TBinaryProtocolFactory;
using namespace social_network;
using json = nlohmann::json;

static memcached_pool_st* memcached_client_pool;
static mongoc_client_pool_t* mongodb_client_pool;

void sigintHandler(int sig) {
  if (memcached_client_pool != nullptr) {
    memcached_pool_destroy(memcached_client_pool);
  }
  if (mongodb_client_pool != nullptr) {
    mongoc_client_pool_destroy(mongodb_client_pool);
  }
  exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
  signal(SIGINT, sigintHandler);
  std::string zone = (std::getenv("ZONE") == NULL) ? "eu" : std::getenv("ZONE");
  std::string service_name = "post-storage-service-" + zone;

  init_logger();
  SetUpTracer("config/jaeger-config.yml", service_name);

  json config_json;
  if (load_config_file("config/service-config.json", &config_json) != 0) {
    exit(EXIT_FAILURE);
  }

  memcached_client_pool =
      init_memcached_client_pool(config_json, "post-storage", 32, 1024);

  int port = config_json[service_name]["port"];

  mongodb_client_pool = init_mongodb_client_pool(config_json, "post-storage", zone, 1024);
  if (memcached_client_pool == nullptr || mongodb_client_pool == nullptr) {
    return EXIT_FAILURE;
  }

  mongoc_client_t *mongodb_client = mongoc_client_pool_pop(mongodb_client_pool);
  if (!mongodb_client) {
    LOG(fatal) << "Failed to pop mongoc client";
    return EXIT_FAILURE;
  }

  // ref: http://mongoc.org/libmongoc/1.14.0/mongoc_client_get_server_descriptions.html
  // ensure client has connected
  bson_error_t error;
  bson_t *b = BCON_NEW ("ping", BCON_INT32 (1));
  if(!mongoc_client_command_simple(mongodb_client, "db", b, NULL, NULL, &error)){
    LOG(fatal) << "Failed to ping mongodb instance: " << error.message;
    return EXIT_FAILURE;
  }
  // get latest server description
  mongoc_server_description_t *sd;
  sd = mongoc_client_get_server_description(mongodb_client, 1);
  if (sd == NULL) {
    LOG(fatal) << "Failed to fetch server description";
    return EXIT_FAILURE;
  }
  // parse is master reply
  auto is_master_bson = bson_as_json(mongoc_server_description_ismaster(sd), nullptr);
  if (is_master_bson == NULL) {
    LOG(fatal) << "Unable to fetch server is_master flag";
    return EXIT_FAILURE;
  }
  mongoc_server_description_destroy(sd);
  // parse bson to json
  json is_master_json = json::parse(is_master_bson);
  if (is_master_json["ismaster"]) {
    if (!CreateIndex(mongodb_client, "post", "post_id", true)) {
      LOG(fatal) << "Failed to create mongodb index on master";
      return EXIT_FAILURE;
    }
  }
  mongoc_client_pool_push(mongodb_client_pool, mongodb_client);

  int antipode_oracle_port = config_json["antipode-oracle"]["port"];
  std::string antipode_oracle_addr = config_json["antipode-oracle"]["addr"];

  int write_home_timeline_service_port = config_json["write-home-timeline-service"]["port"];
  std::string write_home_timeline_service_addr = config_json["write-home-timeline-service"]["addr"];

  int post_storage_eu_port = config_json["post-storage-service-eu"]["port"];
  std::string post_storage_eu_addr = config_json["post-storage-service-eu"]["addr"];

  int post_storage_us_port = config_json["post-storage-service-us"]["port"];
  std::string post_storage_us_addr = config_json["post-storage-service-us"]["addr"];

  ClientPool<ThriftClient<AntipodeOracleClient>>
      antipode_oracle_client_pool("antipode-oracle", antipode_oracle_addr,
                                antipode_oracle_port, 0, 10000, 1000);

  ClientPool<ThriftClient<WriteHomeTimelineServiceClient>>
      write_home_timeline_service_client_pool("write-home-timeline-service", write_home_timeline_service_addr,
                                write_home_timeline_service_port, 0, 10000, 1000);

  ClientPool<ThriftClient<PostStorageServiceClient>>
      post_storage_client_eu_pool("post-storage-client-eu", post_storage_eu_addr,
                               post_storage_eu_port, 0, 10000, 1000);

  ClientPool<ThriftClient<PostStorageServiceClient>>
      post_storage_client_us_pool("post-storage-client-us", post_storage_us_addr,
                               post_storage_us_port, 0, 10000, 1000);

  TThreadedServer server (
      std::make_shared<PostStorageServiceProcessor>(
          std::make_shared<PostStorageHandler>(
              memcached_client_pool,
              mongodb_client_pool,
              &antipode_oracle_client_pool,
              &write_home_timeline_service_client_pool,
              &post_storage_client_eu_pool,
              &post_storage_client_us_pool)),
      std::make_shared<TServerSocket>("0.0.0.0", port),
      std::make_shared<TFramedTransportFactory>(),
      std::make_shared<TBinaryProtocolFactory>()
  );

  std::cout << "Starting the " << service_name << " server..." << std::endl;
  server.serve();
}
