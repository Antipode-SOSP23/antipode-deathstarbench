#!/bin/bash

# load env vars
set -a
source /tmp/base_vm_env
set +a
echo $NAMESPACE

# install docker from dev script
# https://docs.docker.com/engine/install/debian/#install-using-the-convenience-script
if ! type "docker" > /dev/null; then
  curl -fsSL https://get.docker.com -o get-docker.sh
  sh ./get-docker.sh
  rm get-docker.sh
fi

# install docker-compose
# https://docs.docker.com/compose/install/#install-compose-on-linux-systems
if ! type "docker-compose" > /dev/null; then
  sudo curl -L "https://github.com/docker/compose/releases/download/v2.20.2/docker-compose-$(uname -s)-$(uname -m)" -o /usr/local/bin/docker-compose
  sudo chmod +x /usr/local/bin/docker-compose
fi

# authenticate with gcloud:
sudo gcloud auth activate-service-account --key-file=/tmp/credentials.json
sudo gcloud auth -q configure-docker

# pull public images
sudo docker pull jaegertracing/all-in-one:latest
sudo docker pull jonathanmace/xtrace-server:latest
sudo docker pull memcached:latest
sudo docker pull mongo:4.4.6
sudo docker pull mrvautin/adminmongo:latest
sudo docker pull rabbitmq:3.8
sudo docker pull rabbitmq:3.8-management
sudo docker pull redis:latest
sudo docker pull yg397/media-frontend:xenial
sudo docker pull portainer/agent:latest
sudo docker pull portainer/portainer-ce:latest
sudo docker pull prom/pushgateway:latest
sudo docker pull prom/prometheus:latest
# pull our images
sudo docker pull ${NAMESPACE}/mongodb-delayed:4.4.6
sudo docker pull ${NAMESPACE}/mongodb-setup:4.4.6
sudo docker pull ${NAMESPACE}/rabbitmq-setup:3.8
sudo docker pull ${NAMESPACE}/yg397/openresty-thrift:latest
sudo docker pull ${NAMESPACE}/yg397/social-network-microservices:antipode
sudo docker pull ${NAMESPACE}/redis-im:antipode
sudo docker pull ${NAMESPACE}/wrk2:antipode
sudo docker pull ${NAMESPACE}/python-wkld:antipode

# install some extras
sudo apt-get install -y --no-install-recommends \
    build-essential \
    rsync \
    less \
    vim \
    htop \
    iftop \
    iotop \
    nload \
    tree \
    tmux \
    jq \
    python3-dev \
    python3-pip \
    ansible \
    ;

# add alias for python
sudo ln -s /usr/bin/python3 /usr/bin/python
sudo ln -s /usr/bin/pip3 /usr/bin/pip

# Clean image
sudo apt-get clean
sudo apt-get autoclean

# Install pip packages
sudo pip install wheel
sudo pip install cython
sudo pip install aiohttp
sudo pip install plumbum
sudo pip install requests

# Allow ssh root login
sudo sed -i 's/PermitRootLogin no/PermitRootLogin prohibit-password/g' /etc/ssh/sshd_config
# add key to roots authorized_keys
echo "# Added by Antipode@DeathStarBench" | sudo tee --append /root/.ssh/authorized_keys
cat /tmp/google_compute_engine.pub | sudo tee --append /root/.ssh/authorized_keys
# reload ssh
sudo systemctl reload ssh

# Disable Nagle's Algorithm
sudo sysctl -w net.ipv4.tcp_syncookies=1
sudo sysctl -w net.ipv4.tcp_synack_retries=1
sudo sysctl -w net.ipv4.tcp_syn_retries=1
sudo sysctl -w net.core.somaxconn=4096