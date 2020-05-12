FROM yg397/thrift-microservice-deps:latest

ARG NUM_CPUS=12

# install new version of boost c++ lib so we have access to new functionalities like thread_pool
RUN echo "deb http://ppa.launchpad.net/mhier/libboost-latest/ubuntu xenial main" | tee -a /etc/apt/sources.list \
    && apt remove -y libboost-all-dev \
    && apt-get update \
    && apt-get install -y --no-install-recommends --allow-unauthenticated \
                       libboost1.68

COPY ./ /social-network-microservices
RUN cd /social-network-microservices \
    && mkdir -p build \
    && cd build \
    && find / -type f -name "thread_pool.hpp" \
    && cmake .. \
    && make -j${NUM_CPUS} \
    && make install

WORKDIR /social-network-microservices

#  ---> Running in ecf08081609e
# /usr/local/include/tacopie/utils/thread_pool.hpp
# /usr/include/boost/thread/thread_pool.hpp
# /usr/include/boost/detail/winapi/thread_pool.hpp
# /usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
