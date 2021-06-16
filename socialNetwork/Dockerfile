# build and install dockerize to the current proper architecture
FROM golang:1.13.15-buster AS dependencies
RUN apt update && apt install -y openssl git
WORKDIR /go/src/github.com/jwilder/dockerize
RUN set -ex; \
    git clone https://github.com/jwilder/dockerize.git . ;\
    go get github.com/robfig/glock ;\
    glock sync -n < GLOCKFILE ;\
    go install


FROM yg397/thrift-microservice-deps:antipode

ARG NUM_CPUS=12

# install dockerize
COPY --from=dependencies /go/bin/dockerize /usr/local/bin

COPY ./ /social-network-microservices
RUN set -ex ;\
    cd /social-network-microservices ;\
    mkdir -p build ;\
    cd build ;\
    cmake .. ;\
    make -j${NUM_CPUS} ;\
    make install

WORKDIR /social-network-microservices
