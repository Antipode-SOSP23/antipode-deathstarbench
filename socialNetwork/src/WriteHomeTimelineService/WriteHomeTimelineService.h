#ifndef SOCIAL_NETWORK_MICROSERVICES_WRITEHOMETIMELINESERVICE_H
#define SOCIAL_NETWORK_MICROSERVICES_WRITEHOMETIMELINESERVICE_H

#include <iostream>
#include <string>
#include <regex>
#include <future>
#include <chrono>

#include "../../gen-cpp/WriteHomeTimelineService.h"
#include "../logger.h"
#include "../tracing.h"
#include "../ClientPool.h"
#include "../ThriftClient.h"
#include <xtrace/xtrace.h>
#include <xtrace/baggage.h>

#include <tbb/tbb.h>
#include <tbb/concurrent_unordered_set.h>


// This autogenerated skeleton file illustrates how to build a server.
// You should copy it to another filename to avoid overwriting it.

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

using namespace  ::social_network;

// for clock usage
using namespace std::chrono;

namespace social_network {

  tbb::concurrent_unordered_set<int64_t, tbb::tbb_hash<int64_t>, std::equal_to<int>> cache;

  class WriteHomeTimelineServiceHandler : virtual public WriteHomeTimelineServiceIf {
  public:

    WriteHomeTimelineServiceHandler() {
      // Your initialization goes here
    }

    bool MakeVisible(const int64_t object_id, const std::map<std::string, std::string> & carrier) {
      LOG(debug) << "[ANTIPODE][DISTRIBUTED] Making '" << object_id << "' visible ..." ;

      // Jaeger tracing
      TextMapReader span_reader(carrier);
      auto parent_span = opentracing::Tracer::Global()->Extract(span_reader);
      auto span = opentracing::Tracer::Global()->StartSpan(
          "MakeVisible",
          {opentracing::ChildOf(parent_span->get())});
      std::map<std::string, std::string> writer_text_map;
      TextMapWriter writer(writer_text_map);
      opentracing::Tracer::Global()->Inject(span->context(), writer);

      // save ts when notification as placed on rabbitmq
      high_resolution_clock::time_point make_visible_ts = high_resolution_clock::now();
      uint64_t ts = duration_cast<milliseconds>(make_visible_ts.time_since_epoch()).count();
      span->SetTag("make_visible_ts", std::to_string(ts));
      //

      cache.insert(object_id);

      span->Finish();
      return true;
    }

  };

} // namespace social_network

#endif //SOCIAL_NETWORK_MICROSERVICES_WRITEHOMETIMELINESERVICE_H