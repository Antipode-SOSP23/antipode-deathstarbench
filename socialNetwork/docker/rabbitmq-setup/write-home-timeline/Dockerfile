FROM rabbitmq:3-management

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
  ln -s /usr/bin/python3 /usr/bin/python ;\
  ln -s /usr/bin/pip3 /usr/bin/pip

RUN set -ex ;\
  sudo -H pip3 install pika


COPY setup.sh .

RUN set -ex ;\
  chmod 777 /usr/src/configs/setup.sh

CMD ["./setup.sh"]