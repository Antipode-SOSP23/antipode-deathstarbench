# build and install dockerize to the current proper architecture
FROM golang:1.13.15-buster AS dependencies
RUN apt update && apt install -y openssl git
WORKDIR /go/src/github.com/jwilder/dockerize
RUN set -ex; \
    git clone https://github.com/jwilder/dockerize.git . ;\
    go get github.com/robfig/glock ;\
    glock sync -n < GLOCKFILE ;\
    go install


FROM rabbitmq:3.8-management

# create app directory
WORKDIR /usr/src/configs

RUN set -ex ;\
  apt-get update ;\
  apt-get install -y \
      iputils-ping \
      python3-pip \
      sudo \
      ;\
  rm -rf /var/lib/apt/lists/* ;\
  # link useful bins
  ln -s /usr/bin/python3 /usr/bin/python
  # ln -s /usr/bin/pip3 /usr/bin/pip

RUN set -ex ;\
  sudo -H pip3 install pika

# install dockerize
COPY --from=dependencies /go/bin/dockerize /usr/local/bin
COPY setup.sh .

RUN set -ex ;\
  chmod 777 /usr/src/configs/setup.sh

CMD ["./setup.sh"]