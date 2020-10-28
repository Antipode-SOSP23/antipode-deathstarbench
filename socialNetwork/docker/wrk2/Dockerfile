FROM debian:10.6

# packages
RUN set -ex ;\
    apt-get update ;\
    apt-get install -y --no-install-recommends \
      ca-certificates \
      libssl-dev \
      libz-dev \
      make \
      g++ \
      lua5.1 \
      lua5.1-dev \
      luarocks \
      unzip \
      git \
      ;\
    rm -rf /var/lib/apt/lists/*

# wkr2 and DSB dependencies
RUN set -ex ;\
    luarocks install luasocket ;\
    luarocks install json-lua ;\
    luarocks install penlight ;\
    wget "http://github.com/giltene/wrk2/archive/master.zip" -O temp.zip ;\
    unzip temp.zip ;\
    mv wrk2-master wrk2 ;\
    rm temp.zip ;\
    cd /wrk2 ;\
    make clean ;\
    make

WORKDIR /wrk2
CMD /wrk2/wrk