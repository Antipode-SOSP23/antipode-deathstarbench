#ifndef MEDIA_MICROSERVICES_TEXTHANDLER_H
#define MEDIA_MICROSERVICES_TEXTHANDLER_H

#include <iostream>
#include <string>

#include "../../gen-cpp/TextService.h"
#include "../../gen-cpp/ComposeReviewService.h"
#include "../ClientPool.h"
#include "../ThriftClient.h"
#include "../logger.h"
#include "../tracing.h"
#include <xtrace/xtrace.h>
#include <xtrace/baggage.h>

namespace media_service {

class TextHandler : public TextServiceIf {
 public:
  explicit TextHandler(ClientPool<ThriftClient<ComposeReviewServiceClient>> *);
  ~TextHandler() override = default;

  void UploadText(BaseRpcResponse &, int64_t, const std::string &,
      const std::map<std::string, std::string> &) override;
 private:
  ClientPool<ThriftClient<ComposeReviewServiceClient>> *_compose_client_pool;
};

TextHandler::TextHandler(
    ClientPool<ThriftClient<ComposeReviewServiceClient>> *compose_client_pool) {
  _compose_client_pool = compose_client_pool;
}

void TextHandler::UploadText(
    BaseRpcResponse &response,
    int64_t req_id,
    const std::string &text,
    const std::map<std::string, std::string> & carrier) {

  std::map<std::string, std::string>::const_iterator baggage_it = carrier.find("baggage");
  if (baggage_it != carrier.end()) {
    SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
  }

  if (!XTrace::IsTracing()) {
    XTrace::StartTrace("TextHandler");
  }
  XTRACE("TextHandler::UploadText", {{"RequestID", std::to_string(req_id)}});

  // Initialize a span
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map;
  TextMapWriter writer(writer_text_map);
  auto parent_span = opentracing::Tracer::Global()->Extract(reader);
  auto span = opentracing::Tracer::Global()->StartSpan(
      "UploadText",
      { opentracing::ChildOf(parent_span->get()) });
  opentracing::Tracer::Global()->Inject(span->context(), writer);

  auto compose_client_wrapper = _compose_client_pool->Pop();
  if (!compose_client_wrapper) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_THRIFT_CONN_ERROR;
    se.message = "Failed to connected to compose-review-service";
    XTRACE("Failed to connect to compose-review-service");
    throw se;
  }
  auto compose_client = compose_client_wrapper->GetClient();
  Baggage compose_client_baggage = BRANCH_CURRENT_BAGGAGE();
  try {
    writer_text_map["baggage"] = compose_client_baggage.str();
    BaseRpcResponse response;
    compose_client->UploadText(response, req_id, text, writer_text_map);
    Baggage b = Baggage::deserialize(response.baggage);
    JOIN_CURRENT_BAGGAGE(b);
  } catch (...) {
    _compose_client_pool->Push(compose_client_wrapper);
    LOG(error) << "Failed to upload movie_id to compose-review-service";
    XTRACE("Failed to upload movie_id to compose-review-service");
    throw;
  }
  _compose_client_pool->Push(compose_client_wrapper);

  span->Finish();

  XTRACE("TextHandler::UploadText complete");
  response.baggage = GET_CURRENT_BAGGAGE().str();
  DELETE_CURRENT_BAGGAGE();
}

} //namespace media_service





#endif //MEDIA_MICROSERVICES_TEXTHANDLER_H
