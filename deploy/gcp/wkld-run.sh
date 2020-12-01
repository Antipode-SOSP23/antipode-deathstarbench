
#! /bin/bash

export HOST_EU=http://35.184.246.155:8080
export HOST_US=http://35.242.218.165:8082
mkdir -p /tmp/dsb-wkld-data/socialNetwork-gcp-colocated
docker run --rm -it --network=host -e HOST_EU=http://35.184.246.155:8080 -e HOST_US=http://35.242.218.165:8082 -v /code/socialNetwork/wrk2/scripts:/scripts wrk2:antipode ./wrk --connections 6 --duration 120s --threads 4 --latency --script /scripts/social-network/compose-post.lua http://35.184.246.155:8080/wrk2-api/post/compose --rate 400 | tee /tmp/dsb-wkld-data/socialNetwork-gcp-colocated/socialNetwork-gcp-colocated__20201201122607__$(hostname).out
