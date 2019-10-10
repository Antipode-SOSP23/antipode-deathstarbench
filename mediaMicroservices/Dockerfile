FROM yg397/thrift-microservice-deps:latest

ARG NUM_CPUS=12

COPY ./ /media-microservices
RUN cd /media-microservices \
    && mkdir -p build \
    && cd build \
    && cmake .. \
    && make -j${NUM_CPUS} \
    && make install

WORKDIR /media-microservices
