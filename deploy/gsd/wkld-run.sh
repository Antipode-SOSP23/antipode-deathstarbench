
#! /bin/bash

export HOST_EU=http://saturn1:8080
export HOST_US=http://node24:8082
mkdir -p /tmp/dsb-wkld-data/socialNetwork-gsd-split-cluster__20201124141204
docker run --rm -it --network=host -e HOST_EU=http://saturn1:8080 -e HOST_US=http://node24:8082 -v /mnt/nas/inesc/jfloff/antipode/antipode-deathstarbench/socialNetwork/wrk2/scripts:/scripts wrk2:antipode ./wrk --connections 45 --duration 300s --threads 40 --latency --script /scripts/social-network/compose-post.lua http://saturn1:8080/wrk2-api/post/compose --rate 500 | tee /tmp/dsb-wkld-data/socialNetwork-gsd-split-cluster__20201124141204/socialNetwork-gsd-split-cluster__20201124141204__$(hostname).out
