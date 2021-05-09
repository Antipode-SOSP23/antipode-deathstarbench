
#! /bin/bash

export HOST_EU=http://34.72.122.12:8080
export HOST_US=http://34.89.253.127:8082
mkdir -p /tmp/dsb-wkld-data/socialNetwork-gcp-colocated/20210509122010/
docker run --rm -it --network=host -e HOST_EU=http://34.72.122.12:8080 -e HOST_US=http://34.89.253.127:8082 -v /code/socialNetwork/wrk2/scripts:/scripts wrk2:antipode ./wrk --connections 2 --duration 300s --threads 1 --latency --script /scripts/social-network/compose-post.lua http://34.72.122.12:8080/wrk2-api/post/compose --rate 150 | tee /tmp/dsb-wkld-data/socialNetwork-gcp-colocated/20210509122010//$(hostname).out
