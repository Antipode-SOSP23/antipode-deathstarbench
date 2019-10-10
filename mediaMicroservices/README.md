# Media Microservices

## Dependencies
- thrift C++ library
- mongo-c-driver
- libmemcached
- nlohmann/json https://nlohmann.github.io/json/

## Pre-requirements
- Docker
- Docker-compose
- Python 3.5+ (with asyncio and aiohttp)
- libssl-dev (apt-get install libssl-dev)
- libz-dev (apt-get install libz-dev)
- luarocks (apt-get install luarocks)
- luasocket (luarocks install luasocket)

## Running the media service application
### Before you start
- Install Docker and Docker Compose.
- Make sure the following ports are available: port `8080` for Nginx frontend and 
  `16686` for Jaeger,`4080` and `5563` for X-Trace.

### Build modified containers

By default, the DeathStarBench pulls its containers from docker hub.  We need to override these with our modified X-Trace containers.  To do this, we will manually build the docker images for the modified components.

1. Build the base docker image that contains all the dependent libraries.  We modified this to add X-Trace and protocol buffers.
```
cd docker/thrift-microservice-deps/cpp
docker build --no-cache -t yg397/thrift-microservice-deps .
cd ../../..
```

2. Build the nginx server image. We modified this to add X-Trace and protocol buffers
```
cd docker/openresty-thrift
docker build --no-cache -t yg397/openresty-thrift -f xenial/Dockerfile .
cd ../../../
```

3. Build the social network docker image
```
docker build -t yg397/media-microservices .
```

### Start docker containers
Start docker containers by running `docker-compose up -d`. All images will be 
pulled from Docker Hub.

### Register users and movie information
```
python3 scripts/write_movie_info.py && scripts//register_users.sh
```

### Running HTTP workload generator
#### Make
```bash
cd wrk2
make
```

#### Compose reviews
```bash
cd wrk2
./wrk -D exp -t <num-threads> -c <num-conns> -d <duration> -L -s ./scripts/media-microservices/compose-review.lua http://localhost:8080/wrk2-api/review/compose -R <reqs-per-sec>
```

#### View Jaeger traces
View Jaeger traces by accessing `http://localhost:16686`
