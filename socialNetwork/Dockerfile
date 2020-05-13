FROM yg397/thrift-microservice-deps:latest

ARG NUM_CPUS=12

ARG LD_LIBRARY_PATH="/usr/include:$LD_LIBRARY_PATH"
ARG PATH="/usr/include:$PATH"
ARG CPLUS_INCLUDE_PATH="/usr/include:$CPLUS_INCLUDE_PATH"

# install new version of boost c++ lib so we have access to new functionalities like thread_pool
RUN echo "deb http://ppa.launchpad.net/mhier/libboost-latest/ubuntu xenial main" | tee -a /etc/apt/sources.list \
    && apt remove -y libboost-all-dev \
    && apt autoremove -y \
    && apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 31F54F3E108EAD31 \
    && apt-get update \
    && apt-get install -y --no-install-recommends --allow-unauthenticated \
                       libboost1.68 \
                       libboost1.68-dev

COPY ./ /social-network-microservices
RUN cd /social-network-microservices \
    && mkdir -p build \
    && cd build \
    && cmake .. \
    && make -j${NUM_CPUS} \
    && make install

WORKDIR /social-network-microservices

#  ---> Running in ecf08081609e
# /usr/local/include/tacopie/utils/thread_pool.hpp
# /usr/include/boost/thread/thread_pool.hpp
# /usr/include/boost/detail/winapi/thread_pool.hpp
# /usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
