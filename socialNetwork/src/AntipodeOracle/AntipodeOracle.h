#ifndef SOCIAL_NETWORK_MICROSERVICES_ANTIPODEORACLE_H
#define SOCIAL_NETWORK_MICROSERVICES_ANTIPODEORACLE_H

#include <iostream>
#include <string>
#include <regex>
#include <future>


#include "../../gen-cpp/AntipodeOracle.h"
#include "../logger.h"
#include "../tracing.h"
#include "../ClientPool.h"
#include "../ThriftClient.h"
#include <xtrace/xtrace.h>
#include <xtrace/baggage.h>

// This autogenerated skeleton file illustrates how to build a server.
// You should copy it to another filename to avoid overwriting it.

#include "AntipodeOracle.h"
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>

#include <tbb/tbb.h>
#include <tbb/concurrent_unordered_set.h>

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

namespace social_network {

  class AntipodeOracleHandler : virtual public AntipodeOracleIf {
  public:
    tbb::concurrent_unordered_set<int64_t, tbb::tbb_hash<int64_t>, std::equal_to<int>> cache;

    AntipodeOracleHandler() {
      // Your initialization goes here
    }

    bool MakeVisible(const int64_t object_id) {
      // Your implementation goes here
      LOG(error) << "MakeVisible: " << object_id;
      cache.insert(object_id);

      tbb::concurrent_unordered_set<int64_t, tbb::tbb_hash<int64_t>, std::equal_to<int>>::iterator cacheit;
      cacheit = cache.begin();
      while (cacheit != cache.end()){
        LOG(error) << "cache: " << *cacheit;
        ++cacheit;
      }
      return 0;
    }

    bool IsVisible(const int64_t object_id) {
      // Your implementation goes here
      LOG(error) << "IsVisible: " << object_id;
      cache.insert(object_id);
    }
  };

} // namespace social_network

#endif //SOCIAL_NETWORK_MICROSERVICES_TEXTHANDLER_H
