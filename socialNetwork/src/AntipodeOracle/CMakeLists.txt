add_executable(
    AntipodeOracle
    AntipodeOracle.cpp
    ${THRIFT_GEN_CPP_DIR}/AntipodeOracle.cpp
    ${THRIFT_GEN_CPP_DIR}/social_network_types.cpp
)

target_include_directories(
    AntipodeOracle PRIVATE
    /usr/local/include/jaegertracing
    /usr/local/include/xtrace
)

target_link_libraries(
    AntipodeOracle
    nlohmann_json::nlohmann_json
    ${THRIFT_LIB}
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_LIBRARIES}
    Boost::system
    Boost::log
    Boost::log_setup
    jaegertracing
    xtrace
    protobuf
)

install(TARGETS AntipodeOracle DESTINATION ./)