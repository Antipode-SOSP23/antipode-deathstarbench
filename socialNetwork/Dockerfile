FROM yg397/thrift-microservice-deps:antipode

ARG NUM_CPUS=12
COPY ./ /social-network-microservices
RUN set -ex ;\
    cd /social-network-microservices ;\
    mkdir -p build ;\
    cd build ;\
    cmake .. ;\
    make -j${NUM_CPUS} ;\
    make install

WORKDIR /social-network-microservices
