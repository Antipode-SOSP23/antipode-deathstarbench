# Modifications

This is a modified version of the Cornell DeathStarBench social network microservices.  It has been modified to add X-Trace tracing support.  Please note the modified build / install / run directions.

# Social Network Microservices

A social network with unidirectional follow relationships, implemented with loosely-coupled microservices, communicating with each other via Thrift RPCs. 

## Application Structure

![Social Network Architecture](socialNet_arch.png)

Supported actions: 
* Create text post (optional media: image, video, shortened URL, user tag)
* Read post
* Read entire user timeline
* Receive recommendations on which users to follow
* Search database for user or post
* Register/Login using user credentials
* Follow/Unfollow user

## Pre-requirements
- Docker
- Docker-compose
- Python 3.5+ (with asyncio and aiohttp)
- libssl-dev (apt-get install libssl-dev)
- libz-dev (apt-get install libz-dev)
- luarocks (apt-get install luarocks)
- luasocket (luarocks install luasocket)

## Running the social network application
### Before you start
- Install Docker and Docker Compose.
- Make sure the following ports are available: port `8080` for Nginx frontend, `8081` for media frontend and 
  `16686` for Jaeger, `4080` and `5563` for X-Trace

### Build modified docker containers
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
docker build -t yg397/social-network-microservices .
```


### Start docker containers
Start docker containers by running `docker-compose up -d`. All images will be 
pulled from Docker Hub.

### Register users and construct social graphs
Register users and construct social graph by running 
`python3 scripts/init_social_graph.py`. This will initialize a social graph 
based on [Reed98 Facebook Networks](http://networkrepository.com/socfb-Reed98.php),
with 962 users and 18.8K social graph edges.

### Running HTTP workload generator
#### Make
```bash
cd wrk2
make
```

#### Compose posts
```bash
cd wrk2
./wrk -D exp -t <num-threads> -c <num-conns> -d <duration> -L -s ./scripts/social-network/compose-post.lua http://localhost:8080/wrk2-api/post/compose -R <reqs-per-sec>
```

#### Read home timelines
```bash
cd wrk2
./wrk -D exp -t <num-threads> -c <num-conns> -d <duration> -L -s ./scripts/social-network/read-home-timeline.lua http://localhost:8080/wrk2-api/home-timeline/read -R <reqs-per-sec>
```

#### Read user timelines
```bash
cd wrk2
./wrk -D exp -t <num-threads> -c <num-conns> -d <duration> -L -s ./scripts/social-network/read-user-timeline.lua http://localhost:8080/wrk2-api/user-timeline/read -R <reqs-per-sec>
```

#### View Jaeger traces
View Jaeger traces by accessing `http://localhost:16686`

Example of a Jaeger trace for a compose post request: 

![jaeger_example](socialNet_jaeger.png)

#### View X-Trace traces
View X-Trace traces by accessing `http://localhost:4080`

### Development Status

This application is still actively being developed, so keep an eye on the repo to stay up-to-date with recent changes. 

#### Planned updates

* Front-end design
* Upgraded recommender
* Upgraded search engine 

### Questions and contact

You are welcome to submit a pull request if you find a bug or have extended the application in an interesting way. For any questions please contact us at: <microservices-bench-L@list.cornell.edu>

