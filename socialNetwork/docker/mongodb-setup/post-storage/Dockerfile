# build and install dockerize to the current proper architecture
FROM golang:1.13.15-buster AS dependencies
RUN apt update && apt install -y openssl git
WORKDIR /go/src/github.com/jwilder/dockerize
RUN set -ex; \
    git clone https://github.com/jwilder/dockerize.git . ;\
    go get github.com/robfig/glock ;\
    glock sync -n < GLOCKFILE ;\
    go install


FROM mongo

# install python for listener
RUN set -ex ;\
    apt-get update ;\
    apt-get install -y --no-install-recommends \
        python3 \
        ;\
    rm -rf /var/lib/apt/lists/*

# create app directory
WORKDIR /usr/src/configs

# install dockerize
COPY --from=dependencies /go/bin/dockerize /usr/local/bin
COPY replicaSet.js .
COPY setup.sh .

RUN chmod 777 /usr/src/configs/setup.sh

CMD ["./setup.sh"]