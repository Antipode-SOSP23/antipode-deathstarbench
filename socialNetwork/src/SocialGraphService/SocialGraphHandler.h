#ifndef SOCIAL_NETWORK_MICROSERVICES_SOCIALGRAPHHANDLER_H
#define SOCIAL_NETWORK_MICROSERVICES_SOCIALGRAPHHANDLER_H

#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <vector>

#include <mongoc.h>
#include <bson/bson.h>
#include <cpp_redis/cpp_redis>

#include "../../gen-cpp/SocialGraphService.h"
#include "../../gen-cpp/UserService.h"
#include "../ClientPool.h"
#include "../logger.h"
#include "../tracing.h"
#include "../RedisClient.h"
#include "../ThriftClient.h"
#include <xtrace/xtrace.h>
#include <xtrace/baggage.h>

namespace social_network {

using std::chrono::milliseconds;
using std::chrono::duration_cast;
using std::chrono::system_clock;

class SocialGraphHandler : public SocialGraphServiceIf {
 public:
  SocialGraphHandler(
      mongoc_client_pool_t *,
      ClientPool<RedisClient> *,
      ClientPool<ThriftClient<UserServiceClient>> *);
  ~SocialGraphHandler() override = default;
  void GetFollowers(UidListRpcResponse &, int64_t, int64_t,
                    const std::map<std::string, std::string> &) override;
  void GetFollowees(UidListRpcResponse &, int64_t, int64_t,
                    const std::map<std::string, std::string> &) override;
  void Follow(BaseRpcResponse &, int64_t, int64_t, int64_t,
              const std::map<std::string, std::string> &) override;
  void Unfollow(BaseRpcResponse &, int64_t, int64_t, int64_t,
                const std::map<std::string, std::string> &) override;
  void FollowWithUsername(BaseRpcResponse &, int64_t, const std::string &, const std::string &,
              const std::map<std::string, std::string> &) override;
  void UnfollowWithUsername(BaseRpcResponse &, int64_t, const std::string &, const std::string &,
                const std::map<std::string, std::string> &) override;
  void InsertUser(BaseRpcResponse&, int64_t, int64_t,
                  const std::map<std::string, std::string> &) override;


 private:
  mongoc_client_pool_t *_mongodb_client_pool;
  ClientPool<RedisClient> *_redis_client_pool;
  ClientPool<ThriftClient<UserServiceClient>> *_user_service_client_pool;
};

SocialGraphHandler::SocialGraphHandler(
    mongoc_client_pool_t *mongodb_client_pool,
    ClientPool<RedisClient> *redis_client_pool,
    ClientPool<ThriftClient<UserServiceClient>> *user_service_client_pool) {
  _mongodb_client_pool = mongodb_client_pool;
  _redis_client_pool = redis_client_pool;
  _user_service_client_pool = user_service_client_pool;
}

void SocialGraphHandler::Follow(
    BaseRpcResponse &response,
    int64_t req_id,
    int64_t user_id,
    int64_t followee_id,
    const std::map<std::string, std::string> &carrier) {

  auto baggage_it = carrier.find("baggage");
  if (baggage_it != carrier.end()) {
    SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
  }

  if (!XTrace::IsTracing()) {
    XTrace::StartTrace("SocialGraphHandler");
  }

  // XTRACE("SocialGraphHandler::Follow", {{"RequestID", std::to_string(req_id)}});
  // Initialize a span
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map;
  TextMapWriter writer(writer_text_map);
  auto parent_span = opentracing::Tracer::Global()->Extract(reader);
  auto span = opentracing::Tracer::Global()->StartSpan(
      "Follow",
      {opentracing::ChildOf(parent_span->get())});
  opentracing::Tracer::Global()->Inject(span->context(), writer);

  int64_t timestamp = duration_cast<milliseconds>(
      system_clock::now().time_since_epoch()).count();

  Baggage mongo_update_baggage = BRANCH_CURRENT_BAGGAGE();
  std::future<void> mongo_update_follower_future = std::async(
      std::launch::async, [&]() {
        BAGGAGE(mongo_update_baggage);
        mongoc_client_t *mongodb_client = mongoc_client_pool_pop(
            _mongodb_client_pool);
        if (!mongodb_client) {
          ServiceException se;
          se.errorCode = ErrorCode::SE_MONGODB_ERROR;
          se.message = "Failed to pop a client from MongoDB pool";
          // XTRACE("Failed to pop a client from MongoDB pool");
          throw se;
        }
        auto collection = mongoc_client_get_collection(
            mongodb_client, "social-graph", "social-graph");
        if (!collection) {
          ServiceException se;
          se.errorCode = ErrorCode::SE_MONGODB_ERROR;
          se.message = "Failed to create collection social_graph from MongoDB";
          mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
          // XTRACE("Failed to create collection social_graph from MongoDB");
          throw se;
        }

        // Update follower->followee edges
        // XTRACE("Updating follower->followee edges");
        const bson_t *doc;
        bson_t *search_not_exist = BCON_NEW(
            "$and", "[",
            "{", "user_id", BCON_INT64(user_id), "}", "{",
            "followees", "{", "$not", "{", "$elemMatch", "{",
            "user_id", BCON_INT64(followee_id), "}", "}", "}", "}", "]"
        );
        bson_t *update = BCON_NEW(
            "$push",
            "{",
            "followees",
            "{",
            "user_id",
            BCON_INT64(followee_id),
            "timestamp",
            BCON_INT64(timestamp),
            "}",
            "}"
        );
        bson_error_t error;
        bson_t reply;
        // XTRACE("MongoUpdateFollower start");
        auto update_span = opentracing::Tracer::Global()->StartSpan(
            "MongoUpdateFollower", {opentracing::ChildOf(&span->context())});
        bool updated = mongoc_collection_find_and_modify(
            collection,
            search_not_exist,
            nullptr,
            update,
            nullptr,
            false,
            false,
            true,
            &reply,
            &error);
        if (!updated) {
          LOG(error) << "Failed to update social graph for user " << user_id
                     << " to MongoDB: " << error.message;
          ServiceException se;
          se.errorCode = ErrorCode::SE_MONGODB_ERROR;
          se.message = error.message;
          bson_destroy(&reply);
          bson_destroy(update);
          bson_destroy(search_not_exist);
          mongoc_collection_destroy(collection);
          mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
          // XTRACE("Failed to update social graph for user");
          throw se;
        }
        update_span->Finish();
        bson_destroy(&reply);
        bson_destroy(update);
        bson_destroy(search_not_exist);
        mongoc_collection_destroy(collection);
        mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
        // XTRACE("MongoUpdateFollower complete");
      });

  Baggage mongo_update_followee_baggage = BRANCH_CURRENT_BAGGAGE();
  std::future<void> mongo_update_followee_future = std::async(
      std::launch::async, [&]() {
        BAGGAGE(mongo_update_followee_baggage);
        mongoc_client_t *mongodb_client = mongoc_client_pool_pop(
            _mongodb_client_pool);
        if (!mongodb_client) {
          ServiceException se;
          se.errorCode = ErrorCode::SE_MONGODB_ERROR;
          se.message = "Failed to pop a client from MongoDB pool";
          // XTRACE("Failed to pop a client from MongoDB pool");
          throw se;
        }
        auto collection = mongoc_client_get_collection(
            mongodb_client, "social-graph", "social-graph");
        if (!collection) {
          ServiceException se;
          se.errorCode = ErrorCode::SE_MONGODB_ERROR;
          se.message = "Failed to create collection social_graph from MongoDB";
          // XTRACE("Failed to create collection social_graph from MongoDB");
          mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
          throw se;
        }

        // Update followee->follower edges
        bson_t *search_not_exist = BCON_NEW(
            "$and", "[", "{", "user_id", BCON_INT64(followee_id), "}", "{",
            "followers", "{", "$not", "{", "$elemMatch", "{",
            "user_id", BCON_INT64(user_id), "}", "}", "}", "}", "]"
        );
        bson_t *update = BCON_NEW(
            "$push", "{", "followers", "{", "user_id", BCON_INT64(user_id),
            "timestamp", BCON_INT64(timestamp), "}", "}"
        );
        bson_error_t error;
        // XTRACE("MongoUpdateFollowee start");
        auto update_span = opentracing::Tracer::Global()->StartSpan(
            "MongoUpdateFollowee", {opentracing::ChildOf(&span->context())});
        bson_t reply;
        bool updated = mongoc_collection_find_and_modify(
            collection, search_not_exist, nullptr, update, nullptr, false,
            false, true, &reply, &error);
        if (!updated) {
          LOG(error) << "Failed to update social graph for user "
                     << followee_id << " to MongoDB: " << error.message;
          ServiceException se;
          se.errorCode = ErrorCode::SE_MONGODB_ERROR;
          se.message = error.message;
          bson_destroy(update);
          bson_destroy(&reply);
          bson_destroy(search_not_exist);
          mongoc_collection_destroy(collection);
          mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
          // XTRACE("Failed to update social graph for user");
          throw se;
        }
        update_span->Finish();
        // XTRACE("MongoUpdateFollowee complete");
        bson_destroy(update);
        bson_destroy(&reply);
        bson_destroy(search_not_exist);
        mongoc_collection_destroy(collection);
        mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
      });

  Baggage redis_update_baggage = BRANCH_CURRENT_BAGGAGE();
  std::future<void> redis_update_future = std::async(
      std::launch::async, [&]() {
        BAGGAGE(redis_update_baggage);
        auto redis_client_wrapper = _redis_client_pool->Pop();
        if (!redis_client_wrapper) {
          ServiceException se;
          se.errorCode = ErrorCode::SE_REDIS_ERROR;
          se.message = "Cannot connect to Redis server";
          // XTRACE("Cannot connect to Redis server");
          throw se;
        }
        auto redis_client = redis_client_wrapper->GetClient();

        // XTRACE("RedisUpdate start");
        auto redis_span = opentracing::Tracer::Global()->StartSpan(
            "RedisUpdate", {opentracing::ChildOf(&span->context())});
        auto num_followee = redis_client->zcard(
            std::to_string(user_id) + ":followees");
        auto num_follower = redis_client->zcard(
            std::to_string(followee_id) + ":followers");
        redis_client->sync_commit();
        auto num_followee_reply = num_followee.get();
        auto num_follower_reply = num_follower.get();

        std::vector<std::string> options{"NX"};
        if (num_followee_reply.ok() && num_followee_reply.as_integer()) {
          std::string key = std::to_string(user_id) + ":followees";
          std::multimap<std::string, std::string> value = {{
            std::to_string(timestamp), std::to_string(followee_id)}};
          redis_client->zadd(key, options, value);
        }
        if (num_follower_reply.ok() && num_follower_reply.as_integer()) {
          std::string key = std::to_string(followee_id) + ":followers";
          std::multimap<std::string, std::string> value = {
              {std::to_string(timestamp), std::to_string(user_id)}};
          redis_client->zadd(key, options, value);
        }
        redis_client->sync_commit();
        _redis_client_pool->Push(redis_client_wrapper);
        redis_span->Finish();
        // XTRACE("RedisUpdate complete");
      });

  try {
    redis_update_future.get();
    JOIN_CURRENT_BAGGAGE(redis_update_baggage);
    mongo_update_follower_future.get();
    JOIN_CURRENT_BAGGAGE(mongo_update_baggage);
    mongo_update_followee_future.get();
    JOIN_CURRENT_BAGGAGE(mongo_update_followee_baggage);
  } catch (...) {
    throw;
  }

  span->Finish();
  // XTRACE("SocialGraphHandler::Follow complete");
  response.baggage = GET_CURRENT_BAGGAGE().str();
  DELETE_CURRENT_BAGGAGE();
}

void SocialGraphHandler::Unfollow(
    BaseRpcResponse& response,
    int64_t req_id,
    int64_t user_id,
    int64_t followee_id,
    const std::map<std::string, std::string> &carrier) {

  auto baggage_it = carrier.find("baggage");
  if (baggage_it != carrier.end()) {
    SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
  }

  if (!XTrace::IsTracing()) {
    XTrace::StartTrace("SocialGraphHandler");
  }

  // XTRACE("SocialGraphHandler::Unfollow", {{"RequestID", std::to_string(req_id)}});
  // Initialize a span
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map;
  TextMapWriter writer(writer_text_map);
  auto parent_span = opentracing::Tracer::Global()->Extract(reader);
  auto span = opentracing::Tracer::Global()->StartSpan(
      "Unfollow",
      {opentracing::ChildOf(parent_span->get())});
  opentracing::Tracer::Global()->Inject(span->context(), writer);

  Baggage mongo_update_follower_baggage = BRANCH_CURRENT_BAGGAGE();
  std::future<void> mongo_update_follower_future = std::async(
      std::launch::async, [&]() {
        BAGGAGE(mongo_update_follower_baggage);
        mongoc_client_t *mongodb_client = mongoc_client_pool_pop(
            _mongodb_client_pool);
        if (!mongodb_client) {
          ServiceException se;
          se.errorCode = ErrorCode::SE_MONGODB_ERROR;
          se.message = "Failed to pop a client from MongoDB pool";
          // XTRACE("Failed to pop a client from MongoDB pool");
          throw se;
        }
        auto collection = mongoc_client_get_collection(
            mongodb_client, "social-graph", "social-graph");
        if (!collection) {
          ServiceException se;
          se.errorCode = ErrorCode::SE_MONGODB_ERROR;
          se.message = "Failed to create collection social_graph from MongoDB";
          // XTRACE("Failed to pop a client from MongoDB pool");
          mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
          throw se;
        }
        bson_t *query = bson_new();

        // Update follower->followee edges
        BSON_APPEND_INT64(query, "user_id", user_id);
        bson_t *update = BCON_NEW(
            "$pull", "{", "followees", "{",
            "user_id", BCON_INT64(followee_id), "}", "}"
        );
        bson_t reply;
        bson_error_t error;
        // XTRACE("MongoDeleteFollowee start");
        auto update_span = opentracing::Tracer::Global()->StartSpan(
            "MongoDeleteFollowee", {opentracing::ChildOf(&span->context())});
        bool updated = mongoc_collection_find_and_modify(
            collection, query, nullptr, update, nullptr, false, false,
            true, &reply, &error);
        if (!updated) {
          LOG(error) << "Failed to delete social graph for user " << user_id
                     << " to MongoDB: " << error.message;
          ServiceException se;
          se.errorCode = ErrorCode::SE_MONGODB_ERROR;
          se.message = error.message;
          bson_destroy(update);
          bson_destroy(query);
          bson_destroy(&reply);
          mongoc_collection_destroy(collection);
          mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
          // XTRACE("Failed to delete social graph for user " + std::to_string(user_id));
          throw se;
        }
        update_span->Finish();
        // XTRACE("MongoDeleteFollowee compelte");
        bson_destroy(update);
        bson_destroy(query);
        bson_destroy(&reply);
        mongoc_collection_destroy(collection);
        mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
      });

  Baggage mongo_update_followee_baggage = BRANCH_CURRENT_BAGGAGE();
  std::future<void> mongo_update_followee_future = std::async(
      std::launch::async, [&]() {
        BAGGAGE(mongo_update_followee_baggage);
        mongoc_client_t *mongodb_client = mongoc_client_pool_pop(
            _mongodb_client_pool);
        if (!mongodb_client) {
          ServiceException se;
          se.errorCode = ErrorCode::SE_MONGODB_ERROR;
          se.message = "Failed to pop a client from MongoDB pool";
          // XTRACE("Failed to pop a client from MongoDB pool");
          throw se;
        }
        auto collection = mongoc_client_get_collection(
            mongodb_client, "social-graph", "social-graph");
        if (!collection) {
          ServiceException se;
          se.errorCode = ErrorCode::SE_MONGODB_ERROR;
          se.message = "Failed to create collection social_graph from MongoDB";
          // XTRACE("Failed to create collection social_graph from MongoDB");
          mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
          throw se;
        }
        bson_t *query = bson_new();

        // Update followee->follower edges
        BSON_APPEND_INT64(query, "user_id", followee_id);
        bson_t *update = BCON_NEW(
            "$pull", "{", "followers", "{",
            "user_id", BCON_INT64(user_id), "}", "}"
        );
        bson_t reply;
        bson_error_t error;
        // XTRACE("MongoDeleteFollower start");
        auto update_span = opentracing::Tracer::Global()->StartSpan(
            "MongoDeleteFollower", {opentracing::ChildOf(&span->context())});
        bool updated = mongoc_collection_find_and_modify(
            collection, query, nullptr, update, nullptr, false, false,
            true, &reply, &error);
        if (!updated) {
          LOG(error) << "Failed to delete social graph for user " << followee_id
                     << " to MongoDB: " << error.message;
          ServiceException se;
          se.errorCode = ErrorCode::SE_MONGODB_ERROR;
          se.message = error.message;
          bson_destroy(update);
          bson_destroy(query);
          bson_destroy(&reply);
          mongoc_collection_destroy(collection);
          mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
          // XTRACE("Failed to delete social graph for user " + std::to_string(followee_id));
          throw se;
        }
        update_span->Finish();
        // XTRACE("MongoDeleteFollower complete");
        bson_destroy(update);
        bson_destroy(query);
        bson_destroy(&reply);
        mongoc_collection_destroy(collection);
        mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
      });

  Baggage redis_update_baggage = BRANCH_CURRENT_BAGGAGE();
  std::future<void> redis_update_future = std::async(
      std::launch::async, [&]() {
        BAGGAGE(redis_update_baggage);
        auto redis_client_wrapper = _redis_client_pool->Pop();
        if (!redis_client_wrapper) {
          ServiceException se;
          se.errorCode = ErrorCode::SE_REDIS_ERROR;
          se.message = "Cannot connect to Redis server";
          // XTRACE("Cannot connect to Redis server");
          throw se;
        }
        auto redis_client = redis_client_wrapper->GetClient();

        // XTRACE("RedisUpdate start");
        auto redis_span = opentracing::Tracer::Global()->StartSpan(
            "RedisUpdate", {opentracing::ChildOf(&span->context())});
        auto num_followee = redis_client->zcard(
            std::to_string(user_id) + ":followees");
        auto num_follower = redis_client->zcard(
            std::to_string(followee_id) + ":followers");
        redis_client->sync_commit();
        auto num_followee_reply = num_followee.get();
        auto num_follower_reply = num_follower.get();

        if (num_followee_reply.ok() && num_followee_reply.as_integer()) {
          std::string key = std::to_string(user_id) + ":followees";
          std::vector<std::string> members{std::to_string(followee_id)};
          redis_client->zrem(key, members);
        }
        if (num_follower_reply.ok() && num_follower_reply.as_integer()) {
          std::string key = std::to_string(followee_id) + ":followers";
          std::vector<std::string> members{std::to_string(user_id)};
          redis_client->zrem(key, members);
        }
        redis_client->sync_commit();
        _redis_client_pool->Push(redis_client_wrapper);
        redis_span->Finish();
        // XTRACE("RedisUpdate complete");
      });

  try {
    redis_update_future.get();
    JOIN_CURRENT_BAGGAGE(redis_update_baggage);
    mongo_update_follower_future.get();
    JOIN_CURRENT_BAGGAGE(mongo_update_follower_baggage);
    mongo_update_followee_future.get();
    JOIN_CURRENT_BAGGAGE(mongo_update_followee_baggage);
  } catch (...) {
    throw;
  }

  span->Finish();
  // XTRACE("SocialGraphHandler::Unfollow complete");
  response.baggage = GET_CURRENT_BAGGAGE().str();
  DELETE_CURRENT_BAGGAGE();
}

void SocialGraphHandler::GetFollowers(
    UidListRpcResponse &response, const int64_t req_id, const int64_t user_id,
    const std::map<std::string, std::string> &carrier) {

  std::vector<int64_t> _return;
  auto baggage_it = carrier.find("baggage");
  if (baggage_it != carrier.end()) {
    SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
  }

  if (!XTrace::IsTracing()) {
    XTrace::StartTrace("SocialGraphHandler");
  }

  // XTRACE("SocialGraphHandler::GetFollowers", {{"RequestID", std::to_string(req_id)}});
  // Initialize a span
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map;
  TextMapWriter writer(writer_text_map);
  auto parent_span = opentracing::Tracer::Global()->Extract(reader);
  auto span = opentracing::Tracer::Global()->StartSpan(
      "GetFollowers",
      {opentracing::ChildOf(parent_span->get())});
  opentracing::Tracer::Global()->Inject(span->context(), writer);

  auto redis_client_wrapper = _redis_client_pool->Pop();
  if (!redis_client_wrapper) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_REDIS_ERROR;
    se.message = "Cannot connect to Redis server";
    // XTRACE("Cannot connect to Redis server");
    throw se;
  }
  auto redis_client = redis_client_wrapper->GetClient();

  // XTRACE("RedisGet start");
  auto redis_span = opentracing::Tracer::Global()->StartSpan("RedisGet", {opentracing::ChildOf(&span->context())});
  auto num_follower = redis_client->zcard(std::to_string(user_id) + ":followers");
  redis_client->sync_commit();
  auto num_follower_reply = num_follower.get();

  if (num_follower_reply.ok() && num_follower_reply.as_integer()) {
    std::string key = std::to_string(user_id) + ":followers";
    auto redis_followers = redis_client->zrange(key, 0, -1, false);
    redis_client->sync_commit();
    redis_span->Finish();
    // XTRACE("RedisGet complete");
    auto redis_followers_reply = redis_followers.get();
    if (redis_followers_reply.ok()) {
      auto followers_str = redis_followers_reply.as_array();
      for (auto const &item : followers_str) {
        _return.emplace_back(std::stoul(item.as_string()));
      }
      _redis_client_pool->Push(redis_client_wrapper);
    } else {
      ServiceException se;
      se.message = "Failed to get followers from Redis";
      se.errorCode = ErrorCode::SE_REDIS_ERROR;
      _redis_client_pool->Push(redis_client_wrapper);
      // XTRACE("Failed to get followers from Redis");
      LOG(error) << "Failed to get followers from Redis";
      throw se;
    }
  } else {
    redis_span->Finish();
    // XTRACE("RedisGet complete");
    _redis_client_pool->Push(redis_client_wrapper);
    mongoc_client_t *mongodb_client = mongoc_client_pool_pop(
        _mongodb_client_pool);
    if (!mongodb_client) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_MONGODB_ERROR;
      se.message = "Failed to pop a client from MongoDB pool";
      // XTRACE("Failed to pop a client from MongoDB pool");
      throw se;
    }
    auto collection = mongoc_client_get_collection(
        mongodb_client, "social-graph", "social-graph");
    if (!collection) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_MONGODB_ERROR;
      se.message = "Failed to create collection social_graph from MongoDB";
      // XTRACE("Failed to create collection social_graph from MongoDB");
      mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
      throw se;
    }
    bson_t *query = bson_new();
    BSON_APPEND_INT64(query, "user_id", user_id);
    // XTRACE("MongoFindUser start");
    auto find_span = opentracing::Tracer::Global()->StartSpan(
        "MongoFindUser", {opentracing::ChildOf(&span->context())});
    mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
        collection, query, nullptr, nullptr);
    const bson_t *doc;
    bool found = mongoc_cursor_next(cursor, &doc);
    if (found) {
      bson_iter_t iter_0;
      bson_iter_t iter_1;
      bson_iter_t user_id_child;
      bson_iter_t timestamp_child;
      int index = 0;
      std::multimap<std::string, std::string> redis_zset;
      bson_iter_init(&iter_0, doc);
      bson_iter_init(&iter_1, doc);

      while (
          bson_iter_find_descendant(
              &iter_0,
              ("followers." + std::to_string(index) + ".user_id").c_str(),
              &user_id_child) &&
              BSON_ITER_HOLDS_INT64 (&user_id_child) &&
              bson_iter_find_descendant(
                  &iter_1,
                  ("followers." + std::to_string(index) + ".timestamp").c_str(),
                  &timestamp_child)
              && BSON_ITER_HOLDS_INT64 (&timestamp_child)) {

        auto iter_user_id = bson_iter_int64(&user_id_child);
        auto iter_timestamp = bson_iter_int64(&timestamp_child);
        _return.emplace_back(iter_user_id);
        redis_zset.emplace(std::pair<std::string, std::string>(
            std::to_string(iter_timestamp), std::to_string(iter_user_id)));
        bson_iter_init(&iter_0, doc);
        bson_iter_init(&iter_1, doc);
        index++;
      }
      find_span->Finish();
      // XTRACE("MongoFindUser complete");
      bson_destroy(query);
      mongoc_cursor_destroy(cursor);
      mongoc_collection_destroy(collection);
      mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);

      redis_client_wrapper = _redis_client_pool->Pop();
      redis_client = redis_client_wrapper->GetClient();
      // XTRACE("RedistInsert start");
      auto redis_insert_span = opentracing::Tracer::Global()->StartSpan(
          "RedisInsert", {opentracing::ChildOf(&span->context())});
      std::string key = std::to_string(user_id) + ":followers";
      std::vector<std::string> options{"NX"};
      redis_client->zadd(key, options, redis_zset);
      redis_client->sync_commit();
      redis_insert_span->Finish();
      // XTRACE("RedisInsert complete");
      _redis_client_pool->Push(redis_client_wrapper);
    } else {
      find_span->Finish();
      // XTRACE("MongoFindUser complete");
      bson_destroy(query);
      mongoc_cursor_destroy(cursor);
      mongoc_collection_destroy(collection);
      mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
    }
  }
  span->Finish();
  // XTRACE("SocialGraphHandler::GetFollowers complete");
  response.baggage = GET_CURRENT_BAGGAGE().str();
  response.result = _return;
  DELETE_CURRENT_BAGGAGE();
}

void SocialGraphHandler::GetFollowees(
    UidListRpcResponse& response, const int64_t req_id, const int64_t user_id,
    const std::map<std::string, std::string> &carrier) {

  std::vector<int64_t> _return;
  auto baggage_it = carrier.find("baggage");
  if (baggage_it != carrier.end()) {
    SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
  }

  if (!XTrace::IsTracing()) {
    XTrace::StartTrace("SocialGraphHandler");
  }

  // XTRACE("SocialGraphHandler::GetFollowees", {{"RequestID", std::to_string(req_id)}});
  // Initialize a span
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map;
  TextMapWriter writer(writer_text_map);
  auto parent_span = opentracing::Tracer::Global()->Extract(reader);
  auto span = opentracing::Tracer::Global()->StartSpan(
      "GetFollowees",
      {opentracing::ChildOf(parent_span->get())});
  opentracing::Tracer::Global()->Inject(span->context(), writer);

  auto redis_client_wrapper = _redis_client_pool->Pop();
  if (!redis_client_wrapper) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_REDIS_ERROR;
    se.message = "Cannot connect to Redis server";
    // XTRACE("Cannot connect to Redis server");
    throw se;
  }
  auto redis_client = redis_client_wrapper->GetClient();

  // XTRACE("RedisGet start");
  auto redis_span = opentracing::Tracer::Global()->StartSpan(
      "RedisGet", {opentracing::ChildOf(&span->context())});
  auto num_followees = redis_client->zcard(
      std::to_string(user_id) + ":followees");
  redis_client->sync_commit();
  auto num_followees_reply = num_followees.get();

  if (num_followees_reply.ok() && num_followees_reply.as_integer()) {
    std::string key = std::to_string(user_id) + ":followees";
    auto redis_followees = redis_client->zrange(key, 0, -1, false);
    redis_client->sync_commit();
    redis_span->Finish();
    // XTRACE("RedisGet complete");
    auto redis_followees_reply = redis_followees.get();
    if (redis_followees_reply.ok()) {
      auto followees_str = redis_followees_reply.as_array();
      for (auto const &item : followees_str) {
        _return.emplace_back(std::stoul(item.as_string()));
      }
      _redis_client_pool->Push(redis_client_wrapper);
      return;
    } else {
      ServiceException se;
      se.message = "Failed to get followees from Redis";
      se.errorCode = ErrorCode::SE_REDIS_ERROR;
      // XTRACE("Failed to get followees from Redis");
      _redis_client_pool->Push(redis_client_wrapper);
      throw se;
    }
  } else {
    redis_span->Finish();
    // XTRACE("RedisGet complete");
    mongoc_client_t *mongodb_client = mongoc_client_pool_pop(
        _mongodb_client_pool);
    if (!mongodb_client) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_MONGODB_ERROR;
      se.message = "Failed to pop a client from MongoDB pool";
      // XTRACE("Failed to pop a client from MongoDB pool");
      throw se;
    }
    auto collection = mongoc_client_get_collection(
        mongodb_client, "social-graph", "social-graph");
    if (!collection) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_MONGODB_ERROR;
      se.message = "Failed to create collection social_graph from MongoDB";
      // XTRACE("Failed to create collection social_graph from MongoDB");
      mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
      throw se;
    }
    bson_t *query = bson_new();
    BSON_APPEND_INT64(query, "user_id", user_id);
    // XTRACE("MongoFindUser start");
    auto find_span = opentracing::Tracer::Global()->StartSpan(
        "MongoFindUser", {opentracing::ChildOf(&span->context())});
    mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
        collection, query, nullptr, nullptr);
    const bson_t *doc;
    bool found = mongoc_cursor_next(cursor, &doc);
    if (!found) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_THRIFT_HANDLER_ERROR;
      se.message = "Cannot find user_id in MongoDB.";
      bson_destroy(query);
      mongoc_cursor_destroy(cursor);
      mongoc_collection_destroy(collection);
      mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
      // XTRACE("Cannot find user_id in MongoDB");
      throw se;
    } else {
      bson_iter_t iter_0;
      bson_iter_t iter_1;
      bson_iter_t user_id_child;
      bson_iter_t timestamp_child;
      int index = 0;
      std::multimap<std::string, std::string> redis_zset;
      bson_iter_init(&iter_0, doc);
      bson_iter_init(&iter_1, doc);

      while (
          bson_iter_find_descendant(
              &iter_0,
              ("followees." + std::to_string(index) + ".user_id").c_str(),
              &user_id_child) &&
              BSON_ITER_HOLDS_INT64 (&user_id_child) &&
              bson_iter_find_descendant(
                  &iter_1,
                  ("followees." + std::to_string(index) + ".timestamp").c_str(),
                  &timestamp_child)
              && BSON_ITER_HOLDS_INT64 (&timestamp_child)) {

        auto iter_user_id = bson_iter_int64(&user_id_child);
        auto iter_timestamp = bson_iter_int64(&timestamp_child);
        _return.emplace_back(iter_user_id);
        redis_zset.emplace(std::pair<std::string, std::string>(
            std::to_string(iter_timestamp), std::to_string(iter_user_id)));
        bson_iter_init(&iter_0, doc);
        bson_iter_init(&iter_1, doc);
        index++;
      }
      find_span->Finish();
      // XTRACE("MongoFindUser complete");
      bson_destroy(query);
      mongoc_cursor_destroy(cursor);
      mongoc_collection_destroy(collection);
      mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
      redis_client_wrapper = _redis_client_pool->Pop();
      redis_client = redis_client_wrapper->GetClient();
      // XTRACE("RedisInsert start");
      auto redis_insert_span = opentracing::Tracer::Global()->StartSpan(
          "RedisInsert", {opentracing::ChildOf(&span->context())});
      std::string key = std::to_string(user_id) + ":followees";
      std::vector<std::string> options{"NX"};
      redis_client->zadd(key, options, redis_zset);
      redis_client->sync_commit();
      redis_insert_span->Finish();
      // XTRACE("RedisInsert complete");
      _redis_client_pool->Push(redis_client_wrapper);
    }
  }
  span->Finish();
  // XTRACE("SocialGraphHandler::GetFollowees complete");
  response.baggage = GET_CURRENT_BAGGAGE().str();
  response.result = _return;
  DELETE_CURRENT_BAGGAGE();
}

void SocialGraphHandler::InsertUser(
    BaseRpcResponse& response,
    int64_t req_id, int64_t user_id,
    const std::map<std::string, std::string> &carrier) {

  auto baggage_it = carrier.find("baggage");
  if (baggage_it != carrier.end()) {
    SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
  }

  if (!XTrace::IsTracing()) {
    XTrace::StartTrace("SocialGraphHandler");
  }

  // XTRACE("SocialGraphHandler::InsertUser", {{"RequestID", std::to_string(req_id)}});
  // Initialize a span
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map;
  TextMapWriter writer(writer_text_map);
  auto parent_span = opentracing::Tracer::Global()->Extract(reader);
  auto span = opentracing::Tracer::Global()->StartSpan(
      "InsertUser",
      {opentracing::ChildOf(parent_span->get())});
  opentracing::Tracer::Global()->Inject(span->context(), writer);

  mongoc_client_t *mongodb_client = mongoc_client_pool_pop(
      _mongodb_client_pool);
  if (!mongodb_client) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_MONGODB_ERROR;
    se.message = "Failed to pop a client from MongoDB pool";
    // XTRACE("Failed to pop a client from MongoDB pool");
    throw se;
  }
  auto collection = mongoc_client_get_collection(
      mongodb_client, "social-graph", "social-graph");
  if (!collection) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_MONGODB_ERROR;
    se.message = "Failed to create collection social_graph from MongoDB";
    // XTRACE("Failed to create collection social_graph from MongoDB");
    mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
    throw se;
  }

  bson_t *new_doc = BCON_NEW(
      "user_id", BCON_INT64(user_id),
      "followers", "[", "]",
      "followees", "[", "]"
  );
  bson_error_t error;
  // XTRACE("MongoInsertUser start");
  auto insert_span = opentracing::Tracer::Global()->StartSpan(
      "MongoInsertUser", {opentracing::ChildOf(&span->context())});
  bool inserted = mongoc_collection_insert_one(
      collection, new_doc, nullptr, nullptr, &error);
  insert_span->Finish();
  // XTRACE("MongoInsertUser complete");
  if (!inserted) {
    LOG(error) << "Failed to insert social graph for user "
               << user_id << " to MongoDB: " << error.message;
    ServiceException se;
    se.errorCode = ErrorCode::SE_MONGODB_ERROR;
    se.message = error.message;
    bson_destroy(new_doc);
    mongoc_collection_destroy(collection);
    // XTRACE("Failed to insert social graph for user " + std::to_string(user_id));
    mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
    throw se;
  }
  bson_destroy(new_doc);
  mongoc_collection_destroy(collection);
  mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);

  span->Finish();
  // XTRACE("SocialGraphHandler::InsertUser complete");
  response.baggage = GET_CURRENT_BAGGAGE().str();
  DELETE_CURRENT_BAGGAGE();
}

void SocialGraphHandler::FollowWithUsername(
    BaseRpcResponse &response,
    int64_t req_id,
    const std::string &user_name,
    const std::string &followee_name,
    const std::map<std::string, std::string> &carrier) {

  auto baggage_it = carrier.find("baggage");
  if (baggage_it != carrier.end()) {
    SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
  }

  if (!XTrace::IsTracing()) {
    XTrace::StartTrace("SocialGraphHandler");
  }

  // XTRACE("SocialGraphHandler::FollowWithUsername", {{"RequestID", std::to_string(req_id)}});
  // Initialize a span
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map;
  TextMapWriter writer(writer_text_map);
  auto parent_span = opentracing::Tracer::Global()->Extract(reader);
  auto span = opentracing::Tracer::Global()->StartSpan(
      "FollowWithUsername",
      {opentracing::ChildOf(parent_span->get())});
  opentracing::Tracer::Global()->Inject(span->context(), writer);

  Baggage user_id_baggage = BRANCH_CURRENT_BAGGAGE();
  std::future<int64_t> user_id_future = std::async(
      std::launch::async,[&]() {
        BAGGAGE(user_id_baggage);
        auto user_client_wrapper = _user_service_client_pool->Pop();
        if (!user_client_wrapper) {
          ServiceException se;
          se.errorCode = ErrorCode::SE_THRIFT_CONN_ERROR;
          se.message = "Failed to connect to social-graph-service";
          // XTRACE("Failed to connect to social-graph-service");
          throw se;
        }
        auto user_client = user_client_wrapper->GetClient();
        int64_t _return;
        try {
          writer_text_map["baggage"] = BRANCH_CURRENT_BAGGAGE().str();
          UserIdRpcResponse uid_response;
          user_client->GetUserId(uid_response, req_id, user_name, writer_text_map);
          Baggage b = Baggage::deserialize(uid_response.baggage);
          JOIN_CURRENT_BAGGAGE(b);
          _return = uid_response.result;
        } catch (...) {
          _user_service_client_pool->Push(user_client_wrapper);
          LOG(error) << "Failed to get user_id from user-service";
          // XTRACE("Failed to get user_id from user-service");
          throw;
        }
        _user_service_client_pool->Push(user_client_wrapper);
        return _return;
      });

  Baggage followee_id_baggage = BRANCH_CURRENT_BAGGAGE();
  std::future<int64_t> followee_id_future = std::async(
      std::launch::async,[&]() {
        BAGGAGE(followee_id_baggage);
        auto user_client_wrapper = _user_service_client_pool->Pop();
        if (!user_client_wrapper) {
          ServiceException se;
          se.errorCode = ErrorCode::SE_THRIFT_CONN_ERROR;
          se.message = "Failed to connected to social-graph-service";
          // XTRACE("Failed to connect to social-graph-service");
          throw se;
        }
        auto user_client = user_client_wrapper->GetClient();
        int64_t _return;
        try {
          writer_text_map["baggage"] = BRANCH_CURRENT_BAGGAGE().str();
          UserIdRpcResponse uid_response;
          user_client->GetUserId(uid_response,req_id, followee_name, writer_text_map);
          Baggage b = Baggage::deserialize(uid_response.baggage);
          JOIN_CURRENT_BAGGAGE(b);
          _return = uid_response.result;
        } catch (...) {
          _user_service_client_pool->Push(user_client_wrapper);
          LOG(error) << "Failed to get user_id from user-service";
          // XTRACE("Failed to get user_id from user-service");
          throw;
        }
        _user_service_client_pool->Push(user_client_wrapper);
        return _return;
      });

  int64_t user_id;
  int64_t followee_id;
  try {
    user_id = user_id_future.get();
    JOIN_CURRENT_BAGGAGE(user_id_baggage);
    followee_id = followee_id_future.get();
    JOIN_CURRENT_BAGGAGE(followee_id_baggage);
  } catch (...) {
    throw;
  }

  if (user_id >= 0 && followee_id >= 0) {
    try {
      writer_text_map["baggage"] = BRANCH_CURRENT_BAGGAGE().str();
      Follow(response, req_id, user_id, followee_id, writer_text_map);
      Baggage b = Baggage::deserialize(response.baggage);
      JOIN_CURRENT_BAGGAGE(b);
    } catch (...) {
      throw;
    }
  }
  span->Finish();
  // XTRACE("SocialGraphHandler::FollowWithUsername complete");
  response.baggage = GET_CURRENT_BAGGAGE().str();
  DELETE_CURRENT_BAGGAGE();
}

void SocialGraphHandler::UnfollowWithUsername(
    BaseRpcResponse& response,
    int64_t req_id,
    const std::string &user_name,
    const std::string &followee_name,
    const std::map<std::string, std::string> &carrier) {

  auto baggage_it = carrier.find("baggage");
  if (baggage_it != carrier.end()) {
    SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
  }

  if (!XTrace::IsTracing()) {
    XTrace::StartTrace("SocialGraphHandler");
  }

  // XTRACE("SocialGraphHandler::UnfollowWithUsername", {{"RequestID", std::to_string(req_id)}});
  // Initialize a span
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map;
  TextMapWriter writer(writer_text_map);
  auto parent_span = opentracing::Tracer::Global()->Extract(reader);
  auto span = opentracing::Tracer::Global()->StartSpan(
      "UnfollowWithUsername",
      {opentracing::ChildOf(parent_span->get())});
  opentracing::Tracer::Global()->Inject(span->context(), writer);

  Baggage user_id_baggage = BRANCH_CURRENT_BAGGAGE();
  std::future<int64_t> user_id_future = std::async(
      std::launch::async,[&]() {
        BAGGAGE(user_id_baggage);
        auto user_client_wrapper = _user_service_client_pool->Pop();
        if (!user_client_wrapper) {
          ServiceException se;
          se.errorCode = ErrorCode::SE_THRIFT_CONN_ERROR;
          se.message = "Failed to connect to social-graph-service";
          // XTRACE("Failed to connect to social-graph-service");
          throw se;
        }
        auto user_client = user_client_wrapper->GetClient();
        int64_t _return;
        try {
          writer_text_map["baggage"] = BRANCH_CURRENT_BAGGAGE().str();
          UserIdRpcResponse uid_response;
          user_client->GetUserId(uid_response, req_id, user_name, writer_text_map);
          Baggage b = Baggage::deserialize(uid_response.baggage);
          JOIN_CURRENT_BAGGAGE(b);
          _return = uid_response.result;
        } catch (...) {
          _user_service_client_pool->Push(user_client_wrapper);
          LOG(error) << "Failed to get user_id from user-service";
          // XTRACE("Failed to get user_id from user-service");
          throw;
        }
        _user_service_client_pool->Push(user_client_wrapper);
        return _return;
      });

  Baggage followee_id_baggage = BRANCH_CURRENT_BAGGAGE();
  std::future<int64_t> followee_id_future = std::async(
      std::launch::async,[&]() {
        BAGGAGE(followee_id_baggage);
        auto user_client_wrapper = _user_service_client_pool->Pop();
        if (!user_client_wrapper) {
          ServiceException se;
          se.errorCode = ErrorCode::SE_THRIFT_CONN_ERROR;
          se.message = "Failed to connect to social-graph-service";
          // XTRACE("Failed to connect to social-graph-service");
          throw se;
        }
        auto user_client = user_client_wrapper->GetClient();
        int64_t _return;
        try {
          writer_text_map["baggage"] = BRANCH_CURRENT_BAGGAGE().str();
          UserIdRpcResponse uid_response;
          user_client->GetUserId(uid_response, req_id, followee_name, writer_text_map);
          Baggage b = Baggage::deserialize(uid_response.baggage);
          JOIN_CURRENT_BAGGAGE(b);
          _return = uid_response.result;
        } catch (...) {
          _user_service_client_pool->Push(user_client_wrapper);
          LOG(error) << "Failed to get user_id from user-service";
          // XTRACE("Failed to get user_id from user-service");
          throw;
        }
        _user_service_client_pool->Push(user_client_wrapper);
        return _return;
      });

  int64_t user_id;
  int64_t followee_id;
  try {
    user_id = user_id_future.get();
    JOIN_CURRENT_BAGGAGE(user_id_baggage);
    followee_id = followee_id_future.get();
    JOIN_CURRENT_BAGGAGE(followee_id_baggage);
  } catch (...) {
    throw;
  }

  if (user_id >= 0 && followee_id >= 0) {
    try {
      writer_text_map["baggage"] = BRANCH_CURRENT_BAGGAGE().str();
      Unfollow(response, req_id, user_id, followee_id, writer_text_map);
      Baggage b = Baggage::deserialize(response.baggage);
      JOIN_CURRENT_BAGGAGE(b);
    } catch (...) {
      throw;
    }
  }
  span->Finish();
  // XTRACE("SocialGraphService::UnfollowWithUsername complete");
  response.baggage = GET_CURRENT_BAGGAGE().str();
  DELETE_CURRENT_BAGGAGE();
}

} // namespace social_network

#endif //SOCIAL_NETWORK_MICROSERVICES_SOCIALGRAPHHANDLER_H
