
#include <csignal>
#include <cpp_redis/cpp_redis>
#include <mutex>
#include <thread>
#include <sstream>
#include <set>

#include "../AmqpLibeventHandler.h"
#include "../ClientPool.h"
#include "../RedisClient.h"
#include "../ThriftClient.h"
#include "../logger.h"
#include "../tracing.h"
#include "../utils.h"
#include "../../gen-cpp/social_network_types.h"
#include "../../gen-cpp/SocialGraphService.h"
#include "../../gen-cpp/AntipodeOracle.h"
#include <xtrace/xtrace.h>
#include <xtrace/baggage.h>

#define NUM_WORKERS 4

using namespace social_network;

static std::exception_ptr _teptr;
static ClientPool<RedisClient> *_redis_client_pool;
static ClientPool<ThriftClient<SocialGraphServiceClient>> *_social_graph_client_pool;
static ClientPool<ThriftClient<AntipodeOracleClient>> *_antipode_oracle_client_pool;


void sigintHandler(int sig) {
  exit(EXIT_SUCCESS);
}

void OnReceivedWorker(const AMQP::Message &msg) {
  try {
    json msg_json = json::parse(std::string(msg.body(), msg.bodySize()));

    std::map<std::string, std::string> carrier;
    for (auto it = msg_json["carrier"].begin();
        it != msg_json["carrier"].end(); ++it) {
      carrier.emplace(std::make_pair(it.key(), it.value()));
    }

    auto baggage_it = carrier.find("baggage");
    if (baggage_it != carrier.end()) {
      SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
    }

    if (!XTrace::IsTracing()) {
      XTrace::StartTrace("WriteHomeTimelineService");
    }

    XTRACE("WriteHomeTimelineService::OnReceivedWorker");
    // Jaeger tracing
    TextMapReader span_reader(carrier);
    auto parent_span = opentracing::Tracer::Global()->Extract(span_reader);
    auto span = opentracing::Tracer::Global()->StartSpan(
        "FanoutHomeTimelines",
        {opentracing::ChildOf(parent_span->get())});
    std::map<std::string, std::string> writer_text_map;
    TextMapWriter writer(writer_text_map);
    opentracing::Tracer::Global()->Inject(span->context(), writer);

    // Extract information from rabbitmq messages
    int64_t user_id = msg_json["user_id"];
    int64_t req_id = msg_json["req_id"];
    int64_t post_id = msg_json["post_id"];
    int64_t timestamp = msg_json["timestamp"];
    std::vector<int64_t> user_mentions_id = msg_json["user_mentions_id"];

    //----------
    // ANTIPODE
    //----------
    LOG(debug) << "[ANTIPODE] Start IsVisible for post_id: " << post_id;
    auto antipode_orable_client_wrapper = _antipode_oracle_client_pool->Pop();
    if (!antipode_orable_client_wrapper) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_THRIFT_CONN_ERROR;
      se.message = "[ANTIPODE] Failed to connect to antipode-oracle";
      throw se;
    }
    auto antipode_oracle_client = antipode_orable_client_wrapper->GetClient();
    // loop oracle calls until its visible
    bool antipode_oracle_response = false;
    try {
      antipode_oracle_response = antipode_oracle_client->IsVisible(post_id);
    } catch (...) {
      LOG(error) << "[ANTIPODE] Failed to check post visibility in Oracle";
      _antipode_oracle_client_pool->Push(antipode_orable_client_wrapper);
      throw;
    }
    _antipode_oracle_client_pool->Push(antipode_orable_client_wrapper);
    LOG(debug) << "[ANTIPODE] Post successfuly checked as visible in Oracle with response: " << antipode_oracle_response;
    //----------
    // ANTIPODE
    //----------

    // Find followers of the user
    auto social_graph_client_wrapper = _social_graph_client_pool->Pop();
    if (!social_graph_client_wrapper) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_THRIFT_CONN_ERROR;
      se.message = "Failed to connect to social-graph-service";
      XTRACE("Failed to connect to social-graph-service");
      throw se;
    }
    auto social_graph_client = social_graph_client_wrapper->GetClient();
    std::vector<int64_t> followers_id;
    try {
      writer_text_map["baggage"] = BRANCH_CURRENT_BAGGAGE().str();
      UidListRpcResponse response;
      social_graph_client->GetFollowers(response, req_id, user_id,
                                        writer_text_map);
      followers_id = response.result;
      Baggage b = Baggage::deserialize(response.baggage);
      JOIN_CURRENT_BAGGAGE(b);
    } catch (...) {
      LOG(error) << "Failed to get followers from social-network-service";
      XTRACE("Failed to get followers from social-network-service");
      _social_graph_client_pool->Push(social_graph_client_wrapper);
      throw;
    }
    _social_graph_client_pool->Push(social_graph_client_wrapper);

    std::string debug_str(followers_id.begin(), followers_id.end());
    LOG(debug) << "Followers are: " << debug_str;

    std::set<int64_t> followers_id_set(followers_id.begin(),
        followers_id.end());
    followers_id_set.insert(user_mentions_id.begin(), user_mentions_id.end());

    // Update Redis ZSet
    XTRACE("RedisUpdate start");
    auto redis_span = opentracing::Tracer::Global()->StartSpan(
        "RedisUpdate", {opentracing::ChildOf(&span->context())});
    auto redis_client_wrapper = _redis_client_pool->Pop();
    if (!redis_client_wrapper) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_REDIS_ERROR;
      se.message = "Cannot connect to Redis server";
      XTRACE("Cannot connect to Redis server");
      throw se;
    }
    auto redis_client = redis_client_wrapper->GetClient();
    std::vector<std::string> options{"NX"};
    std::string post_id_str = std::to_string(post_id);
    std::string timestamp_str = std::to_string(timestamp);
    std::multimap<std::string, std::string> value =
        {{timestamp_str, post_id_str}};

    // writes to the followers timeline
    for (auto &follower_id : followers_id_set) {
      redis_client->zadd(std::to_string(follower_id), options, value);
    }

    redis_client->sync_commit();
    redis_span->Finish();
    XTRACE("RedisUpdate complete");
    _redis_client_pool->Push(redis_client_wrapper);
  } catch (...) {
    LOG(error) << "OnReveived worker error";
    throw;
  }
  DELETE_CURRENT_BAGGAGE();
}

void HeartbeatSend(AmqpLibeventHandler &handler,
    AMQP::TcpConnection &connection, int interval){
  while(handler.GetIsRunning()){
    LOG(debug) << "Heartbeat sent";
    connection.heartbeat();
    sleep(interval);
  }
}

void WorkerThread(std::string &addr, int port) {
  AmqpLibeventHandler handler;
  AMQP::TcpConnection connection(handler, AMQP::Address(
      addr, port, AMQP::Login("guest", "guest"), "/"));
  AMQP::TcpChannel channel(&connection);
  channel.onError(
      [&handler](const char *message) {
        LOG(error) << "Channel error: " << message;
        handler.Stop();
      });
  channel.declareQueue("write-home-timeline", AMQP::durable).onSuccess(
      [&connection](const std::string &name, uint32_t messagecount,
                    uint32_t consumercount) {
        LOG(debug) << "Created queue: " << name;
      });
  channel.consume("write-home-timeline", AMQP::noack).onReceived(
      [](const AMQP::Message &msg, uint64_t tag, bool redelivered) {
        LOG(debug) << "Received: " << std::string(msg.body(), msg.bodySize());
        OnReceivedWorker(msg);
      });


  std::thread heartbeat_thread(HeartbeatSend, std::ref(handler),
      std::ref(connection), 30);
  heartbeat_thread.detach();
  handler.Start();
  LOG(debug) << "Closing connection.";
  connection.close();
}

int main(int argc, char *argv[]) {
  signal(SIGINT, sigintHandler);
  init_logger();

  SetUpTracer("config/jaeger-config.yml", "write-home-timeline-service");

  json config_json;
  if (load_config_file("config/service-config.json", &config_json) != 0) {
    exit(EXIT_FAILURE);
  }

  int port = config_json["write-home-timeline-service"]["port"];

  std::string rabbitmq_addr =
      config_json["write-home-timeline-rabbitmq"]["addr"];
  int rabbitmq_port = config_json["write-home-timeline-rabbitmq"]["port"];

  std::string redis_addr =
      config_json["home-timeline-redis"]["addr"];
  int redis_port = config_json["home-timeline-redis"]["port"];

  std::string social_graph_service_addr =
      config_json["social-graph-service"]["addr"];
  int social_graph_service_port = config_json["social-graph-service"]["port"];

  int antipode_oracle_port = config_json["antipode-oracle"]["port"];
  std::string antipode_oracle_addr = config_json["antipode-oracle"]["addr"];

  ClientPool<RedisClient> redis_client_pool("redis", redis_addr, redis_port,
                                            0, 128, 1000);

  ClientPool<ThriftClient<SocialGraphServiceClient>>
      social_graph_client_pool(
          "social-graph-service", social_graph_service_addr,
          social_graph_service_port, 0, 128, 1000);

  ClientPool<ThriftClient<AntipodeOracleClient>>
      antipode_oracle_client_pool("antipode-oracle", antipode_oracle_addr,
                                antipode_oracle_port, 0, 128, 1000);

  _redis_client_pool = &redis_client_pool;
  _social_graph_client_pool = &social_graph_client_pool;
  _antipode_oracle_client_pool = &antipode_oracle_client_pool;

  std::unique_ptr<std::thread> threads_ptr[NUM_WORKERS];
  for (auto & thread_ptr : threads_ptr) {
    thread_ptr = std::make_unique<std::thread>(
        WorkerThread, std::ref(rabbitmq_addr), rabbitmq_port);
  }
  for (auto &thread_ptr : threads_ptr) {
    thread_ptr->join();
    if (_teptr) {
      try{
        std::rethrow_exception(_teptr);
      }
      catch(const std::exception &ex)
      {
        LOG(error) << "Thread exited with exception: " << ex.what();
      }
    }
  }



  return 0;
}
