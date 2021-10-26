FROM golang:1.16

RUN set -ex ;\
    apt-get update ;\
    apt-get install -y --no-install-recommends \
        ca-certificates \
        openssl \
        ;\
    rm -rf /var/lib/apt/lists/*

COPY . /go/src/github.com/harlow/go-micro-services
WORKDIR /go/src/github.com/harlow/go-micro-services

RUN set -ex ;\
    go clean -modcache ;\
    go mod init ;\
    go mod edit -replace github.com/uber-go/atomic=github.com/uber-go/atomic@v1.4.0 ;\
    go mod tidy ;\
    go get gopkg.in/mgo.v2 ;\
    go get github.com/bradfitz/gomemcache/memcache ;\
    go get github.com/google/uuid ;\
    go install -ldflags="-s -w" ./cmd/...
