
#! /bin/bash

export HOST_EU=http://34.123.3.33:8080
export HOST_US=http://34.107.55.71:8082
mkdir -p /tmp/dsb-wkld-data/socialNetwork-gcp-colocated
docker run --rm -it --network=host -e HOST_EU=http://34.123.3.33:8080 -e HOST_US=http://34.107.55.71:8082 -v /code/socialNetwork/wrk2/scripts:/scripts wrk2:antipode ./wrk --connections 6 --duration 60s --threads 4 --latency --script /scripts/social-network/compose-post.lua http://34.123.3.33:8080/wrk2-api/post/compose --rate 400 | tee /tmp/dsb-wkld-data/socialNetwork-gcp-colocated/socialNetwork-gcp-colocated__20201201224047__$(hostname).out
