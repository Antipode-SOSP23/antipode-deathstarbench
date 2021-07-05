FROM mongo

COPY delay.sh /home

# install python for listener
RUN set -ex ;\
    apt-get update ;\
    apt-get install -y --no-install-recommends \
        net-tools \
        iproute2 \
        iputils-ping \
        ;\
    rm -rf /var/lib/apt/lists/* ;\
    chmod 777 /home/delay.sh
