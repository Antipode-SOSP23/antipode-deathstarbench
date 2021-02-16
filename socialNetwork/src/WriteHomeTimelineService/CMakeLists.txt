add_executable(
    WriteHomeTimelineService
    WriteHomeTimelineService.cpp
    ${THRIFT_GEN_CPP_DIR}/SocialGraphService.cpp
    ${THRIFT_GEN_CPP_DIR}/PostStorageService.cpp
    ${THRIFT_GEN_CPP_DIR}/social_network_types.cpp
    ${THRIFT_GEN_CPP_DIR}/AntipodeOracle.cpp
    ${THRIFT_GEN_CPP_DIR}/WriteHomeTimelineService.cpp
)

target_include_directories(
    WriteHomeTimelineService PRIVATE
    /usr/local/include/jaegertracing
    /usr/local/include/cpp_redis
    ${LIBEVENT_INCLUDE_DIRS}
    /usr/local/include/xtrace
)


target_link_libraries(
    WriteHomeTimelineService
    ${THRIFT_LIB}
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_LIBRARIES}
    nlohmann_json::nlohmann_json
    Boost::system
    Boost::log
    Boost::log_setup
    OpenSSL::SSL
    /usr/local/lib/libjaegertracing.so
    /usr/local/lib/libamqpcpp.so
    ${LIBEVENT_LIBRARIES}
    /usr/local/lib/libcpp_redis.a
    /usr/local/lib/libtacopie.a
    jaegertracing
    xtrace
    protobuf
    tbb
)

install(TARGETS WriteHomeTimelineService DESTINATION ./)
