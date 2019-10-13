#ifndef MEDIA_MICROSERVICES_SRC_COMPOSEPAGESERVICE_COMPOSEPAGEHANDLER_H_
#define MEDIA_MICROSERVICES_SRC_COMPOSEPAGESERVICE_COMPOSEPAGEHANDLER_H_

#include <iostream>
#include <string>
#include <future>

#include "../../gen-cpp/PageService.h"
#include "../../gen-cpp/MovieReviewService.h"
#include "../../gen-cpp/MovieInfoService.h"
#include "../../gen-cpp/CastInfoService.h"
#include "../../gen-cpp/PlotService.h"
#include "../logger.h"
#include "../tracing.h"
#include "../ClientPool.h"
#include "../ThriftClient.h"
#include <xtrace/xtrace.h>
#include <xtrace/baggage.h>


namespace media_service {

class PageHandler : public PageServiceIf {
 public:
  PageHandler(
      ClientPool<ThriftClient<MovieReviewServiceClient>> *,
      ClientPool<ThriftClient<MovieInfoServiceClient>> *,
      ClientPool<ThriftClient<CastInfoServiceClient>> *,
      ClientPool<ThriftClient<PlotServiceClient>> *);
  ~PageHandler() override = default;

  void ReadPage(Page& _return, int64_t req_id, const std::string& movie_id,
                int32_t review_start, int32_t review_stop,
                const std::map<std::string, std::string> & carrier) override;

 private:
  ClientPool<ThriftClient<MovieReviewServiceClient>> *_movie_review_client_pool;
  ClientPool<ThriftClient<MovieInfoServiceClient>> *_movie_info_client_pool;
  ClientPool<ThriftClient<CastInfoServiceClient>> *_cast_info_client_pool;
  ClientPool<ThriftClient<PlotServiceClient>> *_plot_client_pool;
};
PageHandler::PageHandler(
    ClientPool<ThriftClient<MovieReviewServiceClient>> *movie_review_client_pool,
    ClientPool<ThriftClient<MovieInfoServiceClient>> *movie_info_client_pool,
    ClientPool<ThriftClient<CastInfoServiceClient>> *cast_info_client_pool,
    ClientPool<ThriftClient<PlotServiceClient>> *plot_client_pool) {
  _movie_review_client_pool = movie_review_client_pool;
  _movie_info_client_pool = movie_info_client_pool;
  _cast_info_client_pool = cast_info_client_pool;
  _plot_client_pool = plot_client_pool;
}
void PageHandler::ReadPage(
    Page &_return,
    int64_t req_id,
    const std::string &movie_id,
    int32_t review_start,
    int32_t review_stop,
    const std::map<std::string, std::string> &carrier) {

  std::map<std::string, std::string>::const_iterator baggage_it = carrier.find("baggage");
  if (baggage_it != carrier.end()) {
    SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
  }

  if (!XTrace::IsTracing()) {
    XTrace::StartTrace("PageHandler");
  }
  XTRACE("PageHandler::ReadPage", {{"RequestID", std::to_string(req_id)}});

  // Initialize a span
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map;
  TextMapWriter writer(writer_text_map);
  auto parent_span = opentracing::Tracer::Global()->Extract(reader);
  auto span = opentracing::Tracer::Global()->StartSpan(
      "ReadPage",
      { opentracing::ChildOf(parent_span->get()) });
  opentracing::Tracer::Global()->Inject(span->context(), writer);

  std::future<std::vector<Review>> movie_review_future;
  std::future<MovieInfo> movie_info_future;
  std::future<std::vector<CastInfo>> cast_info_future;
  std::future<std::string> plot_future;

  Baggage movie_info_baggage = BRANCH_CURRENT_BAGGAGE();

  movie_info_future = std::async(std::launch::async, [&](){
    BAGGAGE(movie_info_baggage);
    MovieInfo _reture_movie_info;
    auto movie_info_client_wrapper = _movie_info_client_pool->Pop();
    if (!movie_info_client_wrapper) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_THRIFT_CONN_ERROR;
      se.message = "Failed to connected to movie-info-service";
      XTRACE("Failed to connect to movie-info-service");
      throw se;
    }
    auto movie_info_client = movie_info_client_wrapper->GetClient();
    try {
      writer_text_map["baggage"] = GET_CURRENT_BAGGAGE().str();
      movie_info_client->ReadMovieInfo(_reture_movie_info,
          req_id, movie_id, writer_text_map);
    } catch (...) {
      _movie_info_client_pool->Push(movie_info_client_wrapper);
      LOG(error) << "Failed to read movie_info to movie-info-service";
      XTRACE("Failed to read movie_info to movie-info-service");
      throw;
    }
    _movie_info_client_pool->Push(movie_info_client_wrapper);
    return _reture_movie_info;
  });

  Baggage movie_review_baggage = BRANCH_CURRENT_BAGGAGE();

  movie_review_future = std::async(std::launch::async, [&](){
    BAGGAGE(movie_review_baggage);
    std::vector<Review> _return_movie_reviews;
    auto movie_review_client_wrapper = _movie_review_client_pool->Pop();
    if (!movie_review_client_wrapper) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_THRIFT_CONN_ERROR;
      se.message = "Failed to connected to movie-review-service";
      XTRACE("Failed to connect to movie-review-service");
      throw se;
    }
    auto movie_review_client = movie_review_client_wrapper->GetClient();
    try {
      writer_text_map["baggage"] = GET_CURRENT_BAGGAGE().str();
      movie_review_client->ReadMovieReviews(_return_movie_reviews,
          req_id, movie_id, review_start, review_stop, writer_text_map);
    } catch (...) {
      _movie_review_client_pool->Push(movie_review_client_wrapper);
      LOG(error) << "Failed to read reviews to movie-review-service";
      XTRACE("Failed to read reviews to movie-review-service");
      throw;
    }
    _movie_review_client_pool->Push(movie_review_client_wrapper);
    return _return_movie_reviews;
  });

  try {
    _return.movie_info = movie_info_future.get();
    JOIN_CURRENT_BAGGAGE(movie_info_baggage);
  } catch (...) {
    throw;
  }
  
  std::vector<int64_t> cast_info_ids;
  for (auto &cast : _return.movie_info.casts) {
    cast_info_ids.emplace_back(cast.cast_info_id);
  }

  Baggage cast_info_baggage = BRANCH_CURRENT_BAGGAGE();

  cast_info_future = std::async(std::launch::async, [&](){
    BAGGAGE(cast_info_baggage);
    std::vector<CastInfo> _return_cast_infos;
    auto cast_info_client_wrapper = _cast_info_client_pool->Pop();
    if (!cast_info_client_wrapper) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_THRIFT_CONN_ERROR;
      se.message = "Failed to connected to cast-info-service";
      XTRACE("Failed to connect to cast-info-service");
      throw se;
    }
    auto cast_info_client = cast_info_client_wrapper->GetClient();
    try {
      writer_text_map["baggage"] = GET_CURRENT_BAGGAGE().str();
      cast_info_client->ReadCastInfo(_return_cast_infos, req_id,
          cast_info_ids, writer_text_map);
    } catch (...) {
      _cast_info_client_pool->Push(cast_info_client_wrapper);
      LOG(error) << "Failed to read cast-info to cast-info-service";
      XTRACE("Failed to read cast-info from cast-info-service");
      throw;
    }
    _cast_info_client_pool->Push(cast_info_client_wrapper);
    return _return_cast_infos;
  });

  Baggage plot_baggage = BRANCH_CURRENT_BAGGAGE();

  plot_future = std::async(std::launch::async, [&](){
    BAGGAGE(plot_baggage);
    std::string _return_plot;
    auto plot_client_wrapper = _plot_client_pool->Pop();
    if (!plot_client_wrapper) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_THRIFT_CONN_ERROR;
      se.message = "Failed to connected to plot-service";
      XTRACE("Failed to connect to plot-service");
      throw se;
    }
    auto plot_client = plot_client_wrapper->GetClient();
    try {
      writer_text_map["baggage"] = GET_CURRENT_BAGGAGE().str();
      plot_client->ReadPlot(_return_plot, req_id, _return.movie_info.plot_id,
          writer_text_map);
    } catch (...) {
      _plot_client_pool->Push(plot_client_wrapper);
      LOG(error) << "Failed to read plot to plot-service";
      XTRACE("Failed to read plot to plot-service");
      throw;
    }
    _plot_client_pool->Push(plot_client_wrapper);
    return _return_plot;
  });

  try {
    _return.reviews = movie_review_future.get();
    JOIN_CURRENT_BAGGAGE(movie_review_baggage);
    _return.plot = plot_future.get();
    JOIN_CURRENT_BAGGAGE(plot_baggage);
    _return.cast_infos = cast_info_future.get();
    JOIN_CURRENT_BAGGAGE(cast_info_baggage);
  } catch (...) {
    throw;
  }
  span->Finish();
  XTRACE("PageHandler::ReadPage complete");
  DELETE_CURRENT_BAGGAGE();
}

} //namespace media_service


#endif //MEDIA_MICROSERVICES_SRC_COMPOSEPAGESERVICE_COMPOSEPAGEHANDLER_H_
