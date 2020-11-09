FROM redis:latest

COPY redis.conf /usr/local/etc/redis/redis.conf
COPY rc.local /etc/rc.local

CMD [ "redis-server", "/usr/local/etc/redis/redis.conf" ]