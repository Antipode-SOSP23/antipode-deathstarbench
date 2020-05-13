FROM openresty/openresty:xenial

# Install mongo-c-driver
ARG LIB_MONGOC_VERSION=1.9.3
RUN set -ex ;\
    apt-get update ;\
    apt-get install -y --no-install-recommends wget libssl-dev ;\
    cd /tmp ;\
    wget https://github.com/mongodb/mongo-c-driver/releases/download/${LIB_MONGOC_VERSION}/mongo-c-driver-${LIB_MONGOC_VERSION}.tar.gz ;\
    tar -zxf mongo-c-driver-${LIB_MONGOC_VERSION}.tar.gz ;\
    cd mongo-c-driver-${LIB_MONGOC_VERSION} ;\
    ./configure --disable-automatic-init-and-cleanup ;\
    make -j${RESTY_J} ;\
    make -j${RESTY_J} install ;\
    cd /tmp ;\
    rm -rf mongo-c-driver-${LIB_MONGOC_VERSION}.tar.gz \
           mongo-c-driver-${LIB_MONGOC_VERSION} ;\
    rm -rf /var/lib/apt/lists/* ;\


RUN set -ex ;\
    luarocks install resty-mongol --server=http://rocks.moonscript.org ;\
    luarocks install luasocket ;\
    luarocks install chronos ;\
    luarocks install magick

COPY lualongnumber /tmp/lualongnumber
RUN set -ex ;\
    cd /tmp/lualongnumber ;\
    make ;\
    make install