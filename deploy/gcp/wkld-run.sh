
#! /bin/bash

export HOST_EU=http://34.68.149.96:8080
export HOST_US=http://35.242.218.165:8082
mkdir -p /tmp/dsb-wkld-data/socialNetwork-gcp-colocated/20201202185757/
docker run --rm -it --network=host -e HOST_EU=http://34.68.149.96:8080 -e HOST_US=http://35.242.218.165:8082 -v /code/socialNetwork/wrk2/scripts:/scripts wrk2:antipode ./wrk --connections 4 --duration 300s --threads 2 --latency --script /scripts/social-network/compose-post.lua http://34.68.149.96:8080/wrk2-api/post/compose --rate 75 | tee /tmp/dsb-wkld-data/socialNetwork-gcp-colocated/20201202185757//$(hostname).out
