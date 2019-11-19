#ifndef MEDIA_MICROSERVICES_USERREVIEWHANDLER_H
#define MEDIA_MICROSERVICES_USERREVIEWHANDLER_H

#include <iostream>
#include <string>

#include <mongoc.h>
#include <bson/bson.h>

#include "../../gen-cpp/UserReviewService.h"
#include "../../gen-cpp/ReviewStorageService.h"
#include "../logger.h"
#include "../tracing.h"
#include "../ClientPool.h"
#include "../RedisClient.h"
#include "../ThriftClient.h"
#include <xtrace/xtrace.h>
#include <xtrace/baggage.h>

namespace media_service {
class UserReviewHandler : public UserReviewServiceIf {
 public:
  UserReviewHandler(
      ClientPool<RedisClient> *,
      mongoc_client_pool_t *,
      ClientPool<ThriftClient<ReviewStorageServiceClient>> *);
  ~UserReviewHandler() override = default;
  void UploadUserReview(BaseRpcResponse &, int64_t, int64_t, int64_t, int64_t,
                         const std::map<std::string, std::string> &) override;
  void ReadUserReviews(ReviewListRpcResponse &response, int64_t req_id,
                       int64_t user_id, int32_t start, int32_t stop,
                        const std::map<std::string, std::string> & carrier) override;

 private:
  ClientPool<RedisClient> *_redis_client_pool;
  mongoc_client_pool_t *_mongodb_client_pool;
  ClientPool<ThriftClient<ReviewStorageServiceClient>> *_review_client_pool;
};

UserReviewHandler::UserReviewHandler(
    ClientPool<RedisClient> *redis_client_pool,
    mongoc_client_pool_t *mongodb_pool,
    ClientPool<ThriftClient<ReviewStorageServiceClient>> *review_storage_client_pool) {
  _redis_client_pool = redis_client_pool;
  _mongodb_client_pool = mongodb_pool;
  _review_client_pool = review_storage_client_pool;
}

void UserReviewHandler::UploadUserReview(
    BaseRpcResponse &response,
    int64_t req_id,
    int64_t user_id,
    int64_t review_id,
    int64_t timestamp,
    const std::map<std::string, std::string> &carrier) {

  std::map<std::string, std::string>::const_iterator baggage_it = carrier.find("baggage");
  if (baggage_it != carrier.end()) {
    SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
  }

  if (!XTrace::IsTracing()) {
    XTrace::StartTrace("UserReviewHandler");
  }
  XTRACE("UserReviewHandler::UploadUserReview", {{"RequestID", std::to_string(req_id)}});
  // Initialize a span
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map;
  TextMapWriter writer(writer_text_map);
  auto parent_span = opentracing::Tracer::Global()->Extract(reader);
  auto span = opentracing::Tracer::Global()->StartSpan(
      "UploadUserReview",
      { opentracing::ChildOf(parent_span->get()) });
  opentracing::Tracer::Global()->Inject(span->context(), writer);

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
      mongodb_client, "user-review", "user-review");
  if (!collection) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_MONGODB_ERROR;
    se.message = "Failed to create collection user-review from DB user-review";
    XTRACE("Failed to create collection user-review from DB user-review");
    mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
    throw se;
  }

  bson_t *query = bson_new();
  BSON_APPEND_INT64(query, "user_id", user_id);
  XTRACE("MongoFindUser start");
  auto find_span = opentracing::Tracer::Global()->StartSpan(
      "MongoFindUser", {opentracing::ChildOf(&span->context())});
  mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
      collection, query, nullptr, nullptr);
  XTRACE("MongoFindUser complete");
  const bson_t *doc;
  bool found = mongoc_cursor_next(cursor, &doc);
  if (!found) {
    bson_t *new_doc = BCON_NEW(
        "user_id", BCON_INT64(user_id),
        "reviews",
        "[", "{", "review_id", BCON_INT64(review_id),
        "timestamp", BCON_INT64(timestamp), "}", "]"
    );
    bson_error_t error;
    XTRACE("MongoInsert start");
    auto insert_span = opentracing::Tracer::Global()->StartSpan(
        "MongoInsert", {opentracing::ChildOf(&span->context())});
    bool plotinsert = mongoc_collection_insert_one(
        collection, new_doc, nullptr, nullptr, &error);
    insert_span->Finish();
    XTRACE("MongoInsert finish");
    if (!plotinsert) {
      LOG(error) << "Failed to insert user review of user " << user_id
                 << " to MongoDB: " << error.message;
      XTRACE("Failed to insert user review of user " + std::to_string(user_id) + "to MongoDB");
      ServiceException se;
      se.errorCode = ErrorCode::SE_MONGODB_ERROR;
      se.message = error.message;
      bson_destroy(new_doc);
      bson_destroy(query);
      mongoc_cursor_destroy(cursor);
      mongoc_collection_destroy(collection);
      mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
      throw se;
    }
    bson_destroy(new_doc);
  } else {
    bson_t *update = BCON_NEW(
        "$push", "{",
        "reviews", "{",
        "$each", "[", "{",
        "review_id", BCON_INT64(review_id),
        "timestamp", BCON_INT64(timestamp),
        "}", "]",
        "$position", BCON_INT32(0),
        "}",
        "}"
    );
    bson_error_t error;
    bson_t reply;
    XTRACE("MongoUpdate start");
    auto update_span = opentracing::Tracer::Global()->StartSpan(
        "MongoUpdate", {opentracing::ChildOf(&span->context())});
    bool plotupdate = mongoc_collection_find_and_modify(
        collection, query, nullptr, update, nullptr, false, false,
        true, &reply, &error);
    update_span->Finish();
    XTRACE("MongoUpdate finish");
    if (!plotupdate) {
      LOG(error) << "Failed to update user-review for user " << user_id
                 << " to MongoDB: " << error.message;
      XTRACE("Failed to update user-review for user " + std::to_string(user_id) + " to MongoDB");
      ServiceException se;
      se.errorCode = ErrorCode::SE_MONGODB_ERROR;
      se.message = error.message;
      bson_destroy(update);
      bson_destroy(query);
      bson_destroy(&reply);
      mongoc_cursor_destroy(cursor);
      mongoc_collection_destroy(collection);
      mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
      throw se;
    }
    bson_destroy(update);
    bson_destroy(&reply);
  }
  bson_destroy(query);
  mongoc_cursor_destroy(cursor);
  mongoc_collection_destroy(collection);
  mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);

  auto redis_client_wrapper = _redis_client_pool->Pop();
  if (!redis_client_wrapper) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_REDIS_ERROR;
    se.message = "Cannot connected to Redis server";
    XTRACE("Cannot connect to Redis server");
    throw se;
  }
  auto redis_client = redis_client_wrapper->GetClient();
  XTRACE("RedisUpdate start");
  auto redis_span = opentracing::Tracer::Global()->StartSpan(
      "RedisUpdate", {opentracing::ChildOf(&span->context())});
  auto num_reviews = redis_client->zcard(std::to_string(user_id));
  redis_client->sync_commit();
  auto num_reviews_reply = num_reviews.get();
  std::vector<std::string> options{"NX"};
  if (num_reviews_reply.ok() && num_reviews_reply.as_integer()) {
    std::multimap<std::string, std::string> value = {{
      std::to_string(timestamp), std::to_string(review_id)}};
    redis_client->zadd(std::to_string(user_id), options, value);
    redis_client->sync_commit();
  }
  _redis_client_pool->Push(redis_client_wrapper);
  redis_span->Finish();
  XTRACE("RedisUpdate complete");
  span->Finish();
  response.baggage = GET_CURRENT_BAGGAGE().str();
  XTRACE("UserReviewHandler::WriteReview complete");
}

void UserReviewHandler::ReadUserReviews(
    ReviewListRpcResponse &response, int64_t req_id,
    int64_t user_id, int32_t start, int32_t stop,
    const std::map<std::string, std::string> & carrier) {

  std::vector<Review> _return;
  std::map<std::string, std::string>::const_iterator baggage_it = carrier.find("baggage");
  if (baggage_it != carrier.end()) {
    SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
  }

  if (!XTrace::IsTracing()) {
    XTrace::StartTrace("UserReviewHandler");
  }
  XTRACE("UserReviewHandler::ReadUserReviews", {{"RequestID", std::to_string(req_id)}});
  // Initialize a span
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map;
  TextMapWriter writer(writer_text_map);
  auto parent_span = opentracing::Tracer::Global()->Extract(reader);
  auto span = opentracing::Tracer::Global()->StartSpan(
      "ReadUserReviews",
      { opentracing::ChildOf(parent_span->get()) });
  opentracing::Tracer::Global()->Inject(span->context(), writer);

  if (stop <= start || start < 0) {
    return;
  }

  auto redis_client_wrapper = _redis_client_pool->Pop();
  if (!redis_client_wrapper) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_REDIS_ERROR;
    se.message = "Cannot connected to Redis server";
    XTRACE("Cannot connect to Redis server");
    throw se;
  }
  auto redis_client = redis_client_wrapper->GetClient();
  XTRACE("RedisFind start");
  auto redis_span = opentracing::Tracer::Global()->StartSpan(
      "RedisFind", {opentracing::ChildOf(&span->context())});
  auto review_ids_future = redis_client->zrevrange(
      std::to_string(user_id), start, stop - 1);
  redis_client->commit();
  redis_span->Finish();

  cpp_redis::reply review_ids_reply;
  try {
    review_ids_reply = review_ids_future.get();
  } catch (...) {
    LOG(error) << "Failed to read review_ids from user-review-redis";
    XTRACE("Failed to read review_ids from user-review-redis");
    _redis_client_pool->Push(redis_client_wrapper);
    throw;
  }
  _redis_client_pool->Push(redis_client_wrapper);
  std::vector<int64_t> review_ids;
  auto review_ids_reply_array = review_ids_reply.as_array();
  for (auto &review_id_reply : review_ids_reply_array) {
    review_ids.emplace_back(std::stoul(review_id_reply.as_string()));
  }

  int mongo_start = start + review_ids.size();
  std::multimap<std::string, std::string> redis_update_map;
  if (mongo_start < stop) {
    // Instead find review_ids from mongodb
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
        mongodb_client, "user-review", "user-review");
    if (!collection) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_MONGODB_ERROR;
      se.message = "Failed to create collection user-review from MongoDB";
      XTRACE("Failed to create collection user-review from MongoDB");
      throw se;
    }

    bson_t *query = BCON_NEW("user_id", BCON_INT64(user_id));
    bson_t *opts = BCON_NEW(
        "projection", "{",
        "reviews", "{",
        "$slice", "[",
        BCON_INT32(0), BCON_INT32(stop),
        "]", "}", "}");
    XTRACE("MongoFindUserReviews start");
    auto find_span = opentracing::Tracer::Global()->StartSpan(
        "MongoFindUserReviews", {opentracing::ChildOf(&span->context())});
    mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
        collection, query, opts, nullptr);
    find_span->Finish();
    XTRACE("MongoFindUserReviews finish");
    const bson_t *doc;
    bool found = mongoc_cursor_next(cursor, &doc);
    if (found) {
      bson_iter_t iter_0;
      bson_iter_t iter_1;
      bson_iter_t review_id_child;
      bson_iter_t timestamp_child;
      int idx = 0;
      bson_iter_init(&iter_0, doc);
      bson_iter_init(&iter_1, doc);
      while (bson_iter_find_descendant(&iter_0,
          ("reviews." + std::to_string(idx) + ".review_id").c_str(),
          &review_id_child)
          && BSON_ITER_HOLDS_INT64(&review_id_child)
          && bson_iter_find_descendant(&iter_1,
              ("reviews." + std::to_string(idx) + ".timestamp").c_str(),
              &timestamp_child)
          && BSON_ITER_HOLDS_INT64(&timestamp_child)) {
        auto curr_review_id = bson_iter_int64(&review_id_child);
        auto curr_timestamp = bson_iter_int64(&timestamp_child);
        if (idx >= mongo_start) {
          review_ids.emplace_back(curr_review_id);
        }
        redis_update_map.insert(
            {std::to_string(curr_timestamp), std::to_string(curr_review_id)});
        bson_iter_init(&iter_0, doc);
        bson_iter_init(&iter_1, doc);
        idx++;
      }
    }
    find_span->Finish();
    bson_destroy(opts);
    bson_destroy(query);
    mongoc_cursor_destroy(cursor);
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
  }

  Baggage review_baggage = BRANCH_CURRENT_BAGGAGE();
  std::future<std::vector<Review>> review_future = std::async(
      std::launch::async, [&]() {
        BAGGAGE(review_baggage);
        auto review_client_wrapper = _review_client_pool->Pop();
        if (!review_client_wrapper) {
          ServiceException se;
          se.errorCode = ErrorCode::SE_THRIFT_CONN_ERROR;
          se.message = "Failed to connected to review-storage-service";
          XTRACE("Failed to connect to review-storage-service");
          throw se;
        }
        std::vector<Review> _return_reviews;
        auto review_client = review_client_wrapper->GetClient();
        try {
          writer_text_map["baggage"] = GET_CURRENT_BAGGAGE().str();
          ReviewListRpcResponse response;
          review_client->ReadReviews(
              response, req_id, review_ids, writer_text_map);
          Baggage b = Baggage::deserialize(response.baggage);
          JOIN_CURRENT_BAGGAGE(b);
          _return_reviews = response.result;
        } catch (...) {
          _review_client_pool->Push(review_client_wrapper);
          LOG(error) << "Failed to read review from review-storage-service";
          XTRACE("Failed to read review from review-storage-service");
          throw;
        }
        _review_client_pool->Push(review_client_wrapper);
        return _return_reviews;
      });

  std::future<cpp_redis::reply> zadd_reply_future;
  if (!redis_update_map.empty()) {
    // Update Redis
    redis_client_wrapper = _redis_client_pool->Pop();
    if (!redis_client_wrapper) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_REDIS_ERROR;
      se.message = "Cannot connected to Redis server";
      XTRACE("Cannot connect to Redis server");
      throw se;
    }
    redis_client = redis_client_wrapper->GetClient();
    XTRACE("RedisUpdate start");
    auto redis_update_span = opentracing::Tracer::Global()->StartSpan(
        "RedisUpdate", {opentracing::ChildOf(&span->context())});
    redis_client->del(std::vector<std::string>{std::to_string(user_id)});
    std::vector<std::string> options{"NX"};
    zadd_reply_future = redis_client->zadd(
        std::to_string(user_id), options, redis_update_map);
    redis_client->commit();
    redis_update_span->Finish();
    XTRACE("RedisUpdate finish");
  }

  try {
    _return = review_future.get();
    JOIN_CURRENT_BAGGAGE(review_baggage);
  } catch (...) {
    LOG(error) << "Failed to get review from review-storage-service";
    XTRACE("Failed to get review from review-storage-service");
    if (!redis_update_map.empty()) {
      try {
        zadd_reply_future.get();
      } catch (...) {
        LOG(error) << "Failed to Update Redis Server";
        XTRACE("Failed to Update Redis Server");
      }
      _redis_client_pool->Push(redis_client_wrapper);
    }
    throw;
  }

  if (!redis_update_map.empty()) {
    try {
      zadd_reply_future.get();
    } catch (...) {
      LOG(error) << "Failed to Update Redis Server";
      XTRACE("Failed to Update Redis Server");
      _redis_client_pool->Push(redis_client_wrapper);
      throw;
    }
    _redis_client_pool->Push(redis_client_wrapper);
  }

  span->Finish();
  XTRACE("UserReviewHandler::ReadUserReviews complete");
  response.baggage = GET_CURRENT_BAGGAGE().str();
  response.result = _return;
  DELETE_CURRENT_BAGGAGE();

}

}// namespace media_service

#endif //MEDIA_MICROSERVICES_USERREVIEWHANDLER_H
