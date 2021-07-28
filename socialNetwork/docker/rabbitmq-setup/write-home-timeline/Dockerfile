FROM rabbitmq:3.8-management

RUN set -ex ;\
  apt-get update ;\
  apt-get install -y \
      iputils-ping \
      wget \
      unzip \
      ;\
  rm -rf /var/lib/apt/lists/*

#---------
# Dockerize
#---------
ENV DOCKERIZE_VERSION v0.6.1
RUN set -ex ;\
    wget https://github.com/jwilder/dockerize/releases/download/$DOCKERIZE_VERSION/dockerize-linux-amd64-$DOCKERIZE_VERSION.tar.gz ;\
    tar -C /usr/local/bin -xzvf dockerize-linux-amd64-$DOCKERIZE_VERSION.tar.gz ;\
    rm dockerize-linux-amd64-$DOCKERIZE_VERSION.tar.gz

#---------
# ran HTTP server
#---------
ENV RAN_VERSION v0.1.5
RUN set -ex ;\
    wget https://github.com/m3ng9i/ran/releases/download/$RAN_VERSION/ran_linux_386.zip ;\
    unzip ran_linux_386.zip ;\
    mv ran_linux_386 /usr/local/bin/ran ;\
    rm ran_linux_386.zip

#---------
# Python pika for debug
#---------
# RUN set -ex ;\
#   apt-get update ;\
#   apt-get install -y \
#       sudo \
#       python3-pip \
#       ;\
#   rm -rf /var/lib/apt/lists/* ;\
#   ln -s /usr/bin/python3 /usr/bin/python ;\
#   # ln -s /usr/bin/pip3 /usr/bin/pip
#   sudo -H pip3 install pika

#---------
# Setup scripts
#---------
WORKDIR /usr/src/configs
COPY setup.sh .
RUN set -ex ;\
  chmod 777 /usr/src/configs/setup.sh

CMD ["./setup.sh"]