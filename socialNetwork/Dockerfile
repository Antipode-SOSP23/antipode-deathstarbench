FROM yg397/thrift-microservice-deps:antipode

ARG NUM_CPUS=12

COPY ./ /social-network-microservices
RUN set -ex ;\
    cd /social-network-microservices ;\
    # if you need to update the gen-cpp, run this in a container
    #   $ docker run --rm -it -v /mnt/nas/inesc/jfloff/antipode/antipode-deathstarbench/socialNetwork/:/sn yg397/thrift-microservice-deps:antipode bash
    # thrift -r --gen cpp social_network.thrift ;\
    mkdir -p build ;\
    cd build ;\
    cmake .. ;\
    make -j${NUM_CPUS} ;\
    make install

WORKDIR /social-network-microservices
