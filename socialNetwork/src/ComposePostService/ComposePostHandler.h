#ifndef SOCIAL_NETWORK_MICROSERVICES_SRC_COMPOSEPOSTSERVICE_COMPOSEPOSTHANDLER_H_
#define SOCIAL_NETWORK_MICROSERVICES_SRC_COMPOSEPOSTSERVICE_COMPOSEPOSTHANDLER_H_

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <future>

#include <cpp_redis/cpp_redis>
#include <nlohmann/json.hpp>

#include "../../gen-cpp/ComposePostService.h"
#include "../../gen-cpp/PostStorageService.h"
#include "../../gen-cpp/UserTimelineService.h"
#include "../ClientPool.h"
#include "../logger.h"
#include "../tracing.h"
#include "../RedisClient.h"
#include "../ThriftClient.h"
#include "RabbitmqClient.h"
#include <xtrace/xtrace.h>
#include <xtrace/baggage.h>

#define NUM_COMPONENTS 6
#define REDIS_EXPIRE_TIME 10

namespace social_network {
using json = nlohmann::json;
using std::chrono::milliseconds;
using std::chrono::duration_cast;
using std::chrono::system_clock;

class ComposePostHandler : public ComposePostServiceIf {
 public:
  ComposePostHandler(
      ClientPool<RedisClient> *,
      ClientPool<ThriftClient<PostStorageServiceClient>> *,
      ClientPool<ThriftClient<UserTimelineServiceClient>> *,
      ClientPool<RabbitmqClient> *rabbitmq_client_pool);
  ~ComposePostHandler() override = default;

  void UploadText(BaseRpcResponse& response, int64_t req_id, const std::string& text,
      const std::map<std::string, std::string> & carrier) override;

  void UploadMedia(BaseRpcResponse& response, int64_t req_id, const std::vector<Media>& media,
      const std::map<std::string, std::string> & carrier) override;

  void UploadUniqueId(BaseRpcResponse& response, int64_t req_id, const int64_t post_id,
      const PostType::type post_type,
      const std::map<std::string, std::string> & carrier) override;

  void UploadCreator(BaseRpcResponse& response, int64_t req_id, const Creator& creator,
      const std::map<std::string, std::string> & carrier) override;

  void UploadUrls(BaseRpcResponse& response, int64_t req_id, const std::vector<Url> & urls,
      const std::map<std::string, std::string> & carrier) override;

  void UploadUserMentions(BaseRpcResponse& response, const int64_t req_id,
      const std::vector<UserMention> & user_mentions,
      const std::map<std::string, std::string> & carrier) override;

 private:
  ClientPool<RedisClient> *_redis_client_pool;
  ClientPool<ThriftClient<PostStorageServiceClient>>
      *_post_storage_client_pool;
  ClientPool<ThriftClient<UserTimelineServiceClient>>
      *_user_timeline_client_pool;
  ClientPool<RabbitmqClient> *_rabbitmq_client_pool;
  std::exception_ptr _rabbitmq_teptr;
  std::exception_ptr _post_storage_teptr;
  std::exception_ptr _user_timeline_teptr;

  void _ComposeAndUpload(int64_t req_id,
      const std::map<std::string, std::string> & carrier);

  void _UploadUserTimelineHelper(int64_t req_id, int64_t post_id,
      int64_t user_id, int64_t timestamp,
      const std::map<std::string, std::string> & carrier,
      Baggage& baggage, std::promise<Baggage> baggage_promise);

  void _UploadPostHelper(int64_t req_id, const Post &post,
      const std::map<std::string, std::string> &carrier,
      Baggage& baggage, std::promise<Baggage> baggage_promise);

  void _UploadHomeTimelineHelper(int64_t req_id, int64_t post_id,
      int64_t user_id, int64_t timestamp,
      const std::vector<int64_t> &user_mentions_id,
      const std::map<std::string, std::string> &carrier,
      Baggage& baggage, std::promise<Baggage> baggage_promise);

};

ComposePostHandler::ComposePostHandler(
    ClientPool<social_network::RedisClient> * redis_client_pool,
    ClientPool<social_network::ThriftClient<
        PostStorageServiceClient>> *post_storage_client_pool,
    ClientPool<social_network::ThriftClient<
        UserTimelineServiceClient>> *user_timeline_client_pool,
    ClientPool<RabbitmqClient> *rabbitmq_client_pool) {
  _redis_client_pool = redis_client_pool;
  _post_storage_client_pool = post_storage_client_pool;
  _user_timeline_client_pool = user_timeline_client_pool;
  _rabbitmq_client_pool = rabbitmq_client_pool;
  _rabbitmq_teptr = nullptr;
  _post_storage_teptr = nullptr;
  _user_timeline_teptr = nullptr;
}

void ComposePostHandler::UploadCreator(
    BaseRpcResponse& response,
    int64_t req_id,
    const Creator &creator,
    const std::map<std::string, std::string> &carrier) {

  auto baggage_it = carrier.find("baggage");
  if (baggage_it != carrier.end()) {
    SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
  }

  if (!XTrace::IsTracing()) {
    XTrace::StartTrace("ComposePostHandler");
  }

  XTRACE("ComposePostHandler::UploadCreator", {{"RequestID", std::to_string(req_id)}});

  // Initialize a span
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map;
  TextMapWriter writer(writer_text_map);
  auto parent_span = opentracing::Tracer::Global()->Extract(reader);
  auto span = opentracing::Tracer::Global()->StartSpan(
      "UploadCreator",
      { opentracing::ChildOf(parent_span->get()) });
  opentracing::Tracer::Global()->Inject(span->context(), writer);

  std::string creator_str = "{\"user_id\": " + std::to_string(creator.user_id)
      + ", \"username\": \"" + creator.username + "\"}";

  XTRACE("Connecting to redis server");
  auto redis_client_wrapper = _redis_client_pool->Pop();
  if (!redis_client_wrapper) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_REDIS_ERROR;
    se.message = "Cannot connect to Redis server";
    XTRACE("Cannot connect to Redis server");
    throw se;
  }
  auto redis_client = redis_client_wrapper->GetClient();
  XTRACE("RedisHashSet start");
  auto add_span = opentracing::Tracer::Global()->StartSpan(
      "RedisHashSet", {opentracing::ChildOf(&span->context())});
  auto hset_reply = redis_client->hset(std::to_string(req_id),
      "creator", creator_str);
  auto hlen_reply = redis_client->hincrby(std::to_string(req_id),
                                          "num_components", 1);
  redis_client->expire(std::to_string(req_id), REDIS_EXPIRE_TIME);
  redis_client->sync_commit();
  add_span->Finish();
  XTRACE("RedisHashSet complete");
  _redis_client_pool->Push(redis_client_wrapper);

  auto num_components_reply = hlen_reply.get();
  if (!num_components_reply.ok() || !hset_reply.get().ok()) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_REDIS_ERROR;
    se.message = "Failed to retrieve message from Redis";
    XTRACE("Failed to retrieve message from Redis");
    throw se;
  }

  if (num_components_reply.as_integer() == NUM_COMPONENTS) {
    writer_text_map["baggage"] = BRANCH_CURRENT_BAGGAGE().str();
    _ComposeAndUpload(req_id, writer_text_map);
  }

  span->Finish();
  XTRACE("ComposePostHandler::Upload Creator complete");

  response.baggage = GET_CURRENT_BAGGAGE().str();
  DELETE_CURRENT_BAGGAGE();
}

void ComposePostHandler::UploadText(
    BaseRpcResponse& response,
    int64_t req_id,
    const std::string &text,
    const std::map<std::string, std::string> &carrier) {

  auto baggage_it = carrier.find("baggage");
  if (baggage_it != carrier.end()) {
    SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
  }

  if (!XTrace::IsTracing()) {
    XTrace::StartTrace("ComposePostHandler");
  }

  XTRACE("ComposePostHandler::UploadText", {{"RequestID", std::to_string(req_id)}});

  // Initialize a span
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map;
  TextMapWriter writer(writer_text_map);
  auto parent_span = opentracing::Tracer::Global()->Extract(reader);
  auto span = opentracing::Tracer::Global()->StartSpan(
      "UploadText",
      { opentracing::ChildOf(parent_span->get()) });
  opentracing::Tracer::Global()->Inject(span->context(), writer);

  auto redis_client_wrapper = _redis_client_pool->Pop();
  if (!redis_client_wrapper) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_REDIS_ERROR;
    se.message = "Cannot connect to Redis server";
    XTRACE("Cannot conenct to Redis server");
    throw se;
  }
  auto redis_client = redis_client_wrapper->GetClient();
  XTRACE("RedisHashSet start");
  auto add_span = opentracing::Tracer::Global()->StartSpan(
      "RedisHashSet", {opentracing::ChildOf(&span->context())});
  auto hset_reply = redis_client->hset(std::to_string(req_id), "text", text);
  auto hlen_reply = redis_client->hincrby(std::to_string(req_id),
      "num_components", 1);
  redis_client->expire(std::to_string(req_id), REDIS_EXPIRE_TIME);
  redis_client->sync_commit();
  add_span->Finish();
  XTRACE("RedisHashSet Complete");
  _redis_client_pool->Push(redis_client_wrapper);

  auto num_components_reply = hlen_reply.get();
  if (!num_components_reply.ok() || !hset_reply.get().ok()) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_REDIS_ERROR;
    se.message = "Failed to retrieve message from Redis";
    XTRACE("Failed to retrieve message from Redis");
    throw se;
  }

  if (num_components_reply.as_integer() == NUM_COMPONENTS) {
    writer_text_map["baggage"] = BRANCH_CURRENT_BAGGAGE().str();
    _ComposeAndUpload(req_id, writer_text_map);
  }

  span->Finish();

  XTRACE("ComposePostHandler::UploadText Complete");
  response.baggage = GET_CURRENT_BAGGAGE().str();
  DELETE_CURRENT_BAGGAGE();
}

void ComposePostHandler::UploadMedia(
    BaseRpcResponse& response,
    int64_t req_id,
    const std::vector<Media> &media,
    const std::map<std::string, std::string> &carrier) {

  auto baggage_it = carrier.find("baggage");
  if (baggage_it != carrier.end()) {
    SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
  }

  if (!XTrace::IsTracing()) {
    XTrace::StartTrace("ComposePostHandler");
  }

  XTRACE("ComposePostHandler::UploadMedia", {{"RequestID", std::to_string(req_id)}});

  // Initialize a span
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map;
  TextMapWriter writer(writer_text_map);
  auto parent_span = opentracing::Tracer::Global()->Extract(reader);
  auto span = opentracing::Tracer::Global()->StartSpan(
      "UploadMedia",
      { opentracing::ChildOf(parent_span->get()) });
  opentracing::Tracer::Global()->Inject(span->context(), writer);

  std::string media_str = "[";
  if (!media.empty()) {
    for (auto &item : media) {
      media_str += "{\"media_id\": " + std::to_string(item.media_id) +
          ", \"media_type\": \"" + item.media_type + "\"},";
    }
    media_str.pop_back();
  }
  media_str += "]";

  auto redis_client_wrapper = _redis_client_pool->Pop();
  if (!redis_client_wrapper) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_REDIS_ERROR;
    se.message = "Cannot connect to Redis server";
    XTRACE("Cannot connect to Redis server");
    throw se;
  }
  auto redis_client = redis_client_wrapper->GetClient();
  XTRACE("RedisHashSet start");
  auto add_span = opentracing::Tracer::Global()->StartSpan(
      "RedisHashSet", {opentracing::ChildOf(&span->context())});
  auto hset_reply = redis_client->hset(std::to_string(req_id),
      "media", media_str);
  auto hlen_reply = redis_client->hincrby(std::to_string(req_id),
                                          "num_components", 1);
  redis_client->expire(std::to_string(req_id), REDIS_EXPIRE_TIME);
  redis_client->sync_commit();
  add_span->Finish();
  XTRACE("RedisHashSet complete");
  _redis_client_pool->Push(redis_client_wrapper);

  auto num_components_reply = hlen_reply.get();
  if (!num_components_reply.ok() || !hset_reply.get().ok()) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_REDIS_ERROR;
    se.message = "Failed to retrieve message from Redis";
    XTRACE("Failed to retrieve message from Redis");
    throw se;
  }

  if (num_components_reply.as_integer() == NUM_COMPONENTS) {
    writer_text_map["baggage"] = BRANCH_CURRENT_BAGGAGE().str();
    _ComposeAndUpload(req_id, writer_text_map);
  }

  span->Finish();

  XTRACE("ComposePostService::UploadMedia complete");
  response.baggage = GET_CURRENT_BAGGAGE().str();
  DELETE_CURRENT_BAGGAGE();
}

void ComposePostHandler::UploadUniqueId(
    BaseRpcResponse& response,
    int64_t req_id,
    const int64_t post_id,
    const PostType::type post_type,
    const std::map<std::string, std::string> &carrier) {

  auto baggage_it = carrier.find("baggage");
  if (baggage_it != carrier.end()) {
    SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
  }

  if (!XTrace::IsTracing()) {
    XTrace::StartTrace("ComposePostHandler");
  }

  XTRACE("ComposePostHandler::UploadUniqueId", {{"RequestID", std::to_string(req_id)}});

  // Initialize a span
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map;
  TextMapWriter writer(writer_text_map);
  auto parent_span = opentracing::Tracer::Global()->Extract(reader);
  auto span = opentracing::Tracer::Global()->StartSpan(
      "UploadUniqueId",
      { opentracing::ChildOf(parent_span->get()) });
  opentracing::Tracer::Global()->Inject(span->context(), writer);

  auto redis_client_wrapper = _redis_client_pool->Pop();
  if (!redis_client_wrapper) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_REDIS_ERROR;
    se.message = "Cannot connect to Redis server";
    XTRACE("Cannot connect to Redis server");
    throw se;
  }
  auto redis_client = redis_client_wrapper->GetClient();
  XTRACE("RedisHashSet start");
  auto add_span = opentracing::Tracer::Global()->StartSpan(
      "RedisHashSet", {opentracing::ChildOf(&span->context())});
  auto hset_reply_0 = redis_client->hset(std::to_string(req_id), "post_id",
      std::to_string(post_id));
  auto hset_reply_1 = redis_client->hset(std::to_string(req_id), "post_type",
      std::to_string(post_type));
  auto hlen_reply = redis_client->hincrby(std::to_string(req_id),
                                          "num_components", 1);
  redis_client->expire(std::to_string(req_id), REDIS_EXPIRE_TIME);
  redis_client->sync_commit();
  add_span->Finish();
  XTRACE("RedisHashSet complete");
  _redis_client_pool->Push(redis_client_wrapper);

  auto num_components_reply = hlen_reply.get();
  if (!num_components_reply.ok() || !hset_reply_0.get().ok() ||
      !hset_reply_1.get().ok()) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_REDIS_ERROR;
    se.message = "Failed to retrieve message from Redis";
    XTRACE("Failed to retrieve message from Redis");
    throw se;
  }
;
  if (num_components_reply.as_integer() == NUM_COMPONENTS) {
    writer_text_map["baggage"] = BRANCH_CURRENT_BAGGAGE().str();
    _ComposeAndUpload(req_id, writer_text_map);
  }

  span->Finish();

  XTRACE("ComposePostService::UploadUniqueId complete");
  response.baggage = GET_CURRENT_BAGGAGE().str();
  DELETE_CURRENT_BAGGAGE();
}

void ComposePostHandler::UploadUrls(
    BaseRpcResponse& response,
    int64_t req_id,
    const std::vector<Url> &urls,
    const std::map<std::string, std::string> &carrier) {

  auto baggage_it = carrier.find("baggage");
  if (baggage_it != carrier.end()) {
    SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
  }

  if (!XTrace::IsTracing()) {
    XTrace::StartTrace("ComposePostHandler");
  }

  XTRACE("ComposePostHandler::UploadUrls", {{"RequestID", std::to_string(req_id)}});

  // Initialize a span
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map;
  TextMapWriter writer(writer_text_map);
  auto parent_span = opentracing::Tracer::Global()->Extract(reader);
  auto span = opentracing::Tracer::Global()->StartSpan(
      "UploadUrls",
      { opentracing::ChildOf(parent_span->get()) });
  opentracing::Tracer::Global()->Inject(span->context(), writer);

  std::string urls_str = "[";
  if (!urls.empty()) {
    XTRACE("List of urls not empty");
    for (auto &item : urls) {
      urls_str += "{\"shortened_url\": \"" + item.shortened_url +
          "\", \"expanded_url\": \"" + item.expanded_url + "\"},";
    }
    urls_str.pop_back();
  }
  urls_str += "]";

  auto redis_client_wrapper = _redis_client_pool->Pop();
  if (!redis_client_wrapper) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_REDIS_ERROR;
    se.message = "Cannot connect to Redis server";
    XTRACE("Cannot connect to Redis server");
    throw se;
  }
  auto redis_client = redis_client_wrapper->GetClient();
  XTRACE("RedisHashSet start");
  auto add_span = opentracing::Tracer::Global()->StartSpan(
      "RedisHashSet", {opentracing::ChildOf(&span->context())});
  auto hset_reply = redis_client->hset(std::to_string(req_id), "urls", urls_str);
  auto hlen_reply = redis_client->hincrby(std::to_string(req_id),
                                          "num_components", 1);
  redis_client->expire(std::to_string(req_id), REDIS_EXPIRE_TIME);
  redis_client->sync_commit();
  add_span->Finish();
  XTRACE("RedisHashSet complete");
  _redis_client_pool->Push(redis_client_wrapper);

  auto num_components_reply = hlen_reply.get();
  if (!num_components_reply.ok() || !hset_reply.get().ok()) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_REDIS_ERROR;
    se.message = "Failed to retrieve message from Redis";
    XTRACE("Failed to retrieve message from Redis");
    throw se;
  }

  if (num_components_reply.as_integer() == NUM_COMPONENTS) {
    writer_text_map["baggage"] = BRANCH_CURRENT_BAGGAGE().str();
    _ComposeAndUpload(req_id, writer_text_map);
  }

  span->Finish();

  XTRACE("ComposePostService::UploadUrls complete");
  response.baggage = GET_CURRENT_BAGGAGE().str();
  DELETE_CURRENT_BAGGAGE();
}

void ComposePostHandler::UploadUserMentions(
    BaseRpcResponse& response,
    const int64_t req_id,
    const std::vector<UserMention> &user_mentions,
    const std::map<std::string, std::string> &carrier) {

  auto baggage_it = carrier.find("baggage");
  if (baggage_it != carrier.end()) {
    SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
  }

  if (!XTrace::IsTracing()) {
    XTrace::StartTrace("ComposePostHandler");
  }

  XTRACE("ComposePostHandler::UploadUserMentions", {{"RequestID", std::to_string(req_id)}});

  // Initialize a span
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map;
  TextMapWriter writer(writer_text_map);
  auto parent_span = opentracing::Tracer::Global()->Extract(reader);
  auto span = opentracing::Tracer::Global()->StartSpan(
      "UploadUserMentions",
      { opentracing::ChildOf(parent_span->get()) });
  opentracing::Tracer::Global()->Inject(span->context(), writer);

  std::string user_mentions_str = "[";
  if (!user_mentions.empty()) {
    XTRACE("List of User Mentions not empty");
    for (auto &item : user_mentions) {
      user_mentions_str += "{\"user_id\": " + std::to_string(item.user_id) +
          ", \"username\": \"" + item.username + "\"},";
    }
    user_mentions_str.pop_back();
  }
  user_mentions_str += "]";

  auto redis_client_wrapper = _redis_client_pool->Pop();
  if (!redis_client_wrapper) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_REDIS_ERROR;
    se.message = "Cannot connect to Redis server";
    XTRACE("Cannot connect to Redis server");
    throw se;
  }
  auto redis_client = redis_client_wrapper->GetClient();
  XTRACE("RedisHashSet start");
  auto add_span = opentracing::Tracer::Global()->StartSpan(
      "RedisHashSet", {opentracing::ChildOf(&span->context())});
  auto hset_reply = redis_client->hset(std::to_string(req_id),
      "user_mentions", user_mentions_str);
  auto hlen_reply = redis_client->hincrby(std::to_string(req_id),
                                          "num_components", 1);
  redis_client->expire(std::to_string(req_id), REDIS_EXPIRE_TIME);
  redis_client->sync_commit();
  add_span->Finish();
  XTRACE("RedisHashSet complete");
  _redis_client_pool->Push(redis_client_wrapper);

  auto num_components_reply = hlen_reply.get();
  if (!num_components_reply.ok() || !hset_reply.get().ok()) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_REDIS_ERROR;
    se.message = "Failed to retrieve message from Redis";
    XTRACE("Failed to retrieve message from Redis");
    throw se;
  }

  if (num_components_reply.as_integer() == NUM_COMPONENTS) {
    writer_text_map["baggage"] = BRANCH_CURRENT_BAGGAGE().str();
    _ComposeAndUpload(req_id, writer_text_map);
  }


  span->Finish();

  XTRACE("ComposePostService::UploadUserMentions complete");
  response.baggage = GET_CURRENT_BAGGAGE().str();
  DELETE_CURRENT_BAGGAGE();
}

void ComposePostHandler::_ComposeAndUpload(
    int64_t req_id,
    const std::map<std::string, std::string> &carrier) {

  auto baggage_it = carrier.find("baggage");
  if (baggage_it != carrier.end()) {
    SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second)); 
  }

  if (!XTrace::IsTracing()) {
    XTrace::StartTrace("ComposePostHandler");
  }

  XTRACE("ComposePostHandler::_ComposeAndUpload Start");

  XTRACE("Redis RetrieveMessages start");
  auto redis_client_wrapper = _redis_client_pool->Pop();
  if (!redis_client_wrapper) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_REDIS_ERROR;
    se.message = "Cannot connect to Redis server";
    XTRACE("Cannot connect to Redis server");
    throw se;
  }
  auto redis_client = redis_client_wrapper->GetClient();
  auto f_text_reply = redis_client->hget(std::to_string(req_id), "text");
  auto f_creator_reply = redis_client->hget(std::to_string(req_id), "creator");
  auto f_media_reply = redis_client->hget(std::to_string(req_id), "media");
  auto f_post_id_reply = redis_client->hget(
      std::to_string(req_id), "post_id");
  auto f_urls_reply = redis_client->hget(std::to_string(req_id), "urls");
  auto f_user_mentions_reply = redis_client->hget(
      std::to_string(req_id), "user_mentions");
  auto f_post_type_reply = redis_client->hget(
      std::to_string(req_id), "post_type");
  redis_client->sync_commit();

  cpp_redis::reply text_reply;
  cpp_redis::reply creator_reply;
  cpp_redis::reply media_reply;
  cpp_redis::reply post_id_reply;
  cpp_redis::reply urls_reply;
  cpp_redis::reply user_mentions_reply;
  cpp_redis::reply post_type_reply;
  try {
    text_reply = f_text_reply.get();
    creator_reply = f_creator_reply.get();
    media_reply = f_media_reply.get();
    post_id_reply = f_post_id_reply.get();
    urls_reply = f_urls_reply.get();
    user_mentions_reply = f_user_mentions_reply.get();
    post_type_reply = f_post_type_reply.get();
  } catch (...) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_REDIS_ERROR;
    se.message = "Failed to retrieve messages from Redis";
    XTRACE("Failed to retrieve messages from Redis");
    _redis_client_pool->Push(redis_client_wrapper);
    throw se;
  }

  if (!text_reply.ok() || !creator_reply.ok() || !media_reply.ok() ||
      !post_id_reply.ok() || !urls_reply.ok() || !user_mentions_reply.ok()) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_REDIS_ERROR;
    se.message = "Failed to retrieve messages from Redis";
    XTRACE("Failed to retrieve messages from Redis");
    _redis_client_pool->Push(redis_client_wrapper);
    throw se;
  }

  _redis_client_pool->Push(redis_client_wrapper);

  XTRACE("Redis RetrieveMessages complete");

  // Compose the post
  XTRACE("Compose Post start");
  Post post;
  post.req_id = req_id;
  post.text = text_reply.as_string();
  post.post_id = std::stoul(post_id_reply.as_string());
  post.timestamp = duration_cast<milliseconds>(
      system_clock::now().time_since_epoch()).count();
  post.post_type = static_cast<PostType::type>(stoi(post_type_reply.as_string()));

  LOG(debug) << creator_reply.as_string();

  json creator_json = json::parse(creator_reply.as_string());
  post.creator.user_id = creator_json["user_id"];
  post.creator.username = creator_json["username"];

  LOG(debug) << user_mentions_reply.as_string();

  std::vector<int64_t> user_mentions_id;

  json user_mentions_json = json::parse(user_mentions_reply.as_string());
  for (auto &item : user_mentions_json) {
    UserMention user_mention;
    user_mention.user_id = item["user_id"];
    user_mention.username = item["username"];
    post.user_mentions.emplace_back(user_mention);
    user_mentions_id.emplace_back(user_mention.user_id);
  }

  json media_json = json::parse(media_reply.as_string());
  for (auto &item : media_json) {
    Media media;
    media.media_id = item["media_id"];
    media.media_type = item["media_type"];
    post.media.emplace_back(media);
  }

  json urls_json = json::parse(urls_reply.as_string());
  for (auto &item : urls_json) {
    Url url;
    url.shortened_url = item["shortened_url"];
    url.expanded_url = item["expanded_url"];
    post.urls.emplace_back(url);
  }

  XTRACE("Compose Post complete");
  // Upload the post
  XTRACE("Upload Post start");
  Baggage upload_post_helper_baggage = BRANCH_CURRENT_BAGGAGE();
  std::promise<Baggage> upload_post_promise;
  std::future<Baggage> upload_post_future = upload_post_promise.get_future();
  std::thread upload_post_worker(&ComposePostHandler::_UploadPostHelper,
                                   this, req_id, std::ref(post), std::ref(carrier), std::ref(upload_post_helper_baggage), std::move(upload_post_promise));

  Baggage upload_user_timeline_helper_baggage = BRANCH_CURRENT_BAGGAGE();
  std::promise<Baggage> upload_user_promise;
  std::future<Baggage> upload_user_future = upload_user_promise.get_future();
  std::thread upload_user_timeline_worker(
      &ComposePostHandler::_UploadUserTimelineHelper, this, req_id,
      post.post_id, post.creator.user_id, post.timestamp, std::ref(carrier), std::ref(upload_user_timeline_helper_baggage), std::move(upload_user_promise));

  Baggage upload_home_timeline_helper_baggage = BRANCH_CURRENT_BAGGAGE();
  std::promise<Baggage> upload_home_promise;
  std::future<Baggage> upload_home_future = upload_home_promise.get_future();
  std::thread upload_home_timeline_worker(
      &ComposePostHandler::_UploadHomeTimelineHelper, this, req_id,
      post.post_id, post.creator.user_id, post.timestamp,
      std::ref(user_mentions_id), std::ref(carrier), std::ref(upload_home_timeline_helper_baggage), std::move(upload_home_promise));

  upload_post_worker.join();
  upload_user_timeline_worker.join();
  upload_home_timeline_worker.join();

  try {
    upload_post_helper_baggage = upload_post_future.get();
    upload_user_timeline_helper_baggage = upload_user_future.get();
    upload_home_timeline_helper_baggage = upload_home_future.get();
  } catch (std::exception &) {
    XTRACE("Error whilst trying to get baggages from futures");
  }

  JOIN_CURRENT_BAGGAGE(upload_post_helper_baggage);
  JOIN_CURRENT_BAGGAGE(upload_user_timeline_helper_baggage);
  JOIN_CURRENT_BAGGAGE(upload_home_timeline_helper_baggage);

  if (_user_timeline_teptr) {
    try{
      std::rethrow_exception(_user_timeline_teptr);
    }
    catch(const std::exception &ex)
    {
      LOG(error) << "Thread exited with exception: " << ex.what();
    }
  }
  if (_rabbitmq_teptr) {
    try{
      std::rethrow_exception(_rabbitmq_teptr);
    }
    catch(const std::exception &ex)
    {
      LOG(error) << "Thread exited with exception: " << ex.what();
    }
  }
  if (_post_storage_teptr) {
    try{
      std::rethrow_exception(_post_storage_teptr);
    }
    catch(const std::exception &ex)
    {
      LOG(error) << "Thread exited with exception: " << ex.what();
    }
  }

  XTRACE("Upload Post complete");
  XTRACE("ComposePostService::_ComposeAndUpload complete");

  DELETE_CURRENT_BAGGAGE();

}

void ComposePostHandler::_UploadPostHelper(
    int64_t req_id,
    const Post &post,
    const std::map<std::string, std::string> &carrier,
    Baggage& baggage, std::promise<Baggage> baggage_promise) {
  BAGGAGE(baggage);
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map(carrier);
  TextMapWriter writer(writer_text_map);
  try{
    auto post_storage_client_wrapper = _post_storage_client_pool->Pop();
    if (!post_storage_client_wrapper) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_THRIFT_CONN_ERROR;
      se.message = "Failed to connect to post-storage-service";
      XTRACE("Failed to connect to post-storage-service");
      throw se;
    }
    auto post_storage_client = post_storage_client_wrapper->GetClient();
    try {
      writer_text_map["baggage"] = BRANCH_CURRENT_BAGGAGE().str();
      BaseRpcResponse response;
      post_storage_client->StorePost(response,req_id, post, writer_text_map);
      Baggage b = Baggage::deserialize(response.baggage);
      JOIN_CURRENT_BAGGAGE(b);
    } catch (...) {
      _post_storage_client_pool->Push(post_storage_client_wrapper);
      LOG(error) << "Failed to store post to post-storage-service";
      XTRACE("Failed to store post to post-storage-service");
      throw;
    }
    _post_storage_client_pool->Push(post_storage_client_wrapper);
  } catch (...) {
    LOG(error) << "Failed to connect to post-storage-service";
    XTRACE("Failed to connect to post-storage-service");
    _post_storage_teptr = std::current_exception();
  }
  baggage_promise.set_value(BRANCH_CURRENT_BAGGAGE());
  DELETE_CURRENT_BAGGAGE();
}

void ComposePostHandler::_UploadUserTimelineHelper(
    int64_t req_id,
    int64_t post_id,
    int64_t user_id,
    int64_t timestamp,
    const std::map<std::string, std::string> &carrier,
    Baggage& baggage, std::promise<Baggage> baggage_promise) {
  BAGGAGE(baggage);
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map(carrier);
  TextMapWriter writer(writer_text_map);
  try{
    auto user_timeline_client_wrapper = _user_timeline_client_pool->Pop();
    if (!user_timeline_client_wrapper) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_THRIFT_CONN_ERROR;
      se.message = "Failed to connect to user-timeline-service";
      XTRACE("Failed to connect to user-timeline-service");
      throw se;
    }
    auto user_timeline_client = user_timeline_client_wrapper->GetClient();
    try {
      writer_text_map["baggage"] = BRANCH_CURRENT_BAGGAGE().str();
      BaseRpcResponse response;
      user_timeline_client->WriteUserTimeline(response, req_id, post_id, user_id,
                                              timestamp, writer_text_map);
      Baggage b = Baggage::deserialize(response.baggage);
      JOIN_CURRENT_BAGGAGE(b);
    } catch (...) {
      _user_timeline_client_pool->Push(user_timeline_client_wrapper);
      throw;
    }
    _user_timeline_client_pool->Push(user_timeline_client_wrapper);
  } catch (...) {
    LOG(error) << "Failed to write user-timeline to user-timeline-service";
    XTRACE("Failed to write user-timeline to user-timeline-service");
    _user_timeline_teptr = std::current_exception();
  }
  baggage_promise.set_value(BRANCH_CURRENT_BAGGAGE());
  DELETE_CURRENT_BAGGAGE();
}

void ComposePostHandler::_UploadHomeTimelineHelper(
    int64_t req_id,
    int64_t post_id,
    int64_t user_id,
    int64_t timestamp,
    const std::vector<int64_t> &user_mentions_id,
    const std::map<std::string, std::string> &carrier,
    Baggage& baggage, std::promise<Baggage> baggage_promise) {
  BAGGAGE(baggage);
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map(carrier);
  TextMapWriter writer(writer_text_map);
  try {
    std::string user_mentions_id_str = "[";
    for (auto &i : user_mentions_id){
      user_mentions_id_str += std::to_string(i) + ", ";
    }
    user_mentions_id_str = user_mentions_id_str.substr(0,
        user_mentions_id_str.length() - 2);
    user_mentions_id_str += "]";
    std::string carrier_str = "{";
    for (auto &item : writer_text_map) {
      carrier_str += "\"" + item.first + "\" : \"" + item.second + "\", ";
    }
    carrier_str = carrier_str.substr(0, carrier_str.length() - 2);
    carrier_str += "}";

    std::string msg_str = "{ \"req_id\": " + std::to_string(req_id) +
        ", \"post_id\": " + std::to_string(post_id) +
        ", \"user_id\": " + std::to_string(user_id) +
        ", \"timestamp\": " + std::to_string(timestamp) +
        ", \"user_mentions_id\": " + user_mentions_id_str +
        ", \"carrier\": " + carrier_str + "}";

    auto rabbitmq_client_wrapper = _rabbitmq_client_pool->Pop();
    if (!rabbitmq_client_wrapper) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_RABBITMQ_CONN_ERROR;
      se.message = "Failed to connect to home-timeline-rabbitmq";
      XTRACE("Failed to connect to home-timeline-rabbitmq");
      throw se;
    }
    auto rabbitmq_channel = rabbitmq_client_wrapper->GetChannel();
    auto msg = AmqpClient::BasicMessage::Create(msg_str);
    rabbitmq_channel->BasicPublish("", "write-home-timeline", msg);
    _rabbitmq_client_pool->Push(rabbitmq_client_wrapper);
  } catch (...) {
    LOG(error) << "Failed to connected to home-timeline-rabbitmq";
    XTRACE("Failed to connect to home-timeline-rabbitmq");
    _rabbitmq_teptr = std::current_exception();
  }
  baggage_promise.set_value(BRANCH_CURRENT_BAGGAGE());
  DELETE_CURRENT_BAGGAGE();
}



} // namespace social_network

#endif //SOCIAL_NETWORK_MICROSERVICES_SRC_COMPOSEPOSTSERVICE_COMPOSEPOSTHANDLER_H_
