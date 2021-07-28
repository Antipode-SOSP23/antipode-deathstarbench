FROM ubuntu:16.04

ARG NUM_CPUS=12
ARG BUILD_DEPS="ca-certificates \
                g++ \
                wget \
                git \
                libmemcached-dev \
                automake \
                bison \
                flex \
                libevent-dev \
                libssl-dev \
                libtool \
                make \
                pkg-config \
                librabbitmq-dev \
                lua5.1 \
                lua5.1-dev"

RUN set -ex ;\
  apt-get update ;\
  apt-get install -y --no-install-recommends ${BUILD_DEPS} ;\
  rm -rf /var/lib/apt/lists/*

#---------
# CMake
#---------
ARG CMAKE_VERSION=3.13.4
RUN set -ex ;\
  apt purge --auto-remove -y cmake ;\
  cd /tmp ;\
  wget https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}.tar.gz ;\
  tar -xzvf cmake-${CMAKE_VERSION}.tar.gz ;\
  cd cmake-${CMAKE_VERSION} ;\
  ./bootstrap ;\
  make -j${NUM_CPUS} ;\
  make install ;\
  cd /tmp ;\
  rm -rf cmake-${CMAKE_VERSION}.tar.gz \
         cmake-${CMAKE_VERSION}

#---------
# Boost
#---------
ARG LIB_BOOST_VERSION=1.68.0
RUN set -ex ;\
  cd /tmp ;\
  export LIB_BOOST_VERSION_UNDERSCORED=$(echo "$LIB_BOOST_VERSION" | tr '.' '_') ;\
  wget https://boostorg.jfrog.io/artifactory/main/release/$LIB_BOOST_VERSION/source/boost_$LIB_BOOST_VERSION_UNDERSCORED.tar.gz ;\
  tar -xzvf boost_${LIB_BOOST_VERSION_UNDERSCORED}.tar.gz ;\
  cd boost_${LIB_BOOST_VERSION_UNDERSCORED} ;\
  ./bootstrap.sh ;\
  ./b2 install cxxstd=14 ;\
  cd /tmp ;\
  rm -rf boost_${LIB_BOOST_VERSION_UNDERSCORED}.tar.gz \
         boost_${LIB_BOOST_VERSION_UNDERSCORED}

#---------
# mongo-c-driver
#---------
ARG LIB_MONGOC_VERSION=1.14.0
RUN set -ex ;\
    cd /tmp ;\
    wget https://github.com/mongodb/mongo-c-driver/releases/download/${LIB_MONGOC_VERSION}/mongo-c-driver-${LIB_MONGOC_VERSION}.tar.gz ;\
    tar -zxf mongo-c-driver-${LIB_MONGOC_VERSION}.tar.gz ;\
    cd mongo-c-driver-${LIB_MONGOC_VERSION} ;\
    mkdir -p cmake-build ;\
    cd cmake-build ;\
    cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTS=0 -DENABLE_EXAMPLES=0 .. ;\
    make -j${NUM_CPUS} ;\
    make install ;\
    cd /tmp ;\
    rm -rf mongo-c-driver-${LIB_MONGOC_VERSION}.tar.gz \
           mongo-c-driver-${LIB_MONGOC_VERSION}

#---------
# lib-thrift
#---------
ARG LIB_THRIFT_VERSION=0.12.0
RUN set -ex ;\
    cd /tmp ;\
    wget -O thrift-${LIB_THRIFT_VERSION}.tar.gz https://github.com/apache/thrift/archive/v${LIB_THRIFT_VERSION}.tar.gz ;\
    tar -zxf thrift-${LIB_THRIFT_VERSION}.tar.gz ;\
    cd thrift-${LIB_THRIFT_VERSION} ;\
    mkdir -p cmake-build ;\
    cd cmake-build ;\
    cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=0 .. ;\
    make -j${NUM_CPUS} ;\
    make install ;\
    cd /tmp ;\
    rm -rf thrift-${LIB_THRIFT_VERSION}.tar.gz \
           thrift-${LIB_THRIFT_VERSION}

#---------
# nlohmann/json
#---------
ARG LIB_JSON_VERSION=3.6.1
RUN set -ex ;\
    cd /tmp ;\
    wget -O json-${LIB_JSON_VERSION}.tar.gz https://github.com/nlohmann/json/archive/v${LIB_JSON_VERSION}.tar.gz ;\
    tar -zxf json-${LIB_JSON_VERSION}.tar.gz ;\
    cd json-${LIB_JSON_VERSION} ;\
    mkdir -p cmake-build ;\
    cd cmake-build ;\
    cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=0 .. ;\
    make -j${NUM_CPUS} ;\
    make install ;\
    cd /tmp ;\
    rm -rf json-${LIB_JSON_VERSION}.tar.gz \
           json-${LIB_JSON_VERSION}

#---------
# yaml-cpp
#---------
ARG LIB_YAML_VERSION=0.6.2
RUN set -ex ;\
    cd /tmp ;\
    wget -O yaml-cpp-${LIB_YAML_VERSION}.tar.gz https://github.com/jbeder/yaml-cpp/archive/yaml-cpp-${LIB_YAML_VERSION}.tar.gz ;\
    tar -zxf yaml-cpp-${LIB_YAML_VERSION}.tar.gz ;\
    cd yaml-cpp-yaml-cpp-${LIB_YAML_VERSION} ;\
    mkdir -p cmake-build ;\
    cd cmake-build ;\
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-fPIC" -DYAML_CPP_BUILD_TESTS=0 .. ;\
    make -j${NUM_CPUS} ;\
    make install ;\
    cd /tmp ;\
    rm -rf yaml-cpp-${LIB_YAML_VERSION}.tar.gz \
           yaml-cpp-yaml-cpp-${LIB_YAML_VERSION}

#---------
# opentracing-cpp
#---------
ARG LIB_OPENTRACING_VERSION=1.5.1
RUN set -ex ;\
    cd /tmp ;\
    wget -O opentracing-cpp-${LIB_OPENTRACING_VERSION}.tar.gz https://github.com/opentracing/opentracing-cpp/archive/v${LIB_OPENTRACING_VERSION}.tar.gz ;\
    tar -zxf opentracing-cpp-${LIB_OPENTRACING_VERSION}.tar.gz ;\
    cd opentracing-cpp-${LIB_OPENTRACING_VERSION} ;\
    mkdir -p cmake-build ;\
    cd cmake-build ;\
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-fPIC" -DBUILD_TESTING=0 .. ;\
    make -j${NUM_CPUS} ;\
    make install ;\
    cd /tmp ;\
    rm -rf opentracing-cpp-${LIB_OPENTRACING_VERSION}.tar.gz \
           opentracing-cpp-${LIB_OPENTRACING_VERSION}

#---------
# jaeger-client-cpp
#---------
ARG LIB_JAEGER_VERSION=0.4.2
RUN set -ex ;\
    cd /tmp ;\
    wget -O jaeger-client-cpp-${LIB_JAEGER_VERSION}.tar.gz https://github.com/jaegertracing/jaeger-client-cpp/archive/v${LIB_JAEGER_VERSION}.tar.gz ;\
    tar -zxf jaeger-client-cpp-${LIB_JAEGER_VERSION}.tar.gz ;\
    cd jaeger-client-cpp-${LIB_JAEGER_VERSION} ;\
    mkdir -p cmake-build ;\
    cd cmake-build ;\
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-fPIC" -DHUNTER_ENABLED=0 -DBUILD_TESTING=0 -DJAEGERTRACING_WITH_YAML_CPP=1 -DJAEGERTRACING_BUILD_EXAMPLES=0 .. ;\
    make -j${NUM_CPUS} ;\
    make install ;\
    cd /tmp ;\
    rm -rf jaeger-client-cpp-${LIB_JAEGER_VERSION}.tar.gz \
           jaeger-client-cpp-${LIB_JAEGER_VERSION}

#---------
# cpp-jwt
#---------
ARG LIB_CPP_JWT_VERSION=1.1.1
RUN set -ex ;\
    cd /tmp ;\
    wget -O cpp-jwt-${LIB_CPP_JWT_VERSION}.tar.gz https://github.com/arun11299/cpp-jwt/archive/v${LIB_CPP_JWT_VERSION}.tar.gz ;\
    tar -zxf cpp-jwt-${LIB_CPP_JWT_VERSION}.tar.gz ;\
    cd cpp-jwt-${LIB_CPP_JWT_VERSION} ;\
    cp -R include/jwt /usr/local/include ;\
    # use the dependency in /usr/local/include instead of in jwt/json
    rm -rf /usr/local/include/jwt/json ;\
    sed -i 's/\#include \"jwt\/json\/json.hpp\"/\#include \<nlohmann\/json\.hpp\>/g' /usr/local/include/jwt/jwt.hpp ;\
    cd /tmp ;\
    rm -rf cpp-jwt-${LIB_CPP_JWT_VERSION}.tar.gz \
           cpp-jwt-${LIB_CPP_JWT_VERSION}

#---------
# cpp_redis
#---------
ARG LIB_CPP_REDIS_VERSION=4.3.1
RUN set -ex ;\
    cd /tmp ;\
    git clone https://github.com/cpp-redis/cpp_redis.git ;\
    cd cpp_redis && git checkout ${LIB_CPP_REDIS_VERSION} ;\
    git submodule init && git submodule update ;\
    mkdir cmake-build && cd cmake-build ;\
    cmake .. -DCMAKE_BUILD_TYPE=Release ;\
    make -j${NUM_CPUS} ;\
    make install ;\
    cd /tmp ;\
    rm -rf cpp_redis

#---------
# AMQP-CPP
#---------
ARG LIB_AMQP_CPP_VERSION=4.1.4
RUN set -ex ;\
    cd /tmp ;\
    git clone https://github.com/CopernicaMarketingSoftware/AMQP-CPP.git ;\
    cd AMQP-CPP && git checkout v${LIB_AMQP_CPP_VERSION} ;\
    mkdir cmake-build && cd cmake-build ;\
    cmake .. -DCMAKE_BUILD_TYPE=Release -DAMQP-CPP_BUILD_SHARED=on -DAMQP-CPP_LINUX_TCP=on ;\
    make -j${NUM_CPUS} && make install ;\
    cd /tmp ;\
    rm -rf AMQP-CPP

#---------
# SimpleAmqpClient
#---------
ARG LIB_SIMPLEAMQPCLIENT_VERSION=2.4.0
RUN set -ex ;\
    cd /tmp ;\
    git clone https://github.com/alanxz/SimpleAmqpClient.git ;\
    cd SimpleAmqpClient ;\
    git checkout v${LIB_SIMPLEAMQPCLIENT_VERSION} ;\
    mkdir cmake-build && cd cmake-build ;\
    cmake .. -DCMAKE_BUILD_TYPE=Release ;\
    make -j${NUM_CPUS} ;\
    make install ;\
    cd /tmp ;\
    rm -rf SimpleAmqpClient

#---------
# Protobuf
#---------
ARG LIB_PROTOBUF_VERSION=2.6.1
RUN set -ex ;\
    cd /tmp ;\
    wget https://github.com/protocolbuffers/protobuf/releases/download/v${LIB_PROTOBUF_VERSION}/protobuf-${LIB_PROTOBUF_VERSION}.tar.gz ;\
    gunzip protobuf-${LIB_PROTOBUF_VERSION}.tar.gz ;\
    tar -xvf protobuf-${LIB_PROTOBUF_VERSION}.tar ;\
    cd protobuf-${LIB_PROTOBUF_VERSION} ;\
    ./configure --prefix=/usr ;\
    make ;\
    make install ;\
    cd /tmp ;\
    rm -rf protobuf-${LIB_PROTOBUF_VERSION}.tar \
           protobuf-${LIB_PROTOBUF_VERSION}

#---------
# XTrace
#---------
RUN set -ex ;\
    cd /tmp ;\
    git clone https://github.com/JonathanMace/xtrace-cpp.git ;\
    cd xtrace-cpp ;\
    mkdir build ;\
    cd build ;\
    cmake .. ;\
    make ;\
    make install ;\
    cd /tmp ;\
    rm -rf xtrace-cpp

#---------
# Dockerize
#---------
ENV DOCKERIZE_VERSION v0.6.1
RUN set -ex ;\
    wget https://github.com/jwilder/dockerize/releases/download/$DOCKERIZE_VERSION/dockerize-linux-amd64-$DOCKERIZE_VERSION.tar.gz ;\
    tar -C /usr/local/bin -xzvf dockerize-linux-amd64-$DOCKERIZE_VERSION.tar.gz ;\
    rm dockerize-linux-amd64-$DOCKERIZE_VERSION.tar.gz

#---------
# Run
#---------
ARG RUN_DEPS="g++ \
              make \
              libssl-dev \
              libevent-dev \
              libmemcached-dev \
              librabbitmq-dev \
              lua5.1 \
              lua5.1-dev \
              libicu-dev \
              libtbb-dev \
              libcereal-dev \
              net-tools \
              iproute2"
# Development dependencies - comment if not needed
ARG DEV_DEPS="tree \
              less \
              iputils-ping \
              vim"
ENV LD_LIBRARY_PATH /usr/local/lib:${LD_LIBRARY_PATH}

RUN set -ex ;\
    apt-get purge -y --auto-remove ${BUILD_DEPS} ;\
    apt autoremove -y ;\
    apt-get update ;\
    apt-get install -y --no-install-recommends ${RUN_DEPS} ${DEV_DEPS} ;\
    rm -rf /var/lib/apt/lists/* ;\
    ldconfig
