#ifndef MEDIA_MICROSERVICES_SRC_MOVIEINFOSERVICE_MOVIEINFOHANDLER_H_
#define MEDIA_MICROSERVICES_SRC_MOVIEINFOSERVICE_MOVIEINFOHANDLER_H_

#include <iostream>
#include <string>

#include <libmemcached/memcached.h>
#include <libmemcached/util.h>
#include <mongoc.h>
#include <bson/bson.h>
#include <nlohmann/json.hpp>

#include "../../gen-cpp/MovieInfoService.h"
#include "../logger.h"
#include "../tracing.h"
#include <xtrace/xtrace.h>
#include <xtrace/baggage.h>

namespace media_service {
using json = nlohmann::json;

class MovieInfoHandler : public MovieInfoServiceIf {
 public:
  MovieInfoHandler(
      memcached_pool_st *,
      mongoc_client_pool_t *);
  ~MovieInfoHandler() override = default;
  void ReadMovieInfo(MovieInfoRpcResponse &response, int64_t req_id,
      const std::string& movie_id,
      const std::map<std::string, std::string> & carrier) override;
  void WriteMovieInfo(BaseRpcResponse &response, int64_t req_id, const std::string& movie_id, 
      const std::string& title, const std::vector<Cast> & casts,
      int64_t plot_id, const std::vector<std::string> & thumbnail_ids,
      const std::vector<std::string> & photo_ids,
      const std::vector<std::string> & video_ids,
      const std::string &avg_rating, int32_t num_rating,
      const std::map<std::string, std::string> & carrier) override;
  void UpdateRating(BaseRpcResponse &response, int64_t req_id, const std::string& movie_id,
      int32_t sum_uncommitted_rating, int32_t num_uncommitted_rating,
      const std::map<std::string, std::string> & carrier) override;


 private:
  memcached_pool_st *_memcached_client_pool;
  mongoc_client_pool_t *_mongodb_client_pool;
};

MovieInfoHandler::MovieInfoHandler(
    memcached_pool_st *memcached_client_pool,
    mongoc_client_pool_t *mongodb_client_pool) {
  _memcached_client_pool = memcached_client_pool;
  _mongodb_client_pool = mongodb_client_pool;
}

void MovieInfoHandler::WriteMovieInfo(
    BaseRpcResponse &response,
    int64_t req_id,
    const std::string &movie_id,
    const std::string &title,
    const std::vector<Cast> &casts,
    int64_t plot_id,
    const std::vector<std::string> &thumbnail_ids,
    const std::vector<std::string> &photo_ids,
    const std::vector<std::string> &video_ids,
    const std::string & avg_rating,
    int32_t num_rating,
    const std::map<std::string, std::string> &carrier) {

  std::map<std::string, std::string>::const_iterator baggage_it = carrier.find("baggage");
  if (baggage_it != carrier.end()) {
    SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
  }

  if (!XTrace::IsTracing()) {
    XTrace::StartTrace("MovieInfoHandler");
  }
  XTRACE("MovieInfoHandler::WriteMovieInfo", {{"RequestID", std::to_string(req_id)}});

  // Initialize a span
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map;
  TextMapWriter writer(writer_text_map);
  auto parent_span = opentracing::Tracer::Global()->Extract(reader);
  auto span = opentracing::Tracer::Global()->StartSpan(
      "WriteMovieInfo",
      { opentracing::ChildOf(parent_span->get()) });
  opentracing::Tracer::Global()->Inject(span->context(), writer);

  bson_t *new_doc = bson_new();
  BSON_APPEND_UTF8(new_doc, "movie_id", movie_id.c_str());
  BSON_APPEND_UTF8(new_doc, "title", title.c_str());
  BSON_APPEND_INT64(new_doc, "plot_id", plot_id);
  BSON_APPEND_DOUBLE(new_doc, "avg_rating", std::stod(avg_rating));
  BSON_APPEND_INT64(new_doc, "num_rating", num_rating);
  const char *key;
  int idx = 0;
  char buf[16];
  
  bson_t cast_list;
  BSON_APPEND_ARRAY_BEGIN(new_doc, "casts", &cast_list);
  for (auto &cast : casts) {
    bson_uint32_to_string(idx, &key, buf, sizeof buf);
    bson_t cast_doc;
    BSON_APPEND_DOCUMENT_BEGIN(&cast_list, key, &cast_doc);
    BSON_APPEND_INT64(&cast_doc, "cast_id", cast.cast_id);
    BSON_APPEND_INT64(&cast_doc, "cast_info_id", cast.cast_info_id);
    BSON_APPEND_UTF8(&cast_doc, "character", cast.character.c_str());
    bson_append_document_end(&cast_list, &cast_doc);
    idx++;
  }
  bson_append_array_end(new_doc, &cast_list);
  
  idx = 0;
  bson_t thumbnail_id_list;
  BSON_APPEND_ARRAY_BEGIN(new_doc, "thumbnail_ids", &thumbnail_id_list);
  for (auto &thumbnail_id : thumbnail_ids) {
    bson_uint32_to_string(idx, &key, buf, sizeof buf);
    BSON_APPEND_UTF8(&thumbnail_id_list, key, thumbnail_id.c_str());
    idx++;
  }
  bson_append_array_end(new_doc, &thumbnail_id_list);

  idx = 0;
  bson_t photo_id_list;
  BSON_APPEND_ARRAY_BEGIN(new_doc, "photo_ids", &photo_id_list);
  for (auto &photo_id : photo_ids) {
    bson_uint32_to_string(idx, &key, buf, sizeof buf);
    BSON_APPEND_UTF8(&photo_id_list, key, photo_id.c_str());
    idx++;
  }
  bson_append_array_end(new_doc, &photo_id_list);

  idx = 0;
  bson_t video_id_list;
  BSON_APPEND_ARRAY_BEGIN(new_doc, "video_ids", &video_id_list);
  for (auto &video_id : video_ids) {
    bson_uint32_to_string(idx, &key, buf, sizeof buf);
    BSON_APPEND_UTF8(&video_id_list, key, video_id.c_str());
    idx++;
  }
  bson_append_array_end(new_doc, &video_id_list);

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
      mongodb_client, "movie-info", "movie-info");
  if (!collection) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_MONGODB_ERROR;
    se.message = "Failed to create collection movie-info from DB movie-info";
    XTRACE("Failed to create collection movie-info from DB movie-info");
    mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
    throw se;
  }
  bson_error_t error;
  XTRACE("MongoInsertMovieInfo start");
  auto insert_span = opentracing::Tracer::Global()->StartSpan(
      "MongoInsertMovieInfo", { opentracing::ChildOf(&span->context()) });
  bool plotinsert = mongoc_collection_insert_one (
      collection, new_doc, nullptr, nullptr, &error);
  insert_span->Finish();
  XTRACE("MongoInsertMovieInfo finish");
  if (!plotinsert) {
    LOG(error) << "Error: Failed to insert movie-info to MongoDB: "
               << error.message;
    XTRACE("Error: Failed to insert movie-info to MongoDB " + std::string(error.message));
    ServiceException se;
    se.errorCode = ErrorCode::SE_MONGODB_ERROR;
    se.message = error.message;
    bson_destroy(new_doc);
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
    throw se;
  }

  bson_destroy(new_doc);
  mongoc_collection_destroy(collection);
  mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);

  span->Finish();
  XTRACE("MovieInfoService::WriteMovieInfo complete");
  response.baggage = GET_CURRENT_BAGGAGE().str();
  DELETE_CURRENT_BAGGAGE();
}

void MovieInfoHandler::ReadMovieInfo(
    MovieInfoRpcResponse &response,
    int64_t req_id,
    const std::string &movie_id,
    const std::map<std::string, std::string> &carrier) {

  MovieInfo _return;
  std::map<std::string, std::string>::const_iterator baggage_it = carrier.find("baggage");
  if (baggage_it != carrier.end()) {
    SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
  }

  if (!XTrace::IsTracing()) {
    XTrace::StartTrace("MovieInfoHandler");
  }
  XTRACE("MovieInfoHandler::ReadMovieInfo", {{"RequestID", std::to_string(req_id)}});  

  // Initialize a span
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map;
  TextMapWriter writer(writer_text_map);
  auto parent_span = opentracing::Tracer::Global()->Extract(reader);
  auto span = opentracing::Tracer::Global()->StartSpan(
      "ReadMovieInfo",
      { opentracing::ChildOf(parent_span->get()) });
  opentracing::Tracer::Global()->Inject(span->context(), writer);
  
  memcached_return_t memcached_rc;
  memcached_st *memcached_client = memcached_pool_pop(
      _memcached_client_pool, true, &memcached_rc);
  if (!memcached_client) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_MEMCACHED_ERROR;
    se.message = "Failed to pop a client from memcached pool";
    XTRACE("Failed to pop a client from memcached pool");
    throw se;
  }

  size_t movie_info_mmc_size;
  uint32_t memcached_flags;
  XTRACE("Memcached GetMovieInfo start");
  auto get_span = opentracing::Tracer::Global()->StartSpan(
      "MmcGetMovieInfo", { opentracing::ChildOf(&span->context()) });
  char *movie_info_mmc = memcached_get(
      memcached_client,
      movie_id.c_str(),
      movie_id.length(),
      &movie_info_mmc_size,
      &memcached_flags,
      &memcached_rc);
  if (!movie_info_mmc && memcached_rc != MEMCACHED_NOTFOUND) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_MEMCACHED_ERROR;
    se.message = memcached_strerror(memcached_client, memcached_rc);
    memcached_pool_push(_memcached_client_pool, memcached_client);
    throw se;
  }
  memcached_pool_push(_memcached_client_pool, memcached_client);
  get_span->Finish();
  XTRACE("Memcached GetMovieInfo finish");

  if (movie_info_mmc) {
    LOG(debug) << "Get movie-info " << movie_id << " cache hit from Memcached";
    XTRACE("Cache hit in Memcached for movie " + movie_id);
    json movie_info_json = json::parse(std::string(
        movie_info_mmc, movie_info_mmc + movie_info_mmc_size));
    _return.movie_id = movie_info_json["movie_id"];
    _return.title = movie_info_json["title"];
    _return.avg_rating = movie_info_json["avg_rating"];
    _return.num_rating = movie_info_json["num_rating"];
    _return.plot_id = movie_info_json["plot_id"];
    for (auto &item : movie_info_json["photo_ids"]) {
      _return.photo_ids.emplace_back(item);
    }
    for (auto &item : movie_info_json["video_ids"]) {
      _return.video_ids.emplace_back(item);
    }
    for (auto &item : movie_info_json["thumbnail_ids"]) {
      _return.thumbnail_ids.emplace_back(item);
    }
    for (auto &item : movie_info_json["casts"]) {
      Cast new_cast;
      new_cast.cast_id = item["cast_id"];
      new_cast.cast_info_id = item["cast_info_id"];
      new_cast.character = item["character"];
      _return.casts.emplace_back(new_cast);
    }
    free(movie_info_mmc);
  } else {
    // If not cached in memcached
    XTRACE("Cache miss in Memcached.Checking in MongoDB");
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
        mongodb_client, "movie-info", "movie-info");
    if (!collection) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_MONGODB_ERROR;
      se.message = "Failed to create collection user from DB user";
      XTRACE("Failed to create collection user from DB user");
      mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
      throw se;
    }
    bson_t *query = bson_new();
    BSON_APPEND_UTF8(query, "movie_id", movie_id.c_str());
    XTRACE("MongoFindMovieInfo start");
    auto find_span = opentracing::Tracer::Global()->StartSpan(
        "MongoFindMovieInfo", { opentracing::ChildOf(&span->context()) });
    mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
        collection, query, nullptr, nullptr);
    const bson_t *doc;
    bool found = mongoc_cursor_next(cursor, &doc);
    find_span->Finish();
    XTRACE("MongoFindMovieInfo finish");
    if (!found) {
      bson_error_t error;
      if (mongoc_cursor_error (cursor, &error)) {
        LOG(warning) << error.message;
        XTRACE(error.message);
        bson_destroy(query);
        mongoc_cursor_destroy(cursor);
        mongoc_collection_destroy(collection);
        mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
        ServiceException se;
        se.errorCode = ErrorCode::SE_MONGODB_ERROR;
        se.message = error.message;
        throw se;
      } else {
        LOG(warning) << "Movie_id: " << movie_id << " doesn't exist in MongoDB";
        XTRACE("Movie_id: " + movie_id + " doesn't exist in MongoDB");
        bson_destroy(query);
        mongoc_cursor_destroy(cursor);
        mongoc_collection_destroy(collection);
        mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
        ServiceException se;
        se.errorCode = ErrorCode::SE_THRIFT_HANDLER_ERROR;
        se.message = "Movie_id: " + movie_id + " doesn't exist in MongoDB";
        throw se;
      }
    } else {
      LOG(debug) << "Movie_id: " << movie_id << " found in MongoDB";
      XTRACE("Movie_id: " + movie_id + " found in MongoDB");
      auto movie_info_json_char = bson_as_json(doc, nullptr);
      json movie_info_json = json::parse(movie_info_json_char);
      _return.movie_id = movie_info_json["movie_id"];
      _return.title = movie_info_json["title"];
      _return.avg_rating = movie_info_json["avg_rating"];
      _return.num_rating = movie_info_json["num_rating"];
      _return.plot_id = movie_info_json["plot_id"];
      for (auto &item : movie_info_json["photo_ids"]) {
        _return.photo_ids.emplace_back(item);
      }
      for (auto &item : movie_info_json["video_ids"]) {
        _return.video_ids.emplace_back(item);
      }
      for (auto &item : movie_info_json["thumbnail_ids"]) {
        _return.thumbnail_ids.emplace_back(item);
      }
      for (auto &item : movie_info_json["casts"]) {
        Cast new_cast;
        new_cast.cast_id = item["cast_id"];
        new_cast.cast_info_id = item["cast_info_id"];
        new_cast.character = item["character"];
        _return.casts.emplace_back(new_cast);
      }
      bson_destroy(query);
      mongoc_cursor_destroy(cursor);
      mongoc_collection_destroy(collection);
      mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);

      // upload movie-info to memcached
      XTRACE("Uploading movie info to memcached");
      memcached_client = memcached_pool_pop(
          _memcached_client_pool, true, &memcached_rc);
      if (!memcached_client) {
        ServiceException se;
        se.errorCode = ErrorCode::SE_MEMCACHED_ERROR;
        se.message = "Failed to pop a client from memcached pool";
        XTRACE("Failed to pop a client from memcached pool");
        throw se;
      }
      XTRACE("Memcached SetMovieInfo start");
      auto set_span = opentracing::Tracer::Global()->StartSpan(
          "MmcSetMovieInfo", { opentracing::ChildOf(&span->context()) });

      memcached_rc = memcached_set(
          memcached_client,
          movie_id.c_str(),
          movie_id.length(),
          movie_info_json_char,
          std::strlen(movie_info_json_char),
          static_cast<time_t>(0),
          static_cast<uint32_t>(0));
      if (memcached_rc != MEMCACHED_SUCCESS) {
        LOG(warning) << "Failed to set movie_info to Memcached: "
                     << memcached_strerror(memcached_client, memcached_rc);
        XTRACE("Failed to set movie_info to Memcached");
      }
      set_span->Finish();
      XTRACE("Memcached SetMovieInfo complete");
      bson_free(movie_info_json_char);
      memcached_pool_push(_memcached_client_pool, memcached_client);
    }
  }
  span->Finish();
  XTRACE("MovieInfoHandler::ReadMovieInfo complete");
  response.baggage = GET_CURRENT_BAGGAGE().str();
  response.result = _return;
  DELETE_CURRENT_BAGGAGE();
}

void MovieInfoHandler::UpdateRating(
    BaseRpcResponse &response, int64_t req_id, const std::string& movie_id,
    int32_t sum_uncommitted_rating, int32_t num_uncommitted_rating,
    const std::map<std::string, std::string> & carrier) {

  std::map<std::string, std::string>::const_iterator baggage_it = carrier.find("baggage");
  if (baggage_it != carrier.end()) {
    SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
  }

  if (!XTrace::IsTracing()) {
    XTrace::StartTrace("MovieInfoHandler");
  }
  XTRACE("MovieInfoHandler::UpdateRating", {{"RequestID", std::to_string(req_id)}});

  // Initialize a span
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map;
  TextMapWriter writer(writer_text_map);
  auto parent_span = opentracing::Tracer::Global()->Extract(reader);
  auto span = opentracing::Tracer::Global()->StartSpan(
      "UpdateRating",
      { opentracing::ChildOf(parent_span->get()) });
  opentracing::Tracer::Global()->Inject(span->context(), writer);

  bson_t *query = bson_new();
  BSON_APPEND_UTF8(query, "movie_id", movie_id.c_str());

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
      mongodb_client, "social-graph", "social-graph");
  if (!collection) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_MONGODB_ERROR;
    se.message = "Failed to create collection social_graph from MongoDB";
    XTRACE("Failed to create colletion social_graph from MongoDB");
    mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
    throw se;
  }
  XTRACE("MongoFindMovieInfo start");
  auto find_span = opentracing::Tracer::Global()->StartSpan(
      "MongoFindMovieInfo", {opentracing::ChildOf(&span->context())});
  mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
      collection, query, nullptr, nullptr);
  XTRACE("MongoFindMovieInfo finish");
  const bson_t *doc;
  bool found = mongoc_cursor_next(cursor, &doc);
  if (found) {
    XTRACE("MovieInfo found in MongoDB");
    bson_iter_t iter_0;
    bson_iter_t iter_1;
    bson_iter_init(&iter_0, doc);
    bson_iter_init(&iter_1, doc);
    double avg_rating;
    int32_t num_rating;
    if (bson_iter_init_find(&iter_0, doc, "avg_rating") &&
        bson_iter_init_find(&iter_1, doc, "num_rating")) {
      avg_rating = bson_iter_value(&iter_0)->value.v_double;
      num_rating = bson_iter_value(&iter_1)->value.v_int32;

      avg_rating = (avg_rating * num_rating + sum_uncommitted_rating) /
          (num_rating + num_uncommitted_rating);
      num_rating += num_uncommitted_rating;

      bson_t *update = BCON_NEW(
          "$set", "{",
          "avg_rating", BCON_DOUBLE(avg_rating),
          "num_rating", BCON_INT32(num_rating), "}");
      bson_error_t error;
      bson_t reply;
      XTRACE("MongoUpdateRating start");
      auto update_span = opentracing::Tracer::Global()->StartSpan(
          "MongoUpdateRating", {opentracing::ChildOf(&span->context())});
      bool updated = mongoc_collection_find_and_modify(
          collection,
          query,
          nullptr,
          update,
          nullptr,
          false,
          false,
          true,
          &reply,
          &error);
      if (!updated) {
        LOG(error) << "Failed to update rating for movie " << movie_id
                   << " to MongoDB: " << error.message;
        XTRACE("Failed to update rating for movie " + movie_id + " to MongoDB");
        ServiceException se;
        se.errorCode = ErrorCode::SE_MONGODB_ERROR;
        se.message = "Failed to update rating for movie " + movie_id +
            " to MongoDB: " + error.message;
        bson_destroy(&reply);
        bson_destroy(update);
        mongoc_collection_destroy(collection);
        mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
        throw se;
      }
      update_span->Finish();
      XTRACE("MongoUpdateRating finish");
    }
  }

  XTRACE("Memcached Delete Start");
  auto delete_span = opentracing::Tracer::Global()->StartSpan(
      "MmcDelete", {opentracing::ChildOf(&span->context())});
  memcached_return_t memcached_rc;
  memcached_st *memcached_client = memcached_pool_pop(
      _memcached_client_pool, true, &memcached_rc);
  if (!memcached_client) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_MEMCACHED_ERROR;
    se.message = "Failed to pop a client from memcached pool";
    XTRACE("Failed to pop a client from memcached pool");
    throw se;
  }
  memcached_delete(memcached_client, movie_id.c_str(), movie_id.length(), 0);
  memcached_pool_push(_memcached_client_pool, memcached_client);
  delete_span->Finish();
  XTRACE("Memcached Delete finish");

  span->Finish();
  XTRACE("MovieInfoHandler::UpdateRating complete");
  response.baggage = GET_CURRENT_BAGGAGE().str();
  DELETE_CURRENT_BAGGAGE();
}

} // namespace media_service

#endif //MEDIA_MICROSERVICES_SRC_MOVIEINFOSERVICE_MOVIEINFOHANDLER_H_
