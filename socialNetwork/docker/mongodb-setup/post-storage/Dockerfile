FROM mongo

# create app directory
WORKDIR /usr/src/configs

COPY replicaSet.js .
COPY setup.sh .

RUN chmod 777 /usr/src/configs/setup.sh

CMD ["./setup.sh"]