
#! /bin/bash

export HOST_EU=http://35.232.178.189:8080
export HOST_US=http://35.234.65.72:8082
mkdir -p /tmp/dsb-wkld-data/socialNetwork-gcp-colocated/20210223134705/
docker run --rm -it --network=host -e HOST_EU=http://35.232.178.189:8080 -e HOST_US=http://35.234.65.72:8082 -v /code/socialNetwork/wrk2/scripts:/scripts wrk2:antipode ./wrk --connections 4 --duration 300s --threads 2 --latency --script /scripts/social-network/compose-post.lua http://35.232.178.189:8080/wrk2-api/post/compose --rate 150 | tee /tmp/dsb-wkld-data/socialNetwork-gcp-colocated/20210223134705//$(hostname).out
