
#include <csignal>
#include <cpp_redis/cpp_redis>
#include <mutex>
#include <thread>
#include <sstream>
#include <set>
#include <chrono>

#include "../AmqpLibeventHandler.h"
#include "../ClientPool.h"
#include "../RedisClient.h"
#include "../ThriftClient.h"
#include "../logger.h"
#include "../tracing.h"
#include "../utils.h"
#include "../utils_mongodb.h"
#include "../antipode.h"
#include "../../gen-cpp/social_network_types.h"
#include "../../gen-cpp/SocialGraphService.h"
#include "../../gen-cpp/PostStorageService.h"
#include "../../gen-cpp/AntipodeOracle.h"
#include <xtrace/xtrace.h>
#include <xtrace/baggage.h>

// for thrift
#include <thrift/server/TThreadedServer.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include "WriteHomeTimelineService.h"

// #define NUM_WORKERS 4
// #define NUM_WORKERS 16
#define NUM_WORKERS 32

using apache::thrift::server::TThreadedServer;
using apache::thrift::transport::TServerSocket;
using apache::thrift::transport::TFramedTransportFactory;
using apache::thrift::protocol::TBinaryProtocolFactory;
using namespace social_network;
// for clock usage
using namespace std::chrono;

static std::string zone;

static std::exception_ptr _teptr;
static ClientPool<RedisClient> *_redis_client_pool;
static mongoc_client_pool_t *_mongodb_client_pool;
static ClientPool<ThriftClient<SocialGraphServiceClient>> *_social_graph_client_pool;
static ClientPool<ThriftClient<AntipodeOracleClient>> *_antipode_oracle_client_pool;
static ClientPool<ThriftClient<PostStorageServiceClient>> *_post_storage_client_pool;


void sigintHandler(int sig) {
  exit(EXIT_SUCCESS);
}

bool OnReceivedWorker(const AMQP::Message &msg) {
  high_resolution_clock::time_point start_worker_ts = high_resolution_clock::now();
  uint64_t ts = duration_cast<milliseconds>(start_worker_ts.time_since_epoch()).count();

  try {
    json msg_json = json::parse(std::string(msg.body(), msg.bodySize()));

    std::map<std::string, std::string> carrier;
    for (auto it = msg_json["carrier"].begin(); it != msg_json["carrier"].end(); ++it) {
      carrier.emplace(std::make_pair(it.key(), it.value()));
    }

    auto baggage_it = carrier.find("baggage");
    if (baggage_it != carrier.end()) {
      SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
    }

    // if (!XTrace::IsTracing()) {
    //   XTrace::StartTrace("WriteHomeTimelineService");
    // }

    // XTRACE("WriteHomeTimelineService::OnReceivedWorker");
    // Jaeger tracing
    TextMapReader span_reader(carrier);
    auto parent_span = opentracing::Tracer::Global()->Extract(span_reader);
    auto span = opentracing::Tracer::Global()->StartSpan(
        "FanoutHomeTimelines",
        {opentracing::ChildOf(parent_span->get())});
    std::map<std::string, std::string> writer_text_map;
    TextMapWriter writer(writer_text_map);
    opentracing::Tracer::Global()->Inject(span->context(), writer);

    // eval
    span->SetTag("wht_start_worker_ts", std::to_string(ts));

    // Extract information from rabbitmq messages
    int64_t user_id = msg_json["user_id"];
    int64_t req_id = msg_json["req_id"];
    int64_t post_id = msg_json["post_id"];
    int64_t timestamp = msg_json["timestamp"];
    std::vector<int64_t> user_mentions_id = msg_json["user_mentions_id"];

    //----------
    // -ANTIPODE
    //----------
    high_resolution_clock::time_point t1 = high_resolution_clock::now();
    // Cscope cscope = Cscope::from_json(msg_json["cscope_str"].dump());

    //----------
    // CENTRALIZED
    //----------
    // LOG(debug) << "[ANTIPODE][CENTRALIZED] Start IsVisible for post_id: " << post_id;
    // auto antipode_orable_client_wrapper = _antipode_oracle_client_pool->Pop();
    // if (!antipode_orable_client_wrapper) {
    //   ServiceException se;
    //   se.errorCode = ErrorCode::SE_THRIFT_CONN_ERROR;
    //   se.message = "[ANTIPODE][CENTRALIZED] Failed to connect to antipode-oracle";
    //   throw se;
    // }
    // auto antipode_oracle_client = antipode_orable_client_wrapper->GetClient();
    // // loop oracle calls until its visible
    // bool antipode_oracle_response = false;
    // try {
    //   antipode_oracle_response = antipode_oracle_client->IsVisible(post_id, writer_text_map);
    //   LOG(debug) << "[ANTIPODE][CENTRALIZED] Finished IsVisible for post_id: " << post_id;
    // } catch (...) {
    //   LOG(error) << "[ANTIPODE][CENTRALIZED] Failed to check post visibility in Oracle";
    //   _antipode_oracle_client_pool->Push(antipode_orable_client_wrapper);
    //   throw;
    // }
    // _antipode_oracle_client_pool->Push(antipode_orable_client_wrapper);
    //----------
    // CENTRALIZED
    //----------

    //----------
    // DISTRIBUTED
    //----------
    // mongoc_client_t *mongodb_client = mongoc_client_pool_pop(_mongodb_client_pool);
    // AntipodeMongodb antipode_client = AntipodeMongodb(mongodb_client, "post");

    // // gets cscopes with writes by caller
    // std::list<std::string> wanted_callers {"post-storage-service"};

    // cscope = antipode_client.barrier(cscope);

    // antipode_client.close();
    // mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
    //----------
    // DISTRIBUTED
    //----------

    //----------
    // EVAL CONSISTENCY ERRORS
    //----------

    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    ts = duration_cast<milliseconds>(t2.time_since_epoch()).count();
    span->SetTag("poststorage_read_notification_ts", std::to_string(ts));

    duration<double, std::milli> time_span = t2 - t1;
    span->SetTag("wht_antipode_duration", std::to_string(time_span.count()));

    //----------
    // EVAL CONSISTENCY ERRORS
    //----------

    //----------
    // -ANTIPODE
    //----------

    // force WritHomeTimeline to an error by sleeping
    // LOG(debug) << "[ANTIPODE] Sleeping ...";
    // std::this_thread ::sleep_for(std::chrono::milliseconds(10000));
    // LOG(debug) << "[ANTIPODE] Done Sleeping!";

    // read replica only ONCE while post is not ready at US replica
    // that way we dont stall workers
    bool read_post = false;
    high_resolution_clock::time_point mongoread_ts;
    while(!read_post) {
      mongoc_client_t *mongodb_client = mongoc_client_pool_pop(_mongodb_client_pool);
      if (!mongodb_client) {
        LOG(warning) << "[ANTIPODE] Could not open mongo connection: client";
        continue;
      }

      auto collection = mongoc_client_get_collection(mongodb_client, "post", "post");
      if (!collection) {
        mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
        LOG(warning) << "[ANTIPODE] Could not open mongo connection: collection";
        continue;
      }

      bson_t *query = bson_new();
      BSON_APPEND_INT64(query, "post_id", post_id);

      // add a read concern to opts
      // example: http://mongoc.org/libmongoc/current/mongoc_client_write_command_with_opts.html
      bson_t *opts = bson_new();
      mongoc_read_concern_t *read_concern = mongoc_read_concern_new();
      mongoc_read_concern_set_level (read_concern, MONGOC_READ_CONCERN_LEVEL_LOCAL);
      mongoc_read_concern_append (read_concern, opts);

      mongoc_read_prefs_t *read_prefs;
      read_prefs = mongoc_read_prefs_new(MONGOC_READ_SECONDARY);
      mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(collection, query, opts, read_prefs);

      const bson_t *doc;
      read_post = mongoc_cursor_next(cursor, &doc);

      bson_error_t error;
      if (mongoc_cursor_error (cursor, &error)) {
        LOG(error) << "An error occurred: " << error.message;
        continue;
      }

      //-------- debug
      // mongoc_host_list_t host;
      // mongoc_cursor_get_host (cursor, &host);
      // LOG(warning) << "READ FORM SERVER: " << host.host_and_port;
      // if (read_post) {
      //   char *as_json = bson_as_relaxed_extended_json (doc, NULL);
      //   LOG(error) << "READ DOCUMENT: " << as_json;
      //   bson_free (as_json);
      // }
      // LOG(debug) << "[ANTIPODE] Was post #" << post_id << " found at " << std::getenv("ZONE") << " replica? " << read_post;
      //-------- debug

      mongoread_ts = high_resolution_clock::now();
      bson_destroy(query);
      bson_destroy (opts);
      mongoc_read_prefs_destroy (read_prefs);
      mongoc_read_concern_destroy (read_concern);
      mongoc_cursor_destroy(cursor);
      mongoc_collection_destroy(collection);
      mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
      break; // if it got here it read the post
    }

    span->SetTag("consistency_bool", read_post);
    time_span = mongoread_ts - t2;
    span->SetTag("consistency_mongoread_duration", std::to_string(time_span.count()));

    //----------
    // -EVAL CONSISTENCY ERRORS
    //----------

    //----------
    // -ANTIPODE
    //----------

    // Find followers of the user
    auto social_graph_client_wrapper = _social_graph_client_pool->Pop();
    if (!social_graph_client_wrapper) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_THRIFT_CONN_ERROR;
      se.message = "Failed to connect to social-graph-service";
      // XTRACE("Failed to connect to social-graph-service");
      throw se;
    }
    auto social_graph_client = social_graph_client_wrapper->GetClient();
    std::vector<int64_t> followers_id;
    try {
      writer_text_map["baggage"] = BRANCH_CURRENT_BAGGAGE().str();
      UidListRpcResponse response;
      social_graph_client->GetFollowers(response, req_id, user_id, writer_text_map);
      followers_id = response.result;
      Baggage b = Baggage::deserialize(response.baggage);
      JOIN_CURRENT_BAGGAGE(b);
    } catch (...) {
      LOG(error) << "Failed to get followers from social-network-service";
      // XTRACE("Failed to get followers from social-network-service");
      _social_graph_client_pool->Push(social_graph_client_wrapper);
      throw;
    }
    _social_graph_client_pool->Push(social_graph_client_wrapper);

    std::set<int64_t> followers_id_set(followers_id.begin(), followers_id.end());
    followers_id_set.insert(user_mentions_id.begin(), user_mentions_id.end());

    // Update Redis ZSet
    // XTRACE("RedisUpdate start");
    auto redis_span = opentracing::Tracer::Global()->StartSpan(
        "RedisUpdate", {opentracing::ChildOf(&span->context())});
    auto redis_client_wrapper = _redis_client_pool->Pop();
    if (!redis_client_wrapper) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_REDIS_ERROR;
      se.message = "Cannot connect to Redis server";
      // XTRACE("Cannot connect to Redis server");
      throw se;
    }
    auto redis_client = redis_client_wrapper->GetClient();
    std::vector<std::string> options{"NX"};
    std::string post_id_str = std::to_string(post_id);
    std::string timestamp_str = std::to_string(timestamp);
    std::multimap<std::string, std::string> value = {{timestamp_str, post_id_str}};

    // writes to the followers timeline
    for (auto &follower_id : followers_id_set) {
      // LOG(debug) << "WRITING " << post_id_str << " TO FOLLOWER " << std::to_string(follower_id);
      redis_client->zadd(std::to_string(follower_id), options, value);
    }

    redis_client->sync_commit();
    redis_span->Finish();
    // XTRACE("RedisUpdate complete");
    _redis_client_pool->Push(redis_client_wrapper);

    DELETE_CURRENT_BAGGAGE();
    return true;
  } catch (...) {
    LOG(error) << "OnReceived worker error: " << boost::current_exception_diagnostic_information();
    return false;
  }
}

void HeartbeatSend(AmqpLibeventHandler &handler, AMQP::TcpConnection &connection, int interval){
  while(handler.GetIsRunning()){
    // LOG(debug) << "Heartbeat sent";
    connection.heartbeat();
    sleep(interval);
  }
}

void WorkerThread(std::string &addr, int port) {
  AmqpLibeventHandler handler;

  AMQP::TcpConnection connection(handler, AMQP::Address(addr, port, AMQP::Login("admin", "admin"), "/"));
  AMQP::TcpChannel channel(&connection);
  channel.onError(
      [&handler](const char *message) {
        LOG(error) << "Channel error: " << message;
        handler.Stop();
      });

  // with dead lettering you republish a rejected message
  // refs:
  //    https://www.rabbitmq.com/dlx.html#using-optional-queue-arguments
  //    https://github.com/CopernicaMarketingSoftware/AMQP-CPP/blob/master/README.md#flags-and-tables
  //    https://www.cloudamqp.com/blog/2020-12-29-when-and-how-to-use-the-rabbitmq-dead-letter-exchange.html

  // AMQP::Table arguments;
  // arguments["x-dead-letter-exchange"] = "write-home-timeline";

  channel.declareExchange("write-home-timeline", AMQP::topic);

  channel.declareQueue("write-home-timeline-"+zone, AMQP::durable).onSuccess(
      [&connection](const std::string &name, uint32_t messagecount, uint32_t consumercount) {
        LOG(debug) << "Created queue: " << name;
      }
    ).onError(
      [&handler](const char *message) {
        LOG(error) << "Error creating queue: " << message;
        // handler.Stop();
      });

  // params in order: <exchange>,<queue>,<routing_key>
  // queue and routing key are the same which is the current zone
  // other zones post messages to routing key of this zone
  channel.bindQueue("write-home-timeline", "write-home-timeline-" + zone, "write-home-timeline-" + zone);

  // ref: https://github.com/CopernicaMarketingSoftware/AMQP-CPP#consuming-messages
  // nack ref: https://github.com/CopernicaMarketingSoftware/AMQP-CPP/blob/master/include/amqpcpp/channel.h#L568
  // channel.consume("write-home-timeline-"+zone, AMQP::nolocal).onReceived(

  channel.consume("write-home-timeline-"+zone, AMQP::noack).onReceived(
      [&channel](const AMQP::Message &msg, uint64_t deliveryTag, bool redelivered) {
        LOG(debug) << "Received: " << std::string(msg.body(), msg.bodySize());
        OnReceivedWorker(msg);
        // if (!OnReceivedWorker(msg)) {
          // LOG(debug) << "MESSAGE RERROR: REPUBLISH";
          // channel.publish("write-home-timeline", "write-home-timeline-" + zone, msg);

          // acknowledge the message if true
          // channel.ack(deliveryTag);
          // LOG(debug) << "MESSAGE SUCCESS";

          // return to queue if false
          // LOG(debug) << "MESSAGE REJECT";
          // channel.reject(deliveryTag, AMQP::requeue);
          // channel.reject(deliveryTag);
        // }
      });

  std::thread heartbeat_thread(HeartbeatSend, std::ref(handler),
      std::ref(connection), 60);
  heartbeat_thread.detach();
  handler.Start();
  LOG(debug) << "Closing connection.";
  connection.close();
}

//--------------
// -ANTIPODE
//--------------
void mongoStreamListener(std::string uri, std::string dbname, std::string collection_name) {
  // init db and collection
  bson_error_t error;

  mongoc_init();
  mongoc_client_t* client = mongoc_client_new(uri.c_str());
  mongoc_database_t* db = mongoc_client_get_database(client, dbname.c_str());
  mongoc_collection_t* collection = mongoc_database_get_collection(db, collection_name.c_str());

  // refs:
  // http://mongoc.org/libmongoc/1.17.0/mongoc_change_stream_t.html
  // https://docs.mongodb.com/manual/changeStreams/
  // bson_t *pipeline = bson_new();
  bson_t *pipeline = BCON_NEW("pipeline",
    "[",
      "{",
        "$match", "{",
          "$and", "[",
            "{",
              "operationType", "insert",
            "}",
          "]",
        "}",
      "}",
    "]");

  // if the stream finds the same cscope
  mongoc_change_stream_t *stream;
  stream = mongoc_collection_watch (collection, pipeline, NULL /* opts */);
  LOG(debug) << "Listening to " << uri << ":" << dbname << ":" << collection_name << " changes...";
  while (true) {
    const bson_t *change;
    if (mongoc_change_stream_next (stream, &change)) {
      // for debug:
      // char *as_json = bson_as_relaxed_extended_json (change, NULL);
      // LOG(debug) << "CHANGE JSON: " << as_json;
      // bson_free (as_json);

      // PARTIAL EXAMPLE:
      //
      //      "clusterTime": {
      //        "$timestamp": {
      //          "t": 1627398240,
      //          "i": 16
      //        }
      //      },
      //      "fullDocument": {
      //        "_id": {
      //          "$oid": "61002060e789c6325e0b38d2"
      //        },
      //        "post_id": 784087681735860200,
      //        "timestamp": 1627398240777,
      //        //...
      //      }

      // parsing ref: http://mongoc.org/libbson/current/parsing.html
      bson_iter_t change_iter;
      bson_iter_t post_id_iter;
      bson_iter_t timestamp_iter;

      if (bson_iter_init (&change_iter, change) && bson_iter_find_descendant (&change_iter, "fullDocument.post_id", &post_id_iter) && BSON_ITER_HOLDS_INT64(&post_id_iter) &&
          bson_iter_init (&change_iter, change) && bson_iter_find_descendant (&change_iter, "fullDocument.timestamp", &timestamp_iter) && BSON_ITER_HOLDS_INT64(&timestamp_iter)
      ){
        int64_t post_id(bson_iter_int64(&post_id_iter));
        int64_t post_timestamp(bson_iter_int64(&timestamp_iter));
        uint64_t timestamp_now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        int64_t post_replication_diff = timestamp_now - post_timestamp;

        auto span = opentracing::Tracer::Global()->StartSpan("WriteHomeTimeline-MongoChangeStream");
        span->SetTag("composepost_id", std::to_string(post_id));
        span->SetTag("consistency_diff", std::to_string(post_replication_diff));
        span->Finish();
      }

      // clusterTimestamp is a second-based timestamp - as all mongodb internal timestamps are
      // hence its useless to use them as a base to compare delays in the ms scale
      //
      // if (bson_iter_init (&change_iter, change) && bson_iter_find_descendant (&change_iter, "clusterTime", &cluster_timestamp_iter) && BSON_ITER_HOLDS_TIMESTAMP(&cluster_timestamp_iter)) {
      //   uint32_t cluster_timestamp;
      //   bson_iter_timestamp(&cluster_timestamp_iter, &cluster_timestamp, nullptr);
      //   uint64_t timestamp_now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
      //   LOG(debug) << "CLUSTER TIMESTAMP: " << cluster_timestamp << " VS " << timestamp_now;
      // }
    }
  }

  // const bson_t *resume_token;
  // bson_error_t error;
  // if (mongoc_change_stream_error_document (stream, &error, NULL)) {
  //   MONGOC_ERROR ("%s\n", error.message);
  // }

  mongoc_change_stream_destroy (stream);
  bson_destroy(pipeline);
  mongoc_collection_destroy(collection);
  mongoc_database_destroy(db);
  mongoc_client_destroy (client);
}
//--------------
// -ANTIPODE
//--------------

// The function we want to execute on the new thread.
void serveThriftServer(int port, std::string name) {
  // for thrift
  TThreadedServer server(
      std::make_shared<WriteHomeTimelineServiceProcessor>(std::make_shared<WriteHomeTimelineServiceHandler>()),
      std::make_shared<TServerSocket>("0.0.0.0", port),
      std::make_shared<TFramedTransportFactory>(),
      std::make_shared<TBinaryProtocolFactory>()
  );
  LOG(debug) << "Starting the " << name << " server...";
  server.serve();
}

int main(int argc, char *argv[]) {
  signal(SIGINT, sigintHandler);
  init_logger();

  zone = load_zone();

  std::string service_name = "write-home-timeline-service-" + zone;

  SetUpTracer("config/jaeger-config.yml", service_name);

  json config_json;
  if (load_config_file("config/service-config.json", &config_json) != 0) {
    exit(EXIT_FAILURE);
  }

  int port = config_json[service_name]["port"];

  std::string rabbitmq_addr = config_json["write-home-timeline-rabbitmq-" + zone]["addr"];
  int rabbitmq_port = config_json["write-home-timeline-rabbitmq-" + zone]["port"];

  int post_storage_port = config_json["post-storage-service-" + zone]["port"];
  std::string post_storage_addr = config_json["post-storage-service-" + zone]["addr"];

  std::string redis_addr = config_json["home-timeline-redis-" + zone]["addr"];
  int redis_port = config_json["home-timeline-redis-" + zone]["port"];

  // Should it be on the same region?
  std::string social_graph_service_addr = config_json["social-graph-service"]["addr"];
  int social_graph_service_port = config_json["social-graph-service"]["port"];

  int antipode_oracle_port = config_json["antipode-oracle"]["port"];
  std::string antipode_oracle_addr = config_json["antipode-oracle"]["addr"];


  ClientPool<RedisClient> redis_client_pool("redis", redis_addr, redis_port,
                                            0, 10000, 1000);

  _mongodb_client_pool = init_mongodb_client_pool(config_json, "post-storage", zone, 1024);
  if (_mongodb_client_pool == nullptr) {
    return EXIT_FAILURE;
  }

  // get latest server description
  // mongoc_client_t *mongodb_client = mongoc_client_pool_pop(_mongodb_client_pool);
  // if (!mongodb_client) {
  //   LOG(fatal) << "Failed to pop mongoc client";
  //   return EXIT_FAILURE;
  // }
  // mongoc_server_description_t *sd;
  // sd = mongoc_client_get_server_description(mongodb_client, 1);
  // if (sd == NULL) {
  //   LOG(fatal) << "Failed to fetch server description";
  //   return EXIT_FAILURE;
  // }
  // LOG(debug) << "HOST AND PORT = " << mongoc_server_description_host(sd)->host_and_port;
  // mongoc_server_description_destroy(sd);
  // mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);

  //----------
  // +ANTIPODE
  //----------
  std::string mongodb_uri = mongodb_dsb_uri(config_json, "post-storage", zone);

  // init antipode tables
  // AntipodeMongodb::init_cscope_listener(mongodb_uri, "post");

  // std::thread tmongo_stream_listener(mongoStreamListener, mongodb_uri, "post", "post");
  //----------
  // +ANTIPODE
  //----------

  ClientPool<ThriftClient<SocialGraphServiceClient>>
      social_graph_client_pool(
          "social-graph-service", social_graph_service_addr,
          social_graph_service_port, 0, 10000, 1000);

  ClientPool<ThriftClient<AntipodeOracleClient>>
      antipode_oracle_client_pool("antipode-oracle", antipode_oracle_addr,
                                antipode_oracle_port, 0, 10000, 1000);

  ClientPool<ThriftClient<PostStorageServiceClient>>
      post_storage_client_pool("post-storage-client", post_storage_addr,
                               post_storage_port, 0, 10000, 1000);


  _redis_client_pool = &redis_client_pool;
  _social_graph_client_pool = &social_graph_client_pool;
  _antipode_oracle_client_pool = &antipode_oracle_client_pool;
  _post_storage_client_pool = &post_storage_client_pool;

  std::unique_ptr<std::thread> threads_ptr[NUM_WORKERS];
  for (auto & thread_ptr : threads_ptr) {
    thread_ptr = std::make_unique<std::thread>(
        WorkerThread, std::ref(rabbitmq_addr), rabbitmq_port);
  }

  // Constructs the new thread and runs it. Does not block execution.
  std::thread tserver_thread(serveThriftServer, port, service_name);
  // serveThriftServer(port, service_name);

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

  // wait for server to finish
  tserver_thread.join();

  return 0;
}
