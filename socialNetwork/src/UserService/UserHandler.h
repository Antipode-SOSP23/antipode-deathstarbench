#ifndef SOCIAL_NETWORK_MICROSERVICES_USERHANDLER_H
#define SOCIAL_NETWORK_MICROSERVICES_USERHANDLER_H

#include <iostream>
#include <string>
#include <random>
#include <mongoc.h>
#include <libmemcached/memcached.h>
#include <libmemcached/util.h>
#include <bson/bson.h>
#include <iomanip>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <nlohmann/json.hpp>
#include <jwt/jwt.hpp>

#include "../../gen-cpp/UserService.h"
#include "../../gen-cpp/ComposePostService.h"
#include "../../gen-cpp/SocialGraphService.h"
#include "../../third_party/PicoSHA2/picosha2.h"
#include "../ClientPool.h"
#include "../ThriftClient.h"
#include "../tracing.h"
#include "../logger.h"
#include <xtrace/xtrace.h>
#include <xtrace/baggage.h>

// Custom Epoch (January 1, 2018 Midnight GMT = 2018-01-01T00:00:00Z)
#define CUSTOM_EPOCH 1514764800000
#define MONGODB_TIMEOUT_MS 100

namespace social_network {

using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::duration_cast;
using std::chrono::system_clock;
using namespace jwt::params;

static int64_t current_timestamp = -1;
static int counter = 0;

static int GetCounter(int64_t timestamp) {
  if (current_timestamp > timestamp) {
    LOG(fatal) << "Timestamps are not incremental.";
    exit(EXIT_FAILURE);
  }
  if (current_timestamp == timestamp) {
    return counter++;
  } else {
    current_timestamp = timestamp;
    counter = 0;
    return counter++;
  }
}

std::string GenRandomString(const int len) {
  static const std::string alphanum =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> dist(
      0, static_cast<int>(alphanum.length() - 1));
  std::string s;
  for (int i = 0; i < len; ++i) {
    s += alphanum[dist(gen)];
  }
  return s;
}

class UserHandler : public UserServiceIf {
 public:
  UserHandler(
      std::mutex*,
      const std::string &,
      const std::string &,
      memcached_pool_st *,
      mongoc_client_pool_t *,
      ClientPool<ThriftClient<ComposePostServiceClient>> *,
      ClientPool<ThriftClient<SocialGraphServiceClient>> *);
  ~UserHandler() override = default;
  void RegisterUser(
      BaseRpcResponse &,
      int64_t,
      const std::string &,
      const std::string &,
      const std::string &,
      const std::string &,
      const std::map<std::string, std::string> &) override;
  void RegisterUserWithId(
      BaseRpcResponse &,
      int64_t,
      const std::string &,
      const std::string &,
      const std::string &,
      const std::string &,
      int64_t,
      const std::map<std::string, std::string> &) override;

  void UploadCreatorWithUserId(
      BaseRpcResponse &,
      int64_t,
      int64_t,
      const std::string &,
      const std::map<std::string, std::string> &) override;
  void UploadCreatorWithUsername(
      BaseRpcResponse &,
      int64_t,
      const std::string &,
      const std::map<std::string, std::string> &) override;
  void Login(
      LoginRpcResponse&,
      int64_t,
      const std::string &,
      const std::string &,
      const std::map<std::string, std::string> &) override;
  void GetUserId(
      UserIdRpcResponse&,
      int64_t,
      const std::string &,
      const std::map<std::string, std::string> &) override;
 private:
  std::string _machine_id;
  std::string _secret;
  std::mutex *_thread_lock;
  memcached_pool_st *_memcached_client_pool;
  mongoc_client_pool_t *_mongodb_client_pool;
  ClientPool<ThriftClient<ComposePostServiceClient>> *_compose_client_pool;
  ClientPool<ThriftClient<SocialGraphServiceClient>> *_social_graph_client_pool;

};

UserHandler::UserHandler(
    std::mutex *thread_lock,
    const std::string &machine_id,
    const std::string &secret,
    memcached_pool_st *memcached_client_pool,
    mongoc_client_pool_t *mongodb_client_pool,
    ClientPool<ThriftClient<ComposePostServiceClient>> *compose_client_pool,
    ClientPool<ThriftClient<SocialGraphServiceClient>> *social_graph_client_pool
    ) {
  _thread_lock = thread_lock;
  _machine_id = machine_id;
  _memcached_client_pool = memcached_client_pool;
  _mongodb_client_pool = mongodb_client_pool;
  _compose_client_pool = compose_client_pool;
  _secret = secret;
  _social_graph_client_pool = social_graph_client_pool;
}

void UserHandler::RegisterUserWithId(
    BaseRpcResponse &response,
    const int64_t req_id,
    const std::string &first_name,
    const std::string &last_name,
    const std::string &username,
    const std::string &password,
    const int64_t user_id,
    const std::map<std::string, std::string> &carrier) {

  auto baggage_it = carrier.find("baggage");
  if (baggage_it != carrier.end()) {
    SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
  }

  if (!XTrace::IsTracing()) {
    XTrace::StartTrace("UserHandler-RegisterUserWithId");
  }

  XTRACE("UserHandler::RegisterUserWithId", {{"RequestID", std::to_string(req_id)}});
  // Initialize a span
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map;
  TextMapWriter writer(writer_text_map);
  auto parent_span = opentracing::Tracer::Global()->Extract(reader);
  auto span = opentracing::Tracer::Global()->StartSpan(
      "RegisterUserWithId",
      { opentracing::ChildOf(parent_span->get()) });
  opentracing::Tracer::Global()->Inject(span->context(), writer);

  // Store user info into mongodb
  mongoc_client_t *mongodb_client = mongoc_client_pool_pop(
      _mongodb_client_pool);
  if (!mongodb_client) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_MONGODB_ERROR;
    se.message = "Failed to pop a client from MongoDB pool";
    XTRACE("Failed to pop a client from MongoDB pool");
    throw se;
  }
  auto collection = mongoc_client_get_collection(
      mongodb_client, "user", "user");

  // Check if the username has existed in the database
  bson_t *query = bson_new();
  BSON_APPEND_UTF8(query, "username", username.c_str());
  mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
      collection, query, nullptr, nullptr);
  const bson_t *doc;
  bson_error_t error;
  bool found = mongoc_cursor_next(cursor, &doc);
  if (mongoc_cursor_error (cursor, &error)) {
    LOG(warning) << error.message;
    bson_destroy(query);
    mongoc_cursor_destroy(cursor);
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
    ServiceException se;
    se.errorCode = ErrorCode::SE_MONGODB_ERROR;
    se.message = error.message;
    throw se;
  } else if (found) {
    LOG(warning) << "User " << username << " already existed.";
    ServiceException se;
    se.errorCode = ErrorCode::SE_THRIFT_HANDLER_ERROR;
    se.message = "User " + username + " already existed";
    bson_destroy(query);
    mongoc_cursor_destroy(cursor);
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
    XTRACE("User " + username + " already existed");
    throw se;
  } else {
    bson_t *new_doc = bson_new();
    BSON_APPEND_INT64(new_doc, "user_id", user_id);
    BSON_APPEND_UTF8(new_doc, "first_name", first_name.c_str());
    BSON_APPEND_UTF8(new_doc, "last_name", last_name.c_str());
    BSON_APPEND_UTF8(new_doc, "username", username.c_str());
    std::string salt = GenRandomString(32);
    BSON_APPEND_UTF8(new_doc, "salt", salt.c_str());
    std::string password_hashed = picosha2::hash256_hex_string(password + salt);
    BSON_APPEND_UTF8(new_doc, "password", password_hashed.c_str());

    bson_error_t error;
    XTRACE("MongoInsertUser start");
    auto user_insert_span = opentracing::Tracer::Global()->StartSpan(
        "MongoInsertUser", { opentracing::ChildOf(&span->context()) });
    if (!mongoc_collection_insert_one(
        collection, new_doc, nullptr, nullptr, &error)) {
      LOG(error) << "Failed to insert user " << username
                 << " to MongoDB: " << error.message;
      ServiceException se;
      se.errorCode = ErrorCode::SE_THRIFT_HANDLER_ERROR;
      se.message = "Failed to insert user " + username + " to MongoDB: "
          + error.message;
      bson_destroy(query);
      mongoc_cursor_destroy(cursor);
      mongoc_collection_destroy(collection);
      mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
      throw se;
    } else {
      LOG(debug) << "User: " << username << " registered";
      XTRACE("User: " + username + " registered");
    }
    user_insert_span->Finish();
    XTRACE("MongoInsertUser complete");
    bson_destroy(new_doc);
  }
  bson_destroy(query);
  mongoc_cursor_destroy(cursor);
  mongoc_collection_destroy(collection);
  mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);

  if(!found) {
    auto social_graph_client_wrapper = _social_graph_client_pool->Pop();
    if (!social_graph_client_wrapper) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_THRIFT_CONN_ERROR;
      se.message = "Failed to connect to social-graph-service";
      XTRACE("Failed to connect to social-graph-service");
      throw se;
    }
    auto social_graph_client = social_graph_client_wrapper->GetClient();
    try {
      writer_text_map["baggage"] = BRANCH_CURRENT_BAGGAGE().str();
      social_graph_client->InsertUser(response, req_id, user_id, writer_text_map);
      Baggage b = Baggage::deserialize(response.baggage);
      JOIN_CURRENT_BAGGAGE(b);
    } catch (...) {
      _social_graph_client_pool->Push(social_graph_client_wrapper);
      LOG(error) << "Failed to insert user to social-graph-client";
      XTRACE("Failed to insert user to social-graph-client");
      throw;
    }
    _social_graph_client_pool->Push(social_graph_client_wrapper);
  }

  span->Finish();
  XTRACE("UserHandler::RegisterUserWithUserId complete");
  response.baggage = GET_CURRENT_BAGGAGE().str();
  DELETE_CURRENT_BAGGAGE();
}

void UserHandler::RegisterUser(
    BaseRpcResponse &response,
    const int64_t req_id,
    const std::string &first_name,
    const std::string &last_name,
    const std::string &username,
    const std::string &password,
    const std::map<std::string, std::string> &carrier) {

  auto baggage_it = carrier.find("baggage");
  if (baggage_it != carrier.end()) {
    SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
  }

  if (!XTrace::IsTracing()) {
    XTrace::StartTrace("UserHandler-RegisterUser");
  }

  XTRACE("UserHandler::RegisterUser", {{"RequestID", std::to_string(req_id)}});
  // Initialize a span
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map;
  TextMapWriter writer(writer_text_map);
  auto parent_span = opentracing::Tracer::Global()->Extract(reader);
  auto span = opentracing::Tracer::Global()->StartSpan(
      "RegisterUser",
      { opentracing::ChildOf(parent_span->get()) });
  opentracing::Tracer::Global()->Inject(span->context(), writer);

  // Compose user_id
  int64_t timestamp = duration_cast<milliseconds>(
      system_clock::now().time_since_epoch()).count() - CUSTOM_EPOCH;
  std::stringstream sstream;
  sstream << std::hex << timestamp;
  std::string timestamp_hex(sstream.str());
  if (timestamp_hex.size() > 10) {
    timestamp_hex.erase(0, timestamp_hex.size() - 10);
  } else if (timestamp_hex.size() < 10) {
    timestamp_hex =
        std::string(10 - timestamp_hex.size(), '0') + timestamp_hex;
  }
  _thread_lock->lock();
  int idx = GetCounter(timestamp);
  _thread_lock->unlock();
  // Empty the sstream buffer.
  sstream.clear();
  sstream.str(std::string());

  sstream << std::hex << idx;
  std::string counter_hex(sstream.str());

  if (counter_hex.size() > 3) {
    counter_hex.erase(0, counter_hex.size() - 3);
  } else if (counter_hex.size() < 3) {
    counter_hex = std::string(3 - counter_hex.size(), '0') + counter_hex;
  }
  std::string user_id_str = _machine_id + timestamp_hex + counter_hex;
  int64_t user_id = stoul(user_id_str, nullptr, 16) & 0x7FFFFFFFFFFFFFFF;;
  LOG(debug) << "The user_id of the request " << req_id << " is " << user_id;
  XTRACE("User_id of the request is " + std::to_string(req_id));

  // Store user info into mongodb
  mongoc_client_t *mongodb_client = mongoc_client_pool_pop(
      _mongodb_client_pool);
  if (!mongodb_client) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_MONGODB_ERROR;
    se.message = "Failed to pop a client from MongoDB pool";
    XTRACE("Failed to pop a client from MongoDB pool");
    throw se;
  }
  auto collection = mongoc_client_get_collection(
      mongodb_client, "user", "user");

  // Check if the username has existed in the database
  bson_t *query = bson_new();
  BSON_APPEND_UTF8(query, "username", username.c_str());
  mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
      collection, query, nullptr, nullptr);
  const bson_t *doc;
  bson_error_t error;
  bool found = mongoc_cursor_next(cursor, &doc);
  if (mongoc_cursor_error (cursor, &error)) {
    LOG(error) << error.message;
    bson_destroy(query);
    mongoc_cursor_destroy(cursor);
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
    ServiceException se;
    se.errorCode = ErrorCode::SE_MONGODB_ERROR;
    se.message = error.message;
    throw se;
  } else if (found) {
    LOG(warning) << "User " << username << " already existed.";
    ServiceException se;
    se.errorCode = ErrorCode::SE_THRIFT_HANDLER_ERROR;
    se.message = "User " + username + " already existed";
    bson_destroy(query);
    mongoc_cursor_destroy(cursor);
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
    XTRACE("User " + username + " already existed");
    throw se;
  } else {
    bson_t *new_doc = bson_new();
    BSON_APPEND_INT64(new_doc, "user_id", user_id);
    BSON_APPEND_UTF8(new_doc, "first_name", first_name.c_str());
    BSON_APPEND_UTF8(new_doc, "last_name", last_name.c_str());
    BSON_APPEND_UTF8(new_doc, "username", username.c_str());
    std::string salt = GenRandomString(32);
    BSON_APPEND_UTF8(new_doc, "salt", salt.c_str());
    std::string password_hashed = picosha2::hash256_hex_string(password + salt);
    BSON_APPEND_UTF8(new_doc, "password", password_hashed.c_str());

    XTRACE("MongoInsertUser start");
    auto user_insert_span = opentracing::Tracer::Global()->StartSpan(
        "MongoInsertUser", { opentracing::ChildOf(&span->context()) });
    if (!mongoc_collection_insert_one(
        collection, new_doc, nullptr, nullptr, &error)) {
      LOG(error) << "Failed to insert user " << username
          << " to MongoDB: " << error.message;
      ServiceException se;
      se.errorCode = ErrorCode::SE_THRIFT_HANDLER_ERROR;
      se.message = "Failed to insert user " + username + " to MongoDB: "
          + error.message;
      bson_destroy(query);
      mongoc_cursor_destroy(cursor);
      mongoc_collection_destroy(collection);
      mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
      XTRACE("Failed to insert user " + username + " to MongoDB");
      throw se;
    } else {
      LOG(debug) << "User: " << username << " registered";
      XTRACE("User: " + username + " registered");
    }
    user_insert_span->Finish();
    XTRACE("MongoInsertUser complete");
    bson_destroy(new_doc);
  }
  bson_destroy(query);
  mongoc_cursor_destroy(cursor);
  mongoc_collection_destroy(collection);
  mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);

  if(!found) {
    auto social_graph_client_wrapper = _social_graph_client_pool->Pop();
    if (!social_graph_client_wrapper) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_THRIFT_CONN_ERROR;
      se.message = "Failed to connect to social-graph-service";
      XTRACE("Failed to connect to social-graph-service");
      throw se;
    }
    auto social_graph_client = social_graph_client_wrapper->GetClient();
    try {
      writer_text_map["baggage"] = BRANCH_CURRENT_BAGGAGE().str();
      social_graph_client->InsertUser(response, req_id, user_id, writer_text_map);
      Baggage b = Baggage::deserialize(response.baggage);
      JOIN_CURRENT_BAGGAGE(b);
    } catch (...) {
      _social_graph_client_pool->Push(social_graph_client_wrapper);
      LOG(error) << "Failed to insert user to social-graph-service";
      XTRACE("Failed to insert user to social-graph-service");
      throw;
    }

    _social_graph_client_pool->Push(social_graph_client_wrapper);
  }

  span->Finish();
  XTRACE("UserService::RegisterUser complete");
  response.baggage = GET_CURRENT_BAGGAGE().str();
  DELETE_CURRENT_BAGGAGE();
}

void UserHandler::UploadCreatorWithUsername(
    BaseRpcResponse &response,
    const int64_t req_id,
    const std::string &username,
    const std::map<std::string, std::string> & carrier) {

  auto baggage_it = carrier.find("baggage");
  if (baggage_it != carrier.end()) {
    SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
  }

  if (!XTrace::IsTracing()) {
    XTrace::StartTrace("UserHandler-UploadCreatorWithUsername");
  }

  XTRACE("UserHandler::UploadCreatorWithUsername", {{"RequestID", std::to_string(req_id)}});
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map;
  TextMapWriter writer(writer_text_map);
  auto parent_span = opentracing::Tracer::Global()->Extract(reader);
  auto span = opentracing::Tracer::Global()->StartSpan(
      "UploadUserWithUsername",
      { opentracing::ChildOf(parent_span->get()) });
  opentracing::Tracer::Global()->Inject(span->context(), writer);

  size_t user_id_size;
  uint32_t memcached_flags;

  memcached_return_t memcached_rc;
  memcached_st *memcached_client = memcached_pool_pop(
      _memcached_client_pool, true, &memcached_rc);
  char *user_id_mmc;
  if (memcached_client) {
    XTRACE("MemcahedGetUserId start");
    auto id_get_span = opentracing::Tracer::Global()->StartSpan(
        "MmcGetUserId", { opentracing::ChildOf(&span->context()) });
    user_id_mmc = memcached_get(
        memcached_client,
        (username+":user_id").c_str(),
        (username+":user_id").length(),
        &user_id_size,
        &memcached_flags,
        &memcached_rc);
    id_get_span->Finish();
    XTRACE("MemcachedGetUserId end");
    if (!user_id_mmc && memcached_rc != MEMCACHED_NOTFOUND) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_MEMCACHED_ERROR;
      se.message = memcached_strerror(memcached_client, memcached_rc);
      memcached_pool_push(_memcached_client_pool, memcached_client);
      throw se;
    }
    memcached_pool_push(_memcached_client_pool, memcached_client);
  } else {
    LOG(warning) << "Failed to pop a client from memcached pool";
    XTRACE("Failed to pop a client from memcached pool");
  }

  int64_t user_id = -1;
  bool cached = false;
  if (user_id_mmc) {
    cached = true;
    LOG(debug) << "Found user_id of username :" << username  << " in Memcached";
    XTRACE("Found user_id of username :" + username + " in Memcached");
    user_id = std::stoul(user_id_mmc);
    free(user_id_mmc);
  }

  // If not cached in memcached
  else {
    LOG(debug) << "user_id not cached in Memcached";
    XTRACE("user_id not cached in Memcached");
    mongoc_client_t *mongodb_client = mongoc_client_pool_pop(
        _mongodb_client_pool);
    if (!mongodb_client) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_MONGODB_ERROR;
      se.message = "Failed to pop a client from MongoDB pool";
      XTRACE("Failed to pop a client from MongoDB pool");
      throw se;
    }
    auto collection = mongoc_client_get_collection(
        mongodb_client, "user", "user");
    if (!collection) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_MONGODB_ERROR;
      se.message = "Failed to create collection user from DB user";
      XTRACE("Failed to create collection user from DB user");
      throw se;
    }
    bson_t *query = bson_new();
    BSON_APPEND_UTF8(query, "username", username.c_str());

    XTRACE("MongoFindUser start");
    auto find_span = opentracing::Tracer::Global()->StartSpan(
        "MongoFindUser", { opentracing::ChildOf(&span->context()) });
    mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
        collection, query, nullptr, nullptr);
    const bson_t *doc;
    bool found = mongoc_cursor_next(cursor, &doc);
    find_span->Finish();
    XTRACE("MongoFindUser complete");
    if (!found) {
      bson_error_t error;
      if (mongoc_cursor_error (cursor, &error)) {
        LOG(error) << error.message;
        bson_destroy(query);
        mongoc_cursor_destroy(cursor);
        mongoc_collection_destroy(collection);
        mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
        ServiceException se;
        se.errorCode = ErrorCode::SE_MONGODB_ERROR;
        se.message = error.message;
        throw se;
      } else {
        LOG(warning) << "User: " << username << " doesn't exist in MongoDB";
        bson_destroy(query);
        mongoc_cursor_destroy(cursor);
        mongoc_collection_destroy(collection);
        mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
        ServiceException se;
        se.errorCode = ErrorCode::SE_THRIFT_HANDLER_ERROR;
        se.message = "User: " + username + " is not registered";
        XTRACE("User " + username + " is not registered");
        throw se;
      }
    } else {
      LOG(debug) << "User: " << username << " found in MongoDB";
      bson_iter_t iter;
      if (bson_iter_init_find(&iter, doc, "user_id")) {
        user_id = bson_iter_value(&iter)->value.v_int64;
      } else {
        LOG(error) << "user_id attribute of user "
                   << username <<" was not found in the User object";
        bson_destroy(query);
        mongoc_cursor_destroy(cursor);
        mongoc_collection_destroy(collection);
        mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
        ServiceException se;
        se.errorCode = ErrorCode::SE_THRIFT_HANDLER_ERROR;
        se.message = "user_id attribute of user: " + username +
            " was not found in the User object";
        XTRACE("user_id attribute of user: " + username + " was not found in the User object");
        throw se;
      }
    }
    bson_destroy(query);
    mongoc_cursor_destroy(cursor);
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
  }

  Creator creator;
  creator.username = username;
  creator.user_id = user_id;

  if (user_id != -1) {
    auto compose_post_client_wrapper = _compose_client_pool->Pop();
    if (!compose_post_client_wrapper) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_THRIFT_CONN_ERROR;
      se.message = "Failed to connect to compose-post-service";
      XTRACE("Failed to connect to compose-post-service");
      throw se;
    }
    auto compose_post_client = compose_post_client_wrapper->GetClient();
    try {
      writer_text_map["baggage"] = BRANCH_CURRENT_BAGGAGE().str();
      compose_post_client->UploadCreator(response, req_id, creator, writer_text_map);
      Baggage b = Baggage::deserialize(response.baggage);
      JOIN_CURRENT_BAGGAGE(b);
    } catch (...) {
      _compose_client_pool->Push(compose_post_client_wrapper);
      LOG(error) << "Failed to upload creator to compose-post-service";
      XTRACE("Failed to upload creator to compose-post-service");
      throw;
    }
    _compose_client_pool->Push(compose_post_client_wrapper);

  }

  memcached_client = memcached_pool_pop(
      _memcached_client_pool, true, &memcached_rc);
  if (memcached_client) {
    if (user_id != -1 && !cached) {
      XTRACE("MemcachedSetUserId start");
      auto id_set_span = opentracing::Tracer::Global()->StartSpan(
          "MmcSetUserId", { opentracing::ChildOf(&span->context()) });
      std::string user_id_str = std::to_string(user_id);
      memcached_rc = memcached_set(
          memcached_client,
          (username+":user_id").c_str(),
          (username+":user_id").length(),
          user_id_str.c_str(),
          user_id_str.length(),
          static_cast<time_t>(0),
          static_cast<uint32_t>(0)
      );
      id_set_span->Finish();
      XTRACE("MemcachedSetUserId complete");
      if (memcached_rc != MEMCACHED_SUCCESS) {
        LOG(warning)
          << "Failed to set the user_id of user "
          << username << " to Memcached: "
          << memcached_strerror(memcached_client, memcached_rc);
        XTRACE("Failed to set the user_id of user " + username + " to Memcached");
      }
    }
    memcached_pool_push(_memcached_client_pool, memcached_client);
  } else {
    LOG(warning) << "Failed to pop a client from memcached pool";
    XTRACE("Failed to pop a client from memcached pool");
  }
  span->Finish();
  XTRACE("UserHandler::UploadCreatorWithUsername complete");
  response.baggage = GET_CURRENT_BAGGAGE().str();
  DELETE_CURRENT_BAGGAGE();
}

void UserHandler::UploadCreatorWithUserId(
    BaseRpcResponse &response,
    int64_t req_id,
    int64_t user_id,
    const std::string &username,
    const std::map<std::string, std::string> &carrier) {

  auto baggage_it = carrier.find("baggage");
  if (baggage_it != carrier.end()) {
    SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
  }

  if (!XTrace::IsTracing()) {
    XTrace::StartTrace("UserHandler-UploadCreatorWithUserId");
  }

  XTRACE("UserHandler::UploadCreatorWithUserId", {{"RequestID", std::to_string(req_id)}});
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map;
  TextMapWriter writer(writer_text_map);
  auto parent_span = opentracing::Tracer::Global()->Extract(reader);
  auto span = opentracing::Tracer::Global()->StartSpan(
      "UploadUserWithUserId",
      { opentracing::ChildOf(parent_span->get()) });
  opentracing::Tracer::Global()->Inject(span->context(), writer);

  Creator creator;
  creator.username = username;
  creator.user_id = user_id;

  auto compose_post_client_wrapper = _compose_client_pool->Pop();
  if (!compose_post_client_wrapper) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_THRIFT_CONN_ERROR;
    se.message = "Failed to connect to compose-post-service";
    XTRACE("Failed to connect to compose-post-service");
    throw se;
  }
  auto compose_post_client = compose_post_client_wrapper->GetClient();
  try {
    writer_text_map["baggage"] = BRANCH_CURRENT_BAGGAGE().str();
    compose_post_client->UploadCreator(response, req_id, creator, writer_text_map);
    Baggage b = Baggage::deserialize(response.baggage);
    JOIN_CURRENT_BAGGAGE(b);
  } catch (...) {
    _compose_client_pool->Push(compose_post_client_wrapper);
    LOG(error) << "Failed to upload creator to compose-post-service";
    XTRACE("Failed to upload creator to compose-post-service");
    throw;
  }

  _compose_client_pool->Push(compose_post_client_wrapper);

  span->Finish();
  XTRACE("UploadCreatorWithUserId complete");
  response.baggage = GET_CURRENT_BAGGAGE().str();
  DELETE_CURRENT_BAGGAGE();
}


void UserHandler::Login(
    LoginRpcResponse& response,
    int64_t req_id,
    const std::string &username,
    const std::string &password,
    const std::map<std::string, std::string> &carrier) {

  std::string _return;
  auto baggage_it = carrier.find("baggage");
  if (baggage_it != carrier.end()) {
    SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
  }

  if (!XTrace::IsTracing()) {
    XTrace::StartTrace("UserHandler-Login");
  }

  XTRACE("UserHandler::Login", {{"RequestID", std::to_string(req_id)}});
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map;
  TextMapWriter writer(writer_text_map);
  auto parent_span = opentracing::Tracer::Global()->Extract(reader);
  auto span = opentracing::Tracer::Global()->StartSpan(
      "Login",
      { opentracing::ChildOf(parent_span->get()) });
  opentracing::Tracer::Global()->Inject(span->context(), writer);

  size_t login_size;
  uint32_t memcached_flags;

  memcached_return_t memcached_rc;
  memcached_st *memcached_client = memcached_pool_pop(
      _memcached_client_pool, true, &memcached_rc);
  char *login_mmc;
  if (!memcached_client) {
    LOG(warning) << "Failed to pop a client from memcached pool";
    XTRACE("Failed to pop a client from memcached pool");
  } else {
    XTRACE("MemcachedGetLogin start");
    auto get_login_span = opentracing::Tracer::Global()->StartSpan(
        "MmcGetLogin", { opentracing::ChildOf(&span->context()) });
    login_mmc = memcached_get(
        memcached_client,
        (username + ":login").c_str(),
        (username + ":login").length(),
        &login_size,
        &memcached_flags,
        &memcached_rc);
    get_login_span->Finish();
    if (!login_mmc && memcached_rc != MEMCACHED_NOTFOUND) {
      LOG(warning) << "Memcached error: "
          << memcached_strerror(memcached_client, memcached_rc);
    }
    memcached_pool_push(_memcached_client_pool, memcached_client);
  }

  std::string password_stored;
  std::string salt_stored;
  int64_t user_id_stored = -1;
  bool cached = false;
  json login_json;

  if (login_mmc) {
    // If not cached in memcached
    LOG(debug) << "Found username: "<< username <<" in Memcached";
    XTRACE("Found username: " + username + " in Memcached");
    login_json = json::parse(std::string(login_mmc, login_size));
    password_stored = login_json["password"];
    salt_stored = login_json["salt"];
    user_id_stored = login_json["user_id"];
    cached = true;
    free(login_mmc);
  }

  else {
    // If not cached in memcached
    LOG(debug) << "Username: " << username << " NOT cached in Memcached";
    XTRACE("Username " + username + " NOT cached in Memcached");
    mongoc_client_t *mongodb_client = mongoc_client_pool_pop(
        _mongodb_client_pool);
    if (!mongodb_client) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_MONGODB_ERROR;
      se.message = "Failed to pop a client from MongoDB pool";
      XTRACE("Failed to pop a client from MongoDB pool");
      throw se;
    }
    auto collection = mongoc_client_get_collection(
        mongodb_client, "user", "user");
    if (!collection) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_MONGODB_ERROR;
      se.message = "Failed to create collection user from DB user";
      XTRACE("Failed to create collection user from DB user");
      throw se;
    }
    bson_t *query = bson_new();
    BSON_APPEND_UTF8(query, "username", username.c_str());

    XTRACE("MongoFindUser start");
    auto find_span = opentracing::Tracer::Global()->StartSpan(
        "MongoFindUser", {opentracing::ChildOf(&span->context())});
    mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
        collection, query, nullptr, nullptr);
    const bson_t *doc;
    bool found = mongoc_cursor_next(cursor, &doc);
    find_span->Finish();
    XTRACE("MongoFindUser complete");

    bson_error_t error;
    if (mongoc_cursor_error(cursor, &error)) {
      LOG(error) << error.message;
      bson_destroy(query);
      mongoc_cursor_destroy(cursor);
      mongoc_collection_destroy(collection);
      mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
      ServiceException se;
      se.errorCode = ErrorCode::SE_MONGODB_ERROR;
      se.message = error.message;
      throw se;
    }

    if (!found) {
      LOG(warning) << "User: " << username << " doesn't exist in MongoDB";
      bson_destroy(query);
      mongoc_cursor_destroy(cursor);
      mongoc_collection_destroy(collection);
      mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
      ServiceException se;
      se.errorCode = ErrorCode::SE_UNAUTHORIZED;
      se.message = "User: " + username + " is not registered";
      XTRACE("Username " + username + " doesn't exist in MongoDB");
      throw se;

    } else {
      LOG(debug) << "Username: " << username << " found in MongoDB";
      XTRACE("Username: " + username + " found in MongoDB");
      bson_iter_t iter_password;
      bson_iter_t iter_salt;
      bson_iter_t iter_user_id;
      if (bson_iter_init_find(&iter_password, doc, "password") &&
          bson_iter_init_find(&iter_salt, doc, "salt") &&
          bson_iter_init_find(&iter_user_id, doc, "user_id")) {
        password_stored = bson_iter_value(&iter_password)->value.v_utf8.str;
        salt_stored = bson_iter_value(&iter_salt)->value.v_utf8.str;
        user_id_stored = bson_iter_value(&iter_user_id)->value.v_int64;
        login_json["password"] = password_stored;
        login_json["salt"] = salt_stored;
        login_json["user_id"] = user_id_stored;
      } else {
        LOG(error) << "user: " << username << " entry is NOT complete";
        bson_destroy(query);
        mongoc_cursor_destroy(cursor);
        mongoc_collection_destroy(collection);
        mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
        ServiceException se;
        se.errorCode = ErrorCode::SE_THRIFT_HANDLER_ERROR;
        se.message = "user: " + username + " entry is NOT complete";
        XTRACE("User: " + username + " entry is NOT complete");
        throw se;
      }
      bson_destroy(query);
      mongoc_cursor_destroy(cursor);
      mongoc_collection_destroy(collection);
      mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
    }
  }

  if (user_id_stored != -1 && !salt_stored.empty()
      && !password_stored.empty()) {
    bool auth = picosha2::hash256_hex_string(password + salt_stored)
        == password_stored;
    if (auth) {
      auto user_id_str = std::to_string(user_id_stored);
      auto timestamp_str = std::to_string(duration_cast<seconds>(
          system_clock::now().time_since_epoch()).count());
      jwt::jwt_object obj{
          algorithm("HS256"),
          secret(_secret),
          payload({
              {"user_id", user_id_str},
              {"username", username},
              {"timestamp", timestamp_str},
              {"ttl", "3600"}
          })
      };
      _return = obj.signature();

    } else {
      ServiceException se;
      se.errorCode = ErrorCode::SE_UNAUTHORIZED;
      se.message = "Incorrect username or password";
      XTRACE("incorrect username or password");
      throw se;
    }
  } else {
    ServiceException se;
    se.errorCode = ErrorCode::SE_THRIFT_HANDLER_ERROR;
    se.message = "Username: " + username + " incomplete login information.";
    XTRACE("Username: " + username + " incomplete login information");
    throw se;
  }

  if (!cached) {
    memcached_client = memcached_pool_pop(
        _memcached_client_pool, true, &memcached_rc);
    if (!memcached_client) {
      LOG(warning) << "Failed to pop a client from memcached pool";
      XTRACE("Failed to pop a client from memcached pool");
    } else {
      XTRACE("MemcachedSetLogin start");
      auto set_login_span = opentracing::Tracer::Global()->StartSpan(
          "MmcSetLogin", { opentracing::ChildOf(&span->context()) });
      std::string login_str = login_json.dump();
      memcached_rc = memcached_set(
          memcached_client,
          (username+":login").c_str(),
          (username+":login").length(),
          login_str.c_str(),
          login_str.length(),
          0,
          0
      );
      set_login_span->Finish();
      XTRACE("MemcachedSetLogin complete");
      if (memcached_rc != MEMCACHED_SUCCESS) {
        LOG(warning)
          << "Failed to set the login info of user "
          << username << " to Memcached: "
          << memcached_strerror(memcached_client, memcached_rc);
        XTRACE("Failed to set the login info of user " + username + " to Memcached");
      }
      memcached_pool_push(_memcached_client_pool, memcached_client);
    }
  }
  span->Finish();
  XTRACE("UserService::Login complete");
  response.baggage = GET_CURRENT_BAGGAGE().str();
  DELETE_CURRENT_BAGGAGE();
}

void UserHandler::GetUserId(
    UserIdRpcResponse& response,
    int64_t req_id,
    const std::string &username,
    const std::map<std::string, std::string> &carrier) {

  auto baggage_it = carrier.find("baggage");
  if (baggage_it != carrier.end()) {
    SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
  }

  if (!XTrace::IsTracing()) {
    XTrace::StartTrace("UserHandler-GetUserId");
  }

  XTRACE("UserHandler::GetUserId", {{"RequestID", std::to_string(req_id)}});
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map;
  TextMapWriter writer(writer_text_map);
  auto parent_span = opentracing::Tracer::Global()->Extract(reader);
  auto span = opentracing::Tracer::Global()->StartSpan(
      "GetUserId",
      { opentracing::ChildOf(parent_span->get()) });
  opentracing::Tracer::Global()->Inject(span->context(), writer);

  size_t user_id_size;
  uint32_t memcached_flags;

  memcached_return_t memcached_rc;
  memcached_st *memcached_client = memcached_pool_pop(
      _memcached_client_pool, true, &memcached_rc);
  char *user_id_mmc;
  if (memcached_client) {
    XTRACE("MemcachedGetUserId start");
    auto id_get_span = opentracing::Tracer::Global()->StartSpan(
        "MmcGetUserId", { opentracing::ChildOf(&span->context()) });
    user_id_mmc = memcached_get(
        memcached_client,
        (username+":user_id").c_str(),
        (username+":user_id").length(),
        &user_id_size,
        &memcached_flags,
        &memcached_rc);
    id_get_span->Finish();
    if (!user_id_mmc && memcached_rc != MEMCACHED_NOTFOUND) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_MEMCACHED_ERROR;
      se.message = memcached_strerror(memcached_client, memcached_rc);
      memcached_pool_push(_memcached_client_pool, memcached_client);
      throw se;
    }
    memcached_pool_push(_memcached_client_pool, memcached_client);
  } else {
    LOG(warning) << "Failed to pop a client from memcached pool";
    XTRACE("Failed to pop a client from memcached pool");
  }

  int64_t user_id = -1;
  bool cached = false;
  if (user_id_mmc) {
    cached = true;
    LOG(debug) << "Found user_id of username :" << username  << " in Memcached";
    XTRACE("Found user_id of username :" + username + " in Memcached");
    user_id = std::stoul(user_id_mmc);
    free(user_id_mmc);
  } else {
    // If not cached in memcached
    LOG(debug) << "user_id not cached in Memcached";
    XTRACE("user_id not cached in Memcached");
    mongoc_client_t *mongodb_client = mongoc_client_pool_pop(
        _mongodb_client_pool);
    if (!mongodb_client) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_MONGODB_ERROR;
      se.message = "Failed to pop a client from MongoDB pool";
      XTRACE("Failed to pop a client from MongoDB pool");
      throw se;
    }
    auto collection = mongoc_client_get_collection(
        mongodb_client, "user", "user");
    if (!collection) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_MONGODB_ERROR;
      se.message = "Failed to create collection user from DB user";
      XTRACE("Failed to pop a client from MongoDB pool");
      throw se;
    }
    bson_t *query = bson_new();
    BSON_APPEND_UTF8(query, "username", username.c_str());

    XTRACE("MongoFindUser start");
    auto find_span = opentracing::Tracer::Global()->StartSpan(
        "MongoFindUser", { opentracing::ChildOf(&span->context()) });
    mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
        collection, query, nullptr, nullptr);
    const bson_t *doc;
    bool found = mongoc_cursor_next(cursor, &doc);
    find_span->Finish();
    XTRACE("MongoFindUser complete");
    if (!found) {
      bson_error_t error;
      if (mongoc_cursor_error (cursor, &error)) {
        LOG(error) << error.message;
        bson_destroy(query);
        mongoc_cursor_destroy(cursor);
        mongoc_collection_destroy(collection);
        mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
        ServiceException se;
        se.errorCode = ErrorCode::SE_MONGODB_ERROR;
        se.message = error.message;
        throw se;
      } else {
        LOG(warning) << "User: " << username << " doesn't exist in MongoDB";
        bson_destroy(query);
        mongoc_cursor_destroy(cursor);
        mongoc_collection_destroy(collection);
        mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
        ServiceException se;
        se.errorCode = ErrorCode::SE_THRIFT_HANDLER_ERROR;
        se.message = "User: " + username + " is not registered";
        XTRACE("User: " + username + " is not registered");
        throw se;
      }
    } else {
      LOG(debug) << "User: " << username << " found in MongoDB";
      XTRACE("User: " + username + " found in MongoDB");
      bson_iter_t iter;
      if (bson_iter_init_find(&iter, doc, "user_id")) {
        user_id = bson_iter_value(&iter)->value.v_int64;
      } else {
        LOG(error) << "user_id attribute of user "
                   << username <<" was not found in the User object";
        bson_destroy(query);
        mongoc_cursor_destroy(cursor);
        mongoc_collection_destroy(collection);
        mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
        ServiceException se;
        se.errorCode = ErrorCode::SE_THRIFT_HANDLER_ERROR;
        se.message = "user_id attribute of user: " + username +
            " was not found in the User object";
        XTRACE("user_id attribute of user: " + username + " was not found in the User object");
        throw se;
      }
    }
    bson_destroy(query);
    mongoc_cursor_destroy(cursor);
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
  }

  if (!cached) {
    memcached_client = memcached_pool_pop(
        _memcached_client_pool, true, &memcached_rc);
    if (!memcached_client) {
      LOG(warning) << "Failed to pop a client from memcached pool";
      XTRACE("Failed to pop a client from memcached pool");
    } else {
      XTRACE("MemcachedSetUserId start");
      std::string user_id_str = std::to_string(user_id);
      auto set_login_span = opentracing::Tracer::Global()->StartSpan(
          "MmcSetUserId", { opentracing::ChildOf(&span->context()) });
      memcached_rc = memcached_set(
          memcached_client,
          (username+":user_id").c_str(),
          (username+":user_id").length(),
          user_id_str.c_str(),
          user_id_str.length(),
          0,
          0
      );
      set_login_span->Finish();
      XTRACE("MemcachedSetUserId complete");
      if (memcached_rc != MEMCACHED_SUCCESS) {
        LOG(warning)
          << "Failed to set the login info of user "
          << username << " to Memcached: "
          << memcached_strerror(memcached_client, memcached_rc);
      }
      memcached_pool_push(_memcached_client_pool, memcached_client);
    }
  }

  span->Finish();
  XTRACE("UserHandler::GetUserId complete");
  response.baggage = GET_CURRENT_BAGGAGE().str();
  response.result = user_id;
  DELETE_CURRENT_BAGGAGE();
}

/*
 * The following code which obtaines machine ID from machine's MAC address was
 * inspired from https://stackoverflow.com/a/16859693.
 */
u_int16_t HashMacAddressPid(const std::string &mac)
{
  u_int16_t hash = 0;
  std::string mac_pid = mac + std::to_string(getpid());
  for ( unsigned int i = 0; i < mac_pid.size(); i++ ) {
    hash += ( mac[i] << (( i & 1 ) * 8 ));
  }
  return hash;
}

int GetMachineId (std::string *mac_hash) {
  std::string mac;
  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP );
  if ( sock < 0 ) {
    LOG(error) << "Unable to obtain MAC address";
    return -1;
  }

  struct ifconf conf{};
  char ifconfbuf[ 128 * sizeof(struct ifreq)  ];
  memset( ifconfbuf, 0, sizeof( ifconfbuf ));
  conf.ifc_buf = ifconfbuf;
  conf.ifc_len = sizeof( ifconfbuf );
  if ( ioctl( sock, SIOCGIFCONF, &conf ))
  {
    LOG(error) << "Unable to obtain MAC address";
    return -1;
  }

  struct ifreq* ifr;
  for (
      ifr = conf.ifc_req;
      reinterpret_cast<char *>(ifr) <
          reinterpret_cast<char *>(conf.ifc_req) + conf.ifc_len;
      ifr++) {
    if ( ifr->ifr_addr.sa_data == (ifr+1)->ifr_addr.sa_data ) {
      continue;  // duplicate, skip it
    }

    if ( ioctl( sock, SIOCGIFFLAGS, ifr )) {
      continue;  // failed to get flags, skip it
    }
    if ( ioctl( sock, SIOCGIFHWADDR, ifr ) == 0 ) {
      mac = std::string(ifr->ifr_addr.sa_data);
      if (!mac.empty()) {
        break;
      }
    }
  }
  close(sock);

  std::stringstream stream;
  stream << std::hex << HashMacAddressPid(mac);
  *mac_hash = stream.str();

  if (mac_hash->size() > 3) {
    mac_hash->erase(0, mac_hash->size() - 3);
  } else if (mac_hash->size() < 3) {
    *mac_hash = std::string(3 - mac_hash->size(), '0') + *mac_hash;
  }
  return 0;
}
} // namespace social_network

#endif //SOCIAL_NETWORK_MICROSERVICES_USERHANDLER_H
