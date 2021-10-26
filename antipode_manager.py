#!/usr/bin/env python3

import os
from plumbum import local
from plumbum import FG, BG
from plumbum.cmd import python3
from datetime import datetime
import time
import glob
import re
from pprint import pprint as pp
from pathlib import Path
import sys
import stat
import itertools

#-----------------
# Pre-requisites
#
# > sudo apt-get install libssl-dev libz-dev lua5.1 lua5.1-dev luarocks python3
# > sudo luarocks install luasocket
# > sudo luarocks install json-lua
# > sudo luarocks install penlight
# > pip install plumbum ansible
#-----------------

#-----------------
# CONSTANTS
#-----------------
ROOT_PATH = Path(os.path.abspath(os.path.dirname(sys.argv[0])))

# available deploys
AVAILABLE_DEPLOY_TYPES = {
  'local': { 'name': 'Localhost' },
  'gsd': { 'name': 'GSD Cluster' },
  'gcp': { 'name': 'Google Cloud Platform' },
}
# name of the folder where the app root is
AVAILABLE_APPLICATIONS = [
  'socialNetwork',
  'hotelReservation',
]
AVAILABLE_WKLD_ENDPOINTS = {
  'socialNetwork': {
    'compose-post': {
      'type': 'wrk2',
      'uri': 'wrk2-api/post/compose',
      'script_path': './wrk2/scripts/social-network/compose-post.lua',
    },
    'read-home-timeline': {
      'type': 'wrk2',
      'uri': 'wrk2-api/home-timeline/read',
      'script_path': './wrk2/scripts/social-network/read-home-timeline.lua',
    },
    'read-user-timeline': {
      'type': 'wrk2',
      'uri': 'wrk2-api/user-timeline/read',
      'script_path': './wrk2/scripts/social-network/read-user-timeline.lua',
    },
    'sequence-compose-post-read-home-timeline': {
      'type': 'wrk2',
      'uri': 'wrk2-api/home-timeline/read',
      'script_path': './wrk2/scripts/social-network/sequence-compose-post-read-home-timeline.lua',
    },
    'antipode-wht-error': {
      'type': 'python',
      'script_path': './scripts/antipode-wht-error.py',
    },
    'init-social-graph': {
      'type': 'python',
      'script_path': './scripts/init_social_graph.py',
    },
  },
  'hotelReservation': {
    'reserve': {
      'type': 'wrk2',
      'uri': '',
      'script_path': './wrk2/scripts/hotel-reservation/reserve.lua',
    },
  }
}
SOCIAL_NETWORK_DEFAULT_SERVICES = {
  'services': {
    'social-graph-service': 'HOSTNAME',
    'social-graph-mongodb': 'HOSTNAME',
    'social-graph-redis': 'HOSTNAME',
    'write-home-timeline-service-eu': 'HOSTNAME',
    'write-home-timeline-service-us': 'HOSTNAME',
    'write-home-timeline-rabbitmq-eu': 'HOSTNAME',
    'write-home-timeline-rabbitmq-us': 'HOSTNAME',
    'home-timeline-redis-eu': 'HOSTNAME',
    'home-timeline-redis-us': 'HOSTNAME',
    'home-timeline-service': 'HOSTNAME',
    'post-storage-service': 'HOSTNAME',
    'post-storage-memcached': 'HOSTNAME',
    'post-storage-mongodb': 'HOSTNAME',
    'post-storage-service-us': 'HOSTNAME',
    'post-storage-memcached-us': 'HOSTNAME',
    'post-storage-mongodb-us': 'HOSTNAME',
    'user-timeline-service': 'HOSTNAME',
    'user-timeline-redis': 'HOSTNAME',
    'user-timeline-mongodb': 'HOSTNAME',
    'compose-post-redis': 'HOSTNAME',
    'compose-post-service': 'HOSTNAME',
    'url-shorten-service': 'HOSTNAME',
    'url-shorten-memcached': 'HOSTNAME',
    'url-shorten-mongodb': 'HOSTNAME',
    'user-service': 'HOSTNAME',
    'user-memcached': 'HOSTNAME',
    'user-mongodb': 'HOSTNAME',
    'media-service': 'HOSTNAME',
    'media-memcached': 'HOSTNAME',
    'media-mongodb': 'HOSTNAME',
    'media-frontend': 'HOSTNAME',
    'text-service': 'HOSTNAME',
    'unique-id-service': 'HOSTNAME',
    'user-mention-service': 'HOSTNAME',
    'antipode-oracle': 'HOSTNAME',
    'nginx-thrift': 'HOSTNAME',
    'nginx-thrift-us': 'HOSTNAME',
    'jaeger': 'HOSTNAME',
    'xtrace-server': 'HOSTNAME',
    'mongodb-admin': 'HOSTNAME',
    'post-storage-mongodb-setup': 'HOSTNAME',
    'write-home-timeline-rabbitmq-setup': 'HOSTNAME',
  },
  'nodes': {
    'HOSTNAME': {
      'hostname': 'HOSTNAME',
      'zone': 'europe-west3-c'
    }
  }
}
CONTAINERS_BUILT = [
  'mongodb-delayed',
  'mongodb-setup',
  'rabbitmq-setup',
  'yg397/openresty-thrift:latest',
  'yg397/social-network-microservices:antipode',
  'wrk2:antipode',
  'redis-im:antipode',
]
# GSD Constants
GSD_AVAILABLE_NODES = {
  'node01': '10.100.0.11',
  'node02': '10.100.0.12',
  'node03': '10.100.0.13',
  'node04': '10.100.0.14',
  'node05': '10.100.0.15',
  'node06': '10.100.0.16',
  'node07': '10.100.0.17',
  'node08': '10.100.0.18',
  'node09': '10.100.0.19',
  'node10': '10.100.0.20',
  'node11': '10.100.0.21',
  'node20': '10.100.0.30',
  'node21': '10.100.0.31',
  'node22': '10.100.0.32',
  'node23': '10.100.0.33',
  'node24': '10.100.0.34',
  'node25': '10.100.0.35',
  'node26': '10.100.0.36',
  'node27': '10.100.0.37',
  'node28': '10.100.0.38',
  'node29': '10.100.0.39',
  'node30': '10.100.0.40',
  'node31': '10.100.0.41',
  'node32': '10.100.0.42',
  'node33': '10.100.0.43',
  'node34': '10.100.0.44',
  'node35': '10.100.0.45',
  'angainor': '146.193.41.56',
  'cosmos': '146.193.41.55',
  'ngstorage': '146.193.41.54',
  'intel14v1': '146.193.41.45',
  'intel14v2': '146.193.41.51',
  'nvram': '146.193.41.52',
  'saturn1': '146.193.41.71',
  'saturn2': '146.193.41.77',
}
GSD_SWARM_MANAGER_NODE = { 'node34': '10.100.0.44' }

# GCP Constants
GCP_DOCKER_IMAGE_NAME = 'gcp-manager:antipode'
GCP_CREDENTIALS_FILE = ROOT_PATH / 'deploy' / 'gcp' / 'pluribus.json'
# GCP_PROJECT_ID = 'antipode-296620'
GCP_PROJECT_ID = 'pluribus'
GCP_MACHINE_IMAGE_LINK = f"https://www.googleapis.com/compute/v1/projects/{GCP_PROJECT_ID}/global/images/antipode"

# GATHER variables
PERCENTILES_TO_PRINT = [.25, .5, .75, .90, .99]


#-----------------
# HELPER
#-----------------
def _index_containing_substring(the_list, substring):
  for i, s in enumerate(the_list):
    if substring in s:
      return i
  return -1

def _deploy_type(args):
  for k in AVAILABLE_DEPLOY_TYPES.keys():
    if args[k]: return k
  return None

def _last_configuration(app, deploy_type):
  conf_base_path = ROOT_PATH / 'deploy' / 'configurations'
  pattern = conf_base_path / f"{app}-{deploy_type}-*.yml"
  files = glob.glob(str(pattern))

  # not files found
  if not files:
    print("[ERROR] No file found!")
    exit(-1)

  # find most recent one
  files.sort(key=os.path.getctime)
  return files[-1]

def _remove_prefix(text, prefix):
  if text.startswith(prefix):
    return text[len(prefix):]
  return text

def _is_inside_docker():
  return os.path.isfile('/.dockerenv')

def _force_docker():
  if not _is_inside_docker():
    import platform
    import subprocess
    from plumbum.cmd import docker

    # if image is not built, we do it
    if docker['images', 'gcp-manager:antipode', '--format', '"{{.ID}}"']().strip() == '':
      os.chdir(ROOT_PATH / 'deploy' / 'gcp')
      docker['build', '-t', 'gcp-manager:antipode', '.'] & FG

    args = list()
    args.extend(['docker', 'run', '--rm', '-it',
      # env variables
      '-e', f"HOST_ROOT_PATH={ROOT_PATH}",
      # run docker from host inside the container
      '-v', '/var/run/docker.sock:/var/run/docker.sock',
      '-v', '/usr/bin/docker:/usr/bin/docker',
      '-v', '/usr/bin/docker-compose:/usr/bin/docker-compose',
      # mount code volumes
      '-v', f"{ROOT_PATH / 'deploy'}:/code/deploy",
      '-v', f"{ROOT_PATH / 'antipode_manager.py'}:/code/antipode_manager.py",
      '-v', f"{ROOT_PATH / 'socialNetwork'}:/code/socialNetwork",
      '-w', '/code',
      GCP_DOCKER_IMAGE_NAME
    ])
    args = args + sys.argv
    # print(' '.join(args))
    subprocess.call(args)
    exit()

def _gcp_create_instance(zone, name, machine_type, hostname, firewall_tags):
  import googleapiclient.discovery

  compute = googleapiclient.discovery.build('compute', 'v1')
  config = {
    'name': name,
    'machineType': f"zones/{zone}/machineTypes/{machine_type}",
    'disks': [
      {
        'boot': True,
        'autoDelete': True,
        'initializeParams': {
          'sourceImage': GCP_MACHINE_IMAGE_LINK,
        }
      }
    ],
    'hostname': hostname,
    # Specify a network interface with NAT to access the public internet.
    'networkInterfaces': [
      {
        'network': 'global/networks/default',
        'accessConfigs': [
          {'type': 'ONE_TO_ONE_NAT', 'name': 'External NAT'}
        ]
      }
    ],
    # tags for firewall rules
    "tags": {
      "items": firewall_tags,
    },
  }
  try:
    ret = compute.instances().insert(
        project=GCP_PROJECT_ID,
        zone=zone,
        body=config
      ).execute()
    return ret['status'] == 'RUNNING'
  except googleapiclient.errors.HttpError as e:
    pp(e)

def _gcp_delete_instance(zone, name):
  import googleapiclient.discovery
  compute = googleapiclient.discovery.build('compute', 'v1')

  try:
    ret = compute.instances().delete(
        project=GCP_PROJECT_ID,
        zone=zone,
        instance=name
      ).execute()
    return ret['status'] == 'RUNNING'
  except googleapiclient.errors.HttpError as e:
    pp(e)

def _gcp_get_instance(zone, name):
  import googleapiclient.discovery
  compute = googleapiclient.discovery.build('compute', 'v1')

  return compute.instances().get(project=GCP_PROJECT_ID, zone=zone, instance=name).execute()

def _inventory_to_dict(filepath):
  from ansible.parsing.dataloader import DataLoader
  from ansible.inventory.manager import InventoryManager
  from ansible.vars.manager import VariableManager

  loader = DataLoader()
  inventory = InventoryManager(loader=loader, sources=str(filepath))
  variable_manager = VariableManager(loader=loader, inventory=inventory)

  # other methods:
  # - inventory.get_host('manager.antipode-296620').vars

  loaded_inventory = {}
  for hostname,info in inventory.hosts.items():
    loaded_inventory[info.vars['gcp_name']] = {
      'external_ip': info.vars['ansible_host'],
      'internal_ip': info.vars['gcp_host'],
      'zone': info.vars['gcp_zone'],
      'hostname': hostname,
    }

  return loaded_inventory

def _wait_url_up(url):
  import urllib.request
  while urllib.request.urlopen("http://www.stackoverflow.com").getcode() != 200:
    True

#-----------------
# BUILD
#-----------------
def build(args):
  try:
    getattr(sys.modules[__name__], f"build__{args['app']}__{_deploy_type(args)}")(args)
  except KeyboardInterrupt:
    # if the compose gets interrupted we just continue with the script
    pass

def build__socialNetwork__local(args):
  from plumbum.cmd import docker

  app_dir = ROOT_PATH / args['app']
  os.chdir(app_dir)

  # By default, the DeathStarBench pulls its containers from docker hub.
  # We need to override these with our modified X-Trace containers.
  # To do this, we will manually build the docker images for the modified components.

  thrift_microservice_args = ['build', '-t', 'yg397/thrift-microservice-deps:antipode', '.']
  openresty_thrift_args = ['build', '-t', 'yg397/openresty-thrift', '-f', 'xenial/Dockerfile', '.']

  # adds --no-cache option to build so it rebuilds everything
  if args['strong']:
    thrift_microservice_args.insert(1, '--no-cache')
    openresty_thrift_args.insert(1, '--no-cache')

  # Build the base docker image that contains all the dependent libraries.  We modified this to add X-Trace and protocol buffers.
  os.chdir(app_dir.joinpath('docker', 'thrift-microservice-deps', 'cpp'))
  docker[thrift_microservice_args] & FG

  # Build the nginx server image. We modified this to add X-Trace and protocol buffers
  os.chdir(app_dir.joinpath('docker', 'openresty-thrift'))
  docker[openresty_thrift_args] & FG

  # Build the mongodb-delayed setup image
  os.chdir(app_dir.joinpath('docker', 'mongodb-delayed'))
  docker['build', '-t', 'mongodb-delayed', '.'] & FG

  # Build the mongodb setup image
  os.chdir(app_dir.joinpath('docker', 'mongodb-setup', 'post-storage'))
  docker['build', '-t', 'mongodb-setup', '.'] & FG

  # Build the rabbitmq setup image
  os.chdir(app_dir.joinpath('docker', 'rabbitmq-setup', 'write-home-timeline'))
  docker['build', '-t', 'rabbitmq-setup', '.'] & FG

  # Build the wrk2 image
  os.chdir(app_dir.joinpath('docker', 'wrk2'))
  docker['build', '-t', 'wrk2:antipode', '.'] & FG

  # Build the redis-im image
  os.chdir(app_dir.joinpath('docker', 'redis-im'))
  docker['build', '-t', 'redis-im:antipode', '.'] & FG

  # Build the social network docker image
  os.chdir(app_dir)
  docker['build', '-t', 'yg397/social-network-microservices:antipode', '.'] & FG

def build__hotelReservation__local(args):
  from plumbum.cmd import docker

  app_dir = ROOT_PATH / args['app']
  os.chdir(app_dir)

def build__socialNetwork__gsd(args):
  from plumbum.cmd import ansible_playbook

  # change path to playbooks folder
  os.chdir(ROOT_PATH / 'deploy' / 'gsd')

  ansible_playbook['containers-build.yml', '-e', 'app=socialNetwork'] & FG
  ansible_playbook['containers-backup.yml', '-e', 'app=socialNetwork'] & FG

def build__socialNetwork__gcp(args):
  from plumbum.cmd import docker

  # part of GCP build process is to also build `antipode` image
  # which is a Debian based image with a preset of packages and docker pulls done
  # here is the set of instructions to perform that
  #
  # 1) Start a simple VM and SSH into it using the web interface
  # 2) Install Docker (https://docs.docker.com/engine/install/debian/):
  #     - sudo apt-get update
  #     - sudo apt-get install -y apt-transport-https ca-certificates curl gnupg-agent software-properties-common
  #     - curl -fsSL https://download.docker.com/linux/debian/gpg | sudo apt-key add -
  #     - sudo apt-key fingerprint 0EBFCD88
  #     - sudo add-apt-repository "deb [arch=amd64] https://download.docker.com/linux/debian $(lsb_release -cs) stable"
  #     - sudo apt-get update
  #     - sudo apt-get install -y docker-ce docker-ce-cli containerd.io
  # 3) Install Docker Compose
  #     - sudo curl -L "https://github.com/docker/compose/releases/download/1.27.4/docker-compose-$(uname -s)-$(uname -m)" -o /usr/local/bin/docker-compose
  #     - sudo chmod +x /usr/local/bin/docker-compose
  # 4) Install extras
  #     - sudo apt-get install -y rsync less vim htop jq python3-pip
  # 5) Pull public docker images:
  #     - docker pull jaegertracing/all-in-one:latest
  #     - docker pull jonathanmace/xtrace-server:latest
  #     - docker pull memcached:latest
  #     - docker pull mongo:latest
  #     - docker pull mrvautin/adminmongo:latest
  #     - docker pull rabbitmq:3.8
  #     - docker pull rabbitmq:3.8-management
  #     - docker pull redis:latest
  #     - docker pull portainer/agent:latest
  #     - docker pull portainer/portainer-ce:latest
  #     - docker pull prom/pushgateway:latest
  #     - docker pull prom/prometheus:latest
  #     - docker pull yg397/media-frontend:xenial
  # 6) Add alias for python
  #     - sudo ln -s /usr/bin/python3 /usr/bin/python
  #     - sudo ln -s /usr/bin/pip3 /usr/bin/pip
  # 7) Clean cache
  #     - sudo apt-get clean
  #     - sudo apt-get autoclean
  # 8) Create a copy of the JSON credentials file
  #     - sudo vim /<PROJECT_ID>.json
  #     - copy paste the local content
  #     - authenticate with gcloud:
  #       sudo gcloud auth activate-service-account --key-file=/pluribus.json
  #       sudo gcloud auth configure-docker
  # 9) Close the ssh session and start a local GCP container
  #     - Start container with:
  #     - In the GCP console, in the SSH context menu, copy the gcloud command, it will look something like:
  #       gcloud beta compute ssh --zone "europe-west3-c" "antipode-dev" --project "pluribus"
  #     - Just press enter on the steps until keys are generated and you are logged in
  #     - Copy the files from the container to outside
  #       docker cp <CONTAINER ID>:/root/.ssh/google_compute_engine .
  #       docker cp <CONTAINER ID>:/root/.ssh/google_compute_engine.pub .
  #     - Rename file to: <GCP_PROJECT_ID>_google_compute_engine*
  # 10) Create firewall rules for the following tags:
  #     - 'portainer'
  #         IP Addresses: 0.0.0.0/0
  #         TCP ports: 9000,8000,9090,9091,9100
  #     - 'swarm':
  #         IP Addresses: 0.0.0.0/0
  #         TCP ports: 2376, 2377, 7946
  #         UDP ports: 7946, 4789
  #     - 'nodes':
  #         IP Addresses: 0.0.0.0/0
  #         TCP ports: 9001,16686,8080,8081,8082,1234,4080,5563,15672,5672,5778,14268,14250,9411,9100
  #         UDP ports: 5775,6831,6832
  #

  # locally build the images needed
  if not _is_inside_docker(): build__socialNetwork__local(args)

  # now we go into docker container and push everything to GCP
  _force_docker()
  for tag in CONTAINERS_BUILT:
    gcp_tag = f"gcr.io/{GCP_PROJECT_ID}/{tag}"
    docker['tag', tag, gcp_tag] & FG
    docker['push', gcp_tag] & FG
    docker['rmi', gcp_tag] & FG


#-----------------
# DEPLOY
#-----------------
def deploy(args):
  try:
    getattr(sys.modules[__name__], f"deploy__{args['app']}__{_deploy_type(args)}")(args)
  except KeyboardInterrupt:
    # if the compose gets interrupted we just continue with the script
    pass

def deploy__socialNetwork__local(args):
  return None

def deploy__socialNetwork__gsd(args):
  from plumbum.cmd import ansible_playbook
  from jinja2 import Environment
  import click
  import yaml
  import textwrap
  from shutil import copyfile

  app_dir = ROOT_PATH / args['app']
  os.chdir(ROOT_PATH / 'deploy')

  # base path of configuration file
  conf_base_path = Path(Path.cwd(), 'configurations')
  if args['new']:
    new_filename = f"socialNetwork-gsd-{time.strftime('%Y%m%d%H%M%S')}.yml"
    filepath = conf_base_path / new_filename

    # write default configuration to file
    with open(filepath, 'w') as f_conf:
      yaml.dump(SOCIAL_NETWORK_DEFAULT_SERVICES, f_conf)
      print(f"[SAVED] '{filepath}'")
      # wait for editor to close
      print("[INFO] Waiting for editor to close new configuration file ...")
      click.edit(filename=filepath)

    # check if all nodes are known
    with open(filepath, 'r') as f_conf:
      conf = yaml.load(f_conf, Loader=yaml.FullLoader)
      unknonwn_nodes = [ n for n in set(conf['services'].values()) if n not in GSD_AVAILABLE_NODES.keys() ]
      if unknonwn_nodes:
        print("[ERROR] Found unknown nodes in GSD cluster: " + ', '.join(unknonwn_nodes))
        exit()
  if args['latest']:
    filepath = args['configuration_path']
  if args['file']:
    filepath = args['configuration_path']

  # Update docker-compose.yml
  deploy_nodes = {}
  with open(filepath, 'r') as f_conf, open(Path(app_dir, 'docker-compose.yml'), 'r') as f_compose:
    conf = yaml.load(f_conf, Loader=yaml.FullLoader)
    compose = yaml.load(f_compose, Loader=yaml.FullLoader)

    # add network that is created on deploy playbook
    compose['networks'] = {
      'deathstarbench_network': {
        'external': {
          'name': 'deathstarbench_network'
        }
      }
    }

    # get all the nodes that will be used in deploy
    deploy_nodes = { n:GSD_AVAILABLE_NODES[n] for n in set(conf['services'].values()) }

    for sid,hostname in conf['services'].items():
      # replaces existing networks with new one
      compose['services'][sid]['networks'] = [ 'deathstarbench_network' ]

      # get all the constraints
      deploy_constraints = compose['services'][sid]['deploy']['placement']['constraints']
      # get he id of constraint of the node hostname
      node_constraint_index = _index_containing_substring(deploy_constraints, 'node.hostname')
      # replace docker-compose with that constraint
      deploy_constraints[node_constraint_index] = f'node.hostname == {hostname}'

  # create new docker compose
  new_compose_filepath = ROOT_PATH / 'deploy' / 'gsd' / 'docker-compose-swarm.yml'
  with open(new_compose_filepath, 'w') as f_compose:
    yaml.dump(compose, f_compose)
  print(f"[SAVED] '{new_compose_filepath}'")

  # copy file to socialNetwork folder
  end_docker_compose_swarm = Path(app_dir, 'docker-compose-swarm.yml').resolve()
  copyfile(new_compose_filepath.resolve(), end_docker_compose_swarm)
  print(f"[INFO] Copied '{str(new_compose_filepath.resolve()).split('antipode-deathstarbench/')[1]}' to '{str(end_docker_compose_swarm).split('antipode-deathstarbench/')[1]}'")

  template = """
    [swarm_manager]
    {% for k,v in swarm_manager.items() %}{{k}} ansible_host={{v}} ansible_user=jfloff ansible_ssh_private_key_file=~/.ssh/id_rsa_inesc_cluster_jfloff
    {% endfor %}

    [cluster]
    {% for k,v in deploy_nodes.items() %}{{k}} ansible_host={{v}} ansible_user=jfloff ansible_ssh_private_key_file=~/.ssh/id_rsa_inesc_cluster_jfloff
    {% endfor %}
  """
  inventory = Environment().from_string(template).render({
    'swarm_manager': GSD_SWARM_MANAGER_NODE,
    'deploy_nodes': dict(sorted(deploy_nodes.items())),
  })

  inventory_filepath = Path('gsd/inventory.cfg')
  with open(inventory_filepath, 'w') as f:
    # remove empty lines and dedent for easier read
    f.write(textwrap.dedent(inventory))

  print(f"[SAVED] '{inventory_filepath.resolve()}'")

  # run playbooks
  os.chdir(ROOT_PATH / 'deploy' / 'gsd')
  # first restore all images to everyone
  ansible_playbook['containers-restore.yml', '-e', 'app=socialNetwork'] & FG
  # then deploy everywhere
  ansible_playbook['deploy-swarm.yml', '-e', 'app=socialNetwork'] & FG

  print("[INFO] Deploy Complete!")

def deploy__socialNetwork__gcp(args):
  _force_docker()
  import click
  from jinja2 import Environment
  import yaml
  import textwrap
  from plumbum.cmd import ansible_playbook


  app_dir = ROOT_PATH / args['app']
  os.chdir(ROOT_PATH / 'deploy')

  # base path of configuration file
  conf_base_path = ROOT_PATH / 'deploy' / 'configurations'
  if args['new']:
    filepath = conf_base_path / f"socialNetwork-gcp-{time.strftime('%Y%m%d%H%M%S')}.yml"

    # write default configuration to file
    with open(filepath, 'w') as f_conf:
      yaml.dump(SOCIAL_NETWORK_DEFAULT_SERVICES, f_conf)
      print(f"[SAVED] '{filepath}'")
      print("[INFO] Waiting for editor to close new configuration file ...")
      click.edit(filename=filepath)

    print("[INFO] Appending GCP Project ID to node ids ...")
    # open file and check if GCP_PROJECT_ID is appended to every node id
    with open(filepath, 'r') as f_conf:
      written_conf = yaml.load(f_conf, Loader=yaml.FullLoader)
      for k,v in written_conf['nodes'].items():
        # defaults hostname to id
        if 'hostname' not in v:
          v['hostname'] = k
        # append the project id to hostname
        if GCP_PROJECT_ID not in v['hostname']:
          written_conf['nodes'][k]['hostname'] = f"{v['hostname']}.{GCP_PROJECT_ID}"

    with open(filepath, 'w') as f_conf:
      yaml.dump(written_conf, f_conf)
  if args['latest']:
    filepath = args['configuration_path']
  if args['file']:
    filepath = args['configuration_path']


  with open(filepath, 'r') as f_conf:
    conf = yaml.load(f_conf, Loader=yaml.FullLoader)

    # replace hostname in docker_compose_swarm
    with open(app_dir / 'docker-compose.yml', 'r') as f_app_compose, open(app_dir / 'docker-compose-swarm.yml', 'w') as f_swarm_compose:
      app_compose = yaml.load(f_app_compose, Loader=yaml.FullLoader)

      # add network that is created on deploy playbook
      app_compose['networks'] = {
        'deathstarbench_network': {
          'external': {
            'name': 'deathstarbench_network'
          }
        }
      }

      for service, node_id in conf['services'].items():
        # replaces existing networks with new one
        app_compose['services'][service]['networks'] = [ 'deathstarbench_network' ]

        # replace the deploy constraints
        deploy_constraints = app_compose['services'][service]['deploy']['placement']['constraints']
        # get the id of constraint of the node hostname
        node_constraint_index = _index_containing_substring(deploy_constraints, 'node.hostname')
        # replace docker-compose with that constraint
        deploy_constraints[node_constraint_index] = f"node.hostname == {node_id}"

      # now write the compose into a new file
      yaml.dump(app_compose, f_swarm_compose)
      print(f"[SAVED] '{app_dir / 'docker-compose-swarm.yml'}'")

    # Build inventory for ansible playbooks
    inventory = {}
    # create the swarm manager node
    inventory['manager'] = {
      'hostname': f"manager.{GCP_PROJECT_ID}",
      'zone': 'us-central1-a',
      'external_ip': None,
      'internal_ip': None,
    }
    _gcp_create_instance(
      name='manager',
      machine_type='e2-standard-2',
      zone=inventory['manager']['zone'],
      hostname=inventory['manager']['hostname'],
      firewall_tags=['portainer','swarm']
    )

    # get all the unique node ids to create in GCP
    for node_key, node_info in conf['nodes'].items():
      # skip client nodes
      if node_key in conf['clients']:
        continue

      inventory[node_key] = {
        'hostname': node_info['hostname'],
        'zone': node_info['zone'],
        'external_ip': None,
        'internal_ip': None,
      }
      _gcp_create_instance(
        zone=node_info['zone'],
        name=node_key,
        machine_type=node_info['machine_type'],
        hostname=node_info['hostname'],
        firewall_tags=['swarm','nodes']
      )

    # wait for instances public ips
    for node_key, node_info in inventory.items():
      while inventory[node_key].get('external_ip', None) is None:
        metadata = _gcp_get_instance(node_info['zone'], node_key)
        try:
          inventory[node_key]['external_ip'] = metadata['networkInterfaces'][0]['accessConfigs'][0]['natIP']
          inventory[node_key]['internal_ip'] = metadata['networkInterfaces'][0]['networkIP']
        except KeyError:
          inventory[node_key]['external_ip'] = None
          inventory[node_key]['internal_ip'] = None

    # now build the inventory
    # if you want to get a new google_compute_engine key:
    #   1) go to https://console.cloud.google.com/compute/instances
    #   2) choose one instance, click on SSH triangle for more options
    #   3) 'View gcloud command'
    #   4) Run that command and then copy the generated key
    #
    template = """
      [swarm_manager]
      {{ swarm_manager['hostname'] }} ansible_host={{ swarm_manager['external_ip'] }} gcp_zone={{ swarm_manager['zone'] }} gcp_name=manager gcp_host={{ swarm_manager['internal_ip'] }} ansible_user=root ansible_ssh_private_key_file=/code/deploy/gcp/{{ project_id }}_google_compute_engine


      [cluster]
      {% for node_name,node in deploy_nodes.items() %}
      {{ node['hostname'] }} ansible_host={{ node['external_ip'] }} gcp_zone={{ node['zone'] }} gcp_name={{ node_name }} gcp_host={{ node['internal_ip'] }} ansible_user=root ansible_ssh_private_key_file=/code/deploy/gcp/{{ project_id }}_google_compute_engine
      {% endfor %}
    """
    inventory = Environment().from_string(template).render({
      'project_id': GCP_PROJECT_ID,
      'swarm_manager': inventory['manager'],
      'deploy_nodes': { node_name: inventory[node_name] for node_name in inventory if node_name != 'manager' },
    })

    inventory_filepath = Path('gcp/inventory.cfg')
    with open(inventory_filepath, 'w') as f:
      # remove empty lines and dedent for easier read
      f.write(textwrap.dedent(inventory))

    print(f"[SAVED] '{inventory_filepath}'")

  # sleep before
  time.sleep(30)

  # run playbooks
  os.chdir(ROOT_PATH / 'deploy' / 'gcp')

  # copy all the needed files to remote
  ansible_playbook['deploy-setup.yml', '-e', 'app=socialNetwork', '-e', f"configuration_filepath={str(filepath)}"] & FG
  # then deploy everywhere
  ansible_playbook['deploy-swarm.yml', '-e', 'app=socialNetwork'] & FG


#-----------------
# RUN
#-----------------
def run(args):
  try:
    getattr(sys.modules[__name__], f"run__{args['app']}__{_deploy_type(args)}")(args)
  except KeyboardInterrupt:
    # if the compose gets interrupted we just continue with the script
    pass

def run__socialNetwork__local(args):
  from plumbum import FG, BG
  from plumbum.cmd import docker_compose, docker
  import yaml

  if args['info']:
    from plumbum.cmd import hostname

    public_ip = hostname['-I']().split()[1]
    print(f"Jaeger:\thttp://{public_ip}:16686")
    print(f"RabbitMQ-EU:\thttp://{public_ip}:15672")
    print(f"RabbitMQ-US:\thttp://{public_ip}:15673")
    print("\tuser: admin / pwd: admin")
    return

  run_args = ['up']
  # run containers in detached mode
  if args['detached']:
    run_args.insert(1, '-d')

  # copy docker-compose to the deploy file
  with open(ROOT_PATH / args['app'] / 'docker-compose.yml', 'r') as f_compose:
    compose = yaml.load(f_compose, Loader=yaml.FullLoader)
    # update file with dynamic run flags
    # TODO CHANGE ANTIPODE FLAG
    for _,e in compose['services'].items():
      if ('environment' in e) and ('ANTIPODE' in e['environment']):
        e['environment']['ANTIPODE'] = int(args['antipode']) # 0 - False, 1 - True

  # create deployable docker compose
  new_compose_filepath = ROOT_PATH / 'deploy' / 'local' / 'docker-compose.yml'
  with open(new_compose_filepath, 'w') as f_compose:
    yaml.dump(compose, f_compose)
  print(f"[SAVED] '{new_compose_filepath}'")

  env_args = [
    '--project-directory', str(ROOT_PATH / args['app']),
    '--file', str(new_compose_filepath),
  ]

  # Fixes error: "WARNING: Connection pool is full, discarding connection: localhost"
  # ref: https://github.com/docker/compose/issues/6638#issuecomment-576743595
  with local.env(COMPOSE_PARALLEL_LIMIT=99):
    os.chdir(ROOT_PATH / args['app'])
    docker_compose[env_args + run_args] & FG

def run__hotelReservation__local(args):
  from plumbum import FG, BG
  from plumbum.cmd import docker_compose, docker
  import yaml

  if args['info']:
    from plumbum.cmd import hostname

    public_ip = hostname['-I']().split()[1]
    print(f"Jaeger:\thttp://{public_ip}:16686")
    print("\tuser: admin / pwd: admin")
    return

  run_args = ['up']
  # run containers in detached mode
  if args['detached']:
    run_args.insert(1, '-d')

  # copy docker-compose to the deploy file
  with open(ROOT_PATH / args['app'] / 'docker-compose.yml', 'r') as f_compose:
    compose = yaml.load(f_compose, Loader=yaml.FullLoader)
    # update file with dynamic run flags
    # TODO CHANGE ANTIPODE FLAG
    for _,e in compose['services'].items():
      if ('environment' in e) and ('ANTIPODE' in e['environment']):
        e['environment']['ANTIPODE'] = int(args['antipode']) # 0 - False, 1 - True

  # create deployable docker compose
  new_compose_filepath = ROOT_PATH / 'deploy' / 'local' / 'docker-compose.yml'
  with open(new_compose_filepath, 'w') as f_compose:
    yaml.dump(compose, f_compose)
  print(f"[SAVED] '{new_compose_filepath}'")

  env_args = [
    '--project-directory', str(ROOT_PATH / args['app']),
    '--file', str(new_compose_filepath),
  ]

  # Fixes error: "WARNING: Connection pool is full, discarding connection: localhost"
  # ref: https://github.com/docker/compose/issues/6638#issuecomment-576743595
  with local.env(COMPOSE_PARALLEL_LIMIT=99):
    os.chdir(ROOT_PATH / args['app'])
    docker_compose[env_args + run_args] & FG

def run__socialNetwork__gsd(args):
  from plumbum.cmd import ansible_playbook

  filepath = args['configuration_path']
  if filepath is None:
    print('[ERROR] Deploy file is required')
    exit(-1)

  # change path to playbooks folder
  os.chdir(ROOT_PATH / 'deploy' / 'gsd')

  ansible_playbook['start-portainer.yml'] & FG
  portainer_ip = GSD_AVAILABLE_NODES[next(iter(GSD_SWARM_MANAGER_NODE))]
  print(f"[INFO] Portainer link (u/pwd: admin/antipode): http://{portainer_ip}:9000 ")

  ansible_playbook['start-dsb.yml', '-e', 'app=socialNetwork'] & FG

  # ansible_playbook['init-social-graph.yml', '-e', 'app=socialNetwork', '-e', f"configuration={filepath}"] & FG

  print("[INFO] Run Complete!")

def run__socialNetwork__gcp(args):
  _force_docker()
  from plumbum.cmd import ansible_playbook
  import yaml

  filepath = args['configuration_path']
  if filepath is None:
    print('[ERROR] Deploy file is required')
    exit(-1)

  if args['info']:
    with open(filepath, 'r') as f_conf:
      conf = yaml.load(f_conf, Loader=yaml.FullLoader)
      inventory = _inventory_to_dict(ROOT_PATH / 'deploy' / 'gcp' / 'inventory.cfg')

      jaeger_public_ip = inventory[conf['services']['jaeger']]['external_ip']
      rabbitmq_eu_public_ip = inventory[conf['services']['write-home-timeline-rabbitmq-eu']]['external_ip']
      rabbitmq_us_public_ip = inventory[conf['services']['write-home-timeline-rabbitmq-us']]['external_ip']
      portainer_public_ip = inventory['manager']['external_ip']
      prometheus_public_ip = inventory['manager']['external_ip']

      print(f"Jaeger:    http://{jaeger_public_ip}:16686")
      print(f"RabbitMQ-EU:  http://{rabbitmq_eu_public_ip}:15672")
      print(f"RabbitMQ-US:  http://{rabbitmq_us_public_ip}:15672")
      print("\tuser: admin / pwd: admin")
      print(f"Portainer: http://{portainer_public_ip}:9000")
      print("\tuser: admin / pwd: antipode")
      print(f"Prometheus: http://{prometheus_public_ip}:9090/graph?g0.expr=100%20-%20(avg%20by%20(instance)%20(irate(node_cpu_seconds_total%7Bjob%3D%22nodeexporter%22%2Cmode%3D%22idle%22%7D%5B5m%5D))%20*%20100)&g0.tab=0&g0.stacked=0&g0.range_input=10m&g0.step_input=1&g1.expr=(node_memory_MemTotal_bytes%20-%20node_memory_MemFree_bytes)%2Fnode_memory_MemTotal_bytes%20*100&g1.tab=0&g1.stacked=0&g1.range_input=10m&g1.step_input=1&g2.expr=(node_filesystem_size_bytes%7Bmountpoint%3D%22%2F%22%7D%20-%20node_filesystem_free_bytes%7Bmountpoint%3D%22%2F%22%7D)%2Fnode_filesystem_size_bytes%7Bmountpoint%3D%22%2F%22%7D%20*100&g2.tab=0&g2.stacked=0&g2.range_input=10m&g2.step_input=1&g3.expr=rate(node_network_transmit_bytes_total%7Bdevice%3D%22ens4%22%7D%5B10s%5D)*8%2F1024%2F1024&g3.tab=0&g3.stacked=0&g3.range_input=10m&g3.step_input=1")
    return

  # change path to playbooks folder
  os.chdir(ROOT_PATH / 'deploy' / 'gcp')
  inventory = _inventory_to_dict(ROOT_PATH / 'deploy' / 'gcp' / 'inventory.cfg')

  ansible_playbook['start-portainer.yml'] & FG
  portainer_url = f"http://{inventory['manager']['external_ip']}:9000"
  _wait_url_up(portainer_url)
  print(f"[INFO] Portainer link (u/pwd: admin/antipode): {portainer_url}")

  ansible_playbook['start-prometheus.yaml'] & FG
  prometheus_url = f"http://{inventory['manager']['external_ip']}:9090/graph?g0.expr=100%20-%20(avg%20by%20(instance)%20(irate(node_cpu_seconds_total%7Bjob%3D%22nodeexporter%22%2Cmode%3D%22idle%22%7D%5B5m%5D))%20*%20100)&g0.tab=0&g0.stacked=0&g0.range_input=10m&g0.step_input=1&g1.expr=(node_memory_MemTotal_bytes%20-%20node_memory_MemFree_bytes)%2Fnode_memory_MemTotal_bytes%20*100&g1.tab=0&g1.stacked=0&g1.range_input=10m&g1.step_input=1&g2.expr=(node_filesystem_size_bytes%7Bmountpoint%3D%22%2F%22%7D%20-%20node_filesystem_free_bytes%7Bmountpoint%3D%22%2F%22%7D)%2Fnode_filesystem_size_bytes%7Bmountpoint%3D%22%2F%22%7D%20*100&g2.tab=0&g2.stacked=0&g2.range_input=10m&g2.step_input=1&g3.expr=rate(node_network_transmit_bytes_total%7Bdevice%3D%22ens4%22%7D%5B10s%5D)*8%2F1024%2F1024&g3.tab=0&g3.stacked=0&g3.range_input=10m&g3.step_input=1"
  _wait_url_up(prometheus_url)
  print(f"[INFO] Prometheus link: {prometheus_url}")

  # start dsb services
  ansible_playbook['start-dsb.yml', '-e', 'app=socialNetwork'] & FG

  # init social graph
  # ansible_playbook['init-social-graph.yml', '-e', 'app=socialNetwork', '-e', f"configuration={filepath}"] & FG

  print("[INFO] Run Complete!")


#-----------------
# CLEAN
#-----------------
def clean(args):
  try:
    getattr(sys.modules[__name__], f"clean__{args['app']}__{_deploy_type(args)}")(args)
  except KeyboardInterrupt:
    # if the compose gets interrupted we just continue with the script
    pass

def clean__socialNetwork__local(args):
  from plumbum.cmd import docker_compose, docker

  os.chdir(ROOT_PATH / args['app'])
  # first stops the containers
  docker_compose['stop'] & FG

  if args['strong']:
    docker_compose['down', '--rmi', 'all', '--remove-orphans'] & FG
  else:
    docker_compose['down'] & FG

def clean__socialNetwork__gsd(args):
  from plumbum.cmd import ansible_playbook

  # change path to playbooks folder
  os.chdir(ROOT_PATH / 'deploy' / 'gsd')

  ansible_playbook['undeploy-swarm.yml', '-e', 'app=socialNetwork'] & FG
  print("[INFO] Clean Complete!")

def clean__socialNetwork__gcp(args):
  _force_docker()
  from plumbum.cmd import ansible_playbook

  # change path to playbooks folder
  os.chdir(ROOT_PATH / 'deploy' / 'gcp')
  inventory = _inventory_to_dict(ROOT_PATH / 'deploy' / 'gcp' / 'inventory.cfg')
  client_inventory = _inventory_to_dict(ROOT_PATH / 'deploy' / 'gcp' / 'clients_inventory.cfg')

  # delete instances if strong enabled
  if args['strong']:
    ansible_playbook['undeploy-swarm.yml', '-e', 'app=socialNetwork'] & FG
    for name,host in inventory.items():
      _gcp_delete_instance(host['zone'], name)
    for name,host in client_inventory.items():
      _gcp_delete_instance(host['zone'], name)
  elif args['restart']:
    ansible_playbook['restart-dsb.yml', '-i', 'inventory.cfg', '-i', 'clients_inventory.cfg', '-e', 'app=socialNetwork'] & FG
  else:
    ansible_playbook['undeploy-swarm.yml', '-e', 'app=socialNetwork'] & FG

  print("[INFO] Clean Complete!")

def clean__hotelReservation__local(args):
  from plumbum.cmd import docker_compose, docker

  os.chdir(ROOT_PATH / args['app'])
  # first stops the containers
  docker_compose['stop'] & FG

  if args['strong']:
    docker_compose['down', '--rmi', 'all', '--remove-orphans'] & FG
  else:
    docker_compose['down'] & FG


#-----------------
# DELAY
#-----------------
def delay(args):
  try:
    delay_ms = f"{args['delay']}ms"
    jitter_ms = f"{args['jitter']}ms"
    correlation_per = f"{args['correlation']}%"
    distribution = args['distribution']
    # params - TODO move to args later on
    src_container = 'post-storage-mongodb-eu'
    dst_container = 'post-storage-mongodb-us'

    if args['jitter'] == 0 and distribution in [ 'normal', 'pareto', 'paretonormal']:
      raise argparse.ArgumentTypeError(f"{distribution} does not allow for 0ms jitter")

    getattr(sys.modules[__name__], f"delay__{args['app']}__{_deploy_type(args)}")(args, src_container, dst_container, delay_ms, jitter_ms, correlation_per, distribution)
  except KeyboardInterrupt:
    # if the compose gets interrupted we just continue with the script
    pass

def delay__socialNetwork__local(args, src_container, dst_container, delay_ms, jitter_ms, correlation_per, distribution):
  from plumbum.cmd import docker_compose, docker

  os.chdir(ROOT_PATH / args['app'])

  # get ip of the dst container since delay only accepts ips
  # dst_container_id = docker_compose['ps', '-q', dst_container].run()[1].rstrip()
  # dst_ip = docker['inspect', '-f' '{{range.NetworkSettings.Networks}}{{.IPAddress}}{{end}}', dst_container_id].run()[1].rstrip()

  docker_compose['exec', src_container, '/home/delay.sh', dst_container, delay_ms, jitter_ms, correlation_per, distribution] & FG

def delay__socialNetwork__gcp(args, src_container, dst_container, delay_ms, jitter_ms, correlation_per, distribution):
  _force_docker()
  from plumbum.cmd import ansible_playbook
  from jinja2 import Environment
  import textwrap
  import yaml

  filepath = args['configuration_path']
  with open(filepath, 'r') as f_conf:
    conf = yaml.load(f_conf, Loader=yaml.FullLoader)

  # change path to playbooks folder
  os.chdir(ROOT_PATH / 'deploy' / 'gcp')

  # checks the configuration for the hostname of the delayed container
  src_gcp_container_hostname = conf['nodes'][conf['services'][src_container]]['hostname']
  dst_gcp_container_hostname = conf['nodes'][conf['services'][dst_container]]['hostname']

  template = """
    ---
    - hosts: {{ src_gcp_container_hostname }}
      gather_facts: no
      become: yes
      any_errors_fatal: true
      vars:
        - stack: deathstarbench
        - app: socialNetwork

      tasks:
        - name: Delay container
          shell: >
              docker exec $( docker ps -a --filter name='{{ src_container }}' --filter status=running --format {{ "{% raw %}'{{ .ID }}'{% endraw %}" }} ) /home/delay.sh {{ dst_container }} {{ delay_ms }} {{ jitter_ms }} {{ correlation_per }} {{ distribution }}
          ignore_errors: True

        - name: Spam ping to kickstart delay
          shell: >
              docker exec $( docker ps -a --filter name='{{ src_container }}' --filter status=running --format {{ "{% raw %}'{{ .ID }}'{% endraw %}" }} ) bash -c "for IP in \$(dig +short post-storage-mongodb-us); do ping -c 200 -f \$IP 2>&1 | tail -1; done"
          register: ping_out

        - name: Check delay
          debug:
            msg: {% raw %}"{{ ping_out.stdout.split('\\n') }}"{% endraw %}

  """
  playbook = Environment().from_string(template).render({
    'src_gcp_container_hostname': src_gcp_container_hostname,
    'dst_gcp_container_hostname': dst_gcp_container_hostname,
    'src_container': src_container,
    'dst_container': dst_container,
    'delay_ms': delay_ms,
    'jitter_ms': jitter_ms,
    'correlation_per': correlation_per,
    'distribution': distribution,
  })

  playbook_filepath = ROOT_PATH / 'deploy' / 'gcp' / 'delay-container.yml'
  with open(playbook_filepath, 'w') as f:
    # remove empty lines and dedent for easier read
    f.write(textwrap.dedent(playbook))
    print(f"[SAVED] '{playbook_filepath}'")

  ansible_playbook['delay-container.yml',
    '-e', 'app=socialNetwork',
  ] & FG
  print("[INFO] Delay Complete!")


#-----------------
# WORKLOAD
#-----------------
def wkld(args):
  import urllib.parse

  try:
    app_dir = ROOT_PATH / args['app']
    os.chdir(app_dir)

    # get host for each zone
    hosts, main_host = getattr(sys.modules[__name__], f"wkld__{args['app']}__{_deploy_type(args)}__hosts")(args)

    endpoint = AVAILABLE_WKLD_ENDPOINTS[args['app']][args['Endpoint']]

    if endpoint['type'] == 'wrk2':
      # build arguments
      wrk2_args = []
      if 'connections' in args:
        wrk2_args.extend(['--connections', args['connections']])
      if 'duration' in args:
        wrk2_args.extend(['--duration', f"{args['duration']}s"])
      if 'threads' in args:
        wrk2_args.extend(['--threads', args['threads']])

      # add arguments to previous list
      wrk2_args.extend([
          '--latency',
          '--script', str(Path('/scripts') / endpoint['script_path'].split('wrk2/scripts/')[1]),
          urllib.parse.urljoin(hosts[main_host], endpoint['uri']),
          '--rate', args['requests']
        ])

      # add docker arguments
      wkld_args = ['run',
        '--rm', '-it',
        '--network=host',
        '-v', f"{app_dir / 'wrk2' / 'scripts'}:/scripts",
      ]
      # add hosts env vars
      for k,v in hosts.items():
        wkld_args += [ '-e', f"{k.upper()}"]
      # add remaing vars
      wkld_args += [
          'wrk2:antipode',
          './wrk'
        ] + wrk2_args

      exe_path = 'docker'
      exe_args = wkld_args

    elif endpoint['type'] == 'python':
      script_path = app_dir.joinpath(endpoint['script_path'])
      exe_path = 'python3'
      exe_args = [script_path]

    # run workload for endpoint with correct env variables
    for k,v in hosts.items():
      k = k.upper()
      local.env[k] = v
    getattr(sys.modules[__name__], f"wkld__{_deploy_type(args)}__run")(args, hosts, exe_path, exe_args)

  except KeyboardInterrupt:
    # if the compose gets interrupted we just continue with the script
    pass

#-----------------

def wkld__socialNetwork__local__hosts(args):
  return {
    'host_eu': 'http://127.0.0.1:8080',
    'host_us': 'http://127.0.0.1:8082',
  }, 'host_eu'

def wkld__hotelReservation__local__hosts(args):
  return {
    'host': 'http://127.0.0.1:5000',
  }, 'host'

def wkld__socialNetwork__gsd__hosts(args):
  import yaml
  # eu - nginx-thrift: node23
  # us - nginx-thrift-us: node24§

  filepath = args['configuration_path']
  with open(filepath, 'r') as f_conf:
    conf = yaml.load(f_conf, Loader=yaml.FullLoader)
    return {
      'host_eu': f"http://{conf['services']['nginx-thrift']}:8080",
      'host_us': f"http://{conf['services']['nginx-thrift-us']}:8082",
    }, 'host_eu'

def wkld__socialNetwork__gcp__hosts(args):
  import yaml

  filepath = args['configuration_path']
  with open(filepath, 'r') as f_conf:
    conf = yaml.load(f_conf, Loader=yaml.FullLoader)
    inventory = _inventory_to_dict(ROOT_PATH / 'deploy' / 'gcp' / 'inventory.cfg')

    return {
      'host_eu': f"http://{inventory[conf['services']['nginx-thrift']]['external_ip']}:8080",
      'host_us': f"http://{inventory[conf['services']['nginx-thrift-us']]['external_ip']}:8082",
    }, 'host_eu'

#-----------------

def wkld__local__run(args, hosts, exe_path, exe_args):
  exe = local[exe_path]
  exe[exe_args] & FG

def wkld__gsd__run(args, hosts, exe_path, exe_args):
  from plumbum.cmd import ansible_playbook
  from jinja2 import Environment
  import textwrap


  if not args['node']:
    print("[INFO] No nodes passed onto gsd deployment workload. Using current node with remote hosts.")
    wkld__socialNetwork__local__run(args, hosts, exe_path, exe_args)
    return

  app_dir = ROOT_PATH / args['app']
  os.chdir(ROOT_PATH / 'deploy')

  conf_filepath = args['configuration_path']

  # Create inventory for clients
  template = """
    [clients]
    {% for k,v in client_nodes.items() %}{{k}} ansible_host={{v}} ansible_user=jfloff ansible_ssh_private_key_file=~/.ssh/id_rsa_inesc_cluster_jfloff
    {% endfor %}
  """
  inventory = Environment().from_string(template).render({
    'client_nodes': { n:GSD_AVAILABLE_NODES[n] for n in set(args['node']) },
  })
  inventory_filepath = Path('gsd/clients_inventory.cfg')
  with open(inventory_filepath, 'w') as f:
    # remove empty lines and dedent for easier read
    f.write(textwrap.dedent(inventory))

  print(f"[SAVED] '{inventory_filepath.resolve()}'")

  # Create script to run
  conf_id = f"{conf_filepath.stem}__{datetime.now().strftime('%Y%m%d%H%M%S')}"
  out_folder = f"/tmp/dsb-wkld-data/{conf_id}"
  out_filepath = f"{out_folder}/{conf_filepath.stem}__{datetime.now().strftime('%Y%m%d%H%M%S')}__$(hostname).out"
  template = """
    #! /bin/bash

    export HOST_EU={{ host_eu }}
    export HOST_US={{ host_us }}
    mkdir -p {{out_folder}}
    {{exe_path}} {{exe_args}} | tee {{out_filepath}}
  """
  script = Environment().from_string(template).render({
    'host_eu': hosts['host_eu'],
    'host_us': hosts['host_us'],
    'exe_path': exe_path,
    'exe_args': ' '.join([str(e) for e in exe_args]),
    'out_folder': out_folder,
    'out_filepath': out_filepath,
  })
  script_filepath = Path('gsd/wkld-run.sh')
  with open(script_filepath, 'w') as f:
    # remove empty lines and dedent for easier read
    f.write(textwrap.dedent(script))
  # add executable permissions
  script_filepath.chmod(script_filepath.stat().st_mode | stat.S_IEXEC)

  print(f"[SAVED] '{script_filepath.resolve()}'")

  # change path to playbooks folder
  os.chdir(ROOT_PATH / 'deploy' / 'gsd')
  ansible_playbook['wkld-run.yml', '-i', 'clients_inventory.cfg', '-e', 'app=socialNetwork'] & FG

  # sleep before gather
  if args['duration'] is not None:
    sleep_for = args['duration'] + 60
    print(f"[INFO] Sleeping for {sleep_for} seconds ...")
    time.sleep(sleep_for)
  else:
    input("Press any key when done ...")

  ansible_playbook['wkld-gather.yml', '-i', 'clients_inventory.cfg', '-e', 'app=socialNetwork', '-e', f'conf_id={conf_id}' ] & FG

def wkld__gcp__run(args, hosts, exe_path, exe_args):
  from plumbum.cmd import ansible_playbook
  from jinja2 import Environment
  import textwrap
  import yaml

  conf_filepath = args['configuration_path']

  with open(conf_filepath, 'r') as f_conf:
    conf = yaml.load(f_conf, Loader=yaml.FullLoader)

    client_nodes = list(set(conf['clients']) & set(args['node']))
    if not client_nodes:
      print("[INFO] No nodes passed onto GCP deployment workload. Using current local node as client.")
      wkld__socialNetwork__local__run(args, hosts, exe_path, exe_args)
      return

    # inventory = _inventory_to_dict(ROOT_PATH / 'deploy' / 'gcp' / 'inventory.cfg')
    _force_docker()
    os.chdir(ROOT_PATH / 'deploy')

    # build inventory while creating instances
    inventory = {}
    for client_name in client_nodes:
      client_info = conf['nodes'][client_name]

      inventory[client_name] = {
        'hostname': client_info['hostname'],
        'zone': client_info['zone'],
        'external_ip': None,
        'internal_ip': None,
      }
      _gcp_create_instance(
        zone=client_info['zone'],
        name=client_name,
        machine_type=client_info['machine_type'],
        hostname=client_info['hostname'],
        firewall_tags=[]
      )

    # wait for instances public ips
    print("[INFO] Waiting for GCP nodes to have public IP addresses ...")
    for node_key, node_info in inventory.items():
      while inventory[node_key].get('external_ip', None) is None:
        metadata = _gcp_get_instance(node_info['zone'], node_key)
        try:
          inventory[node_key]['external_ip'] = metadata['networkInterfaces'][0]['accessConfigs'][0]['natIP']
          inventory[node_key]['internal_ip'] = metadata['networkInterfaces'][0]['networkIP']
        except KeyError:
          inventory[node_key]['external_ip'] = None
          inventory[node_key]['internal_ip'] = None

    # we faced some connections bugs from time to time, we sleep to wait a bit more for the machines to be ready
    time.sleep(60)
    print("[INFO] GCP Nodes ready!")

    # now build the inventory
    # if you want to get a new google_compute_engine key:
    #   1) go to https://console.cloud.google.com/compute/instances
    #   2) choose one instance, click on SSH triangle for more options
    #   3) 'View gcloud command'
    #   4) Run that command and then copy the generated key
    #
    template = """
      [clients]
      {% for node_name,node in client_nodes.items() %}
      {{ node['hostname'] }} ansible_host={{ node['external_ip'] }} gcp_zone={{ node['zone'] }} gcp_name={{ node_name }} gcp_host={{ node['internal_ip'] }} ansible_user=root ansible_ssh_private_key_file={{ private_ssh_key_path }}
      {% endfor %}
    """

    # save the docker version of the client_inventory
    inventory_contents = Environment().from_string(template).render({
      'project_id': GCP_PROJECT_ID,
      'private_ssh_key_path': f"/code/deploy/gcp/{GCP_PROJECT_ID}_google_compute_engine",
      'client_nodes': inventory,
    })
    inventory_filepath = ROOT_PATH / 'deploy' / 'gcp' / 'clients_inventory.cfg'
    with open(inventory_filepath, 'w') as f:
      # remove empty lines and dedent for easier read
      f.write(textwrap.dedent(inventory_contents))
    print(f"[SAVED] '{inventory_filepath}'")

    # save the local version of the client_inventory
    inventory_contents = Environment().from_string(template).render({
      'project_id': GCP_PROJECT_ID,
      'private_ssh_key_path': f"{ os.environ.get('HOST_ROOT_PATH') }/deploy/gcp/{ GCP_PROJECT_ID }_google_compute_engine",
      'client_nodes': inventory,
    })
    inventory_filepath = ROOT_PATH / 'deploy' / 'gcp' / 'clients_inventory_local.cfg'
    with open(inventory_filepath, 'w') as f:
      # remove empty lines and dedent for easier read
      f.write(textwrap.dedent(inventory_contents))
    print(f"[SAVED] '{inventory_filepath}'")

    # Create script to run
    wkld_filename = f"$(hostname).out"
    conf_path = f"{conf_filepath.stem}/{datetime.now().strftime('%Y%m%d%H%M%S')}"
    wkld_folderpath = f"/tmp/dsb-wkld-data/{conf_path}/"
    template = """
      #! /bin/bash

      export HOST_EU={{ host_eu }}
      export HOST_US={{ host_us }}
      mkdir -p {{wkld_folderpath}}
      {{exe_path}} {{exe_args}} | tee {{wkld_folderpath}}/{{wkld_filename}}
    """
    script = Environment().from_string(template).render({
      'host_eu': hosts['host_eu'],
      'host_us': hosts['host_us'],
      'exe_path': exe_path,
      'exe_args': ' '.join([str(e) for e in exe_args]),
      'wkld_folderpath': wkld_folderpath,
      'wkld_filename': wkld_filename,
    })
    script_filepath = ROOT_PATH / 'deploy' / 'gcp' / 'wkld-run.sh'
    with open(script_filepath, 'w') as f:
      # remove empty lines and dedent for easier read
      f.write(textwrap.dedent(script))
    # add executable permissions
    script_filepath.chmod(script_filepath.stat().st_mode | stat.S_IEXEC)

  # change path to playbooks folder
  os.chdir(ROOT_PATH / 'deploy' / 'gcp')
  ansible_playbook['wkld-run.yml', '-i', 'clients_inventory.cfg', '-e', 'app=socialNetwork'] & FG

  # sleep before gather
  if args['duration'] is not None:
    sleep_for = args['duration'] + 30
    print(f"[INFO] Waiting {sleep_for} seconds for workload to be ready ...")
    time.sleep(sleep_for)
  else:
    input("Press any key when workload is done ...")



#-----------------
# GATHER
#-----------------
def _fetch_span_tag(tags, tag_to_search):
  return next(item for item in tags if item['key'] == tag_to_search)['value']

def _fetch_compose_post_service_traces(jaeger_host, limit):
  import requests
  import pandas as pd

  # curl -X GET "jaeger:16686/api/traces?service=write-home-timeline-service&prettyPrint=true
  params = (
    # ('service', 'antipode-oracle'),
    ('service', 'compose-post-service'),
    ('limit', limit),
    ('lookback', '1h'),
    # ('prettyPrint', 'true'),
  )
  response = requests.get(f'{jaeger_host}/api/traces', params=params)

  # error if we do not get a 200 OK code
  if response.status_code != 200 :
    print("[ERROR] Could not fetch traces from Jaeger")
    exit(-1)

  # pick only the traces with the desired info
  content = response.json()
  missing_info = 0
  traces = []
  for trace in content['data']:
    trace_info = {
      'ts': None,
      'post_id': None,
      # 'trace_id': trace['spans'][0]['traceID'],
      #
      'poststorage_post_written_ts': None,
      'poststorage_read_notification_ts': None,
      'consistency_bool': None,
      'consistency_mongoread_duration': None,
      #
      'wht_start_queue_ts': None,
      'wht_start_worker_ts': None,
      'wht_antipode_duration': None,
    }
    # search trace info in different spans
    for s in trace['spans']:
      if s['operationName'] == '/wrk2-api/post/compose':
        trace_info['ts'] = datetime.fromtimestamp(s['startTime']/1000000.0)
      elif s['operationName'] == '_ComposeAndUpload':
        trace_info['post_id'] = int(_fetch_span_tag(s['tags'], 'composepost_id'))
      elif s['operationName'] == 'StorePost':
        trace_info['poststorage_post_written_ts'] = int(_fetch_span_tag(s['tags'], 'poststorage_post_written_ts'))
      elif s['operationName'] == 'FanoutHomeTimelines':
        trace_info['poststorage_read_notification_ts'] = int(_fetch_span_tag(s['tags'], 'poststorage_read_notification_ts'))
        trace_info['consistency_bool'] = _fetch_span_tag(s['tags'], 'consistency_bool')
        trace_info['consistency_mongoread_duration'] = float(_fetch_span_tag(s['tags'], 'consistency_mongoread_duration'))
        # compute the time spent in the queue
        trace_info['wht_start_worker_ts'] = float(_fetch_span_tag(s['tags'], 'wht_start_worker_ts'))
        # duration spent in antipode
        trace_info['wht_antipode_duration'] = float(_fetch_span_tag(s['tags'], 'wht_antipode_duration'))
      elif s['operationName'] == '_UploadHomeTimelineHelper':
        # compute the time spent in the queue
        trace_info['wht_start_queue_ts'] = float(_fetch_span_tag(s['tags'], 'wht_start_queue_ts'))

    # skip if we still have -1 values
    if any(v is None for v in trace_info.values()):
      # print(f"[INFO] trace missing information: {trace_info}")
      missing_info += 1
      continue

    # computes the difference in ms from post to notification
    diff = datetime.fromtimestamp(trace_info['poststorage_read_notification_ts']/1000.0) - datetime.fromtimestamp(trace_info['poststorage_post_written_ts']/1000.0)
    trace_info['post_notification_diff_ms'] = float(diff.total_seconds() * 1000)

    # computes time spent queued in rabbitmq
    diff = datetime.fromtimestamp(trace_info['wht_start_worker_ts']/1000.0) - datetime.fromtimestamp(trace_info['wht_start_queue_ts']/1000.0)
    trace_info['wht_queue_duration'] = float(diff.total_seconds() * 1000)

    # vl from nginx to notification
    diff = datetime.fromtimestamp(trace_info['poststorage_read_notification_ts']/1000.0) - trace_info['ts']
    trace_info['wht_vl_duration'] = float(diff.total_seconds() * 1000)

    traces.append(trace_info)

  # Build dataframe with all the traces
  df = pd.DataFrame(traces)
  df = df.set_index('ts')

  return df, missing_info

def _fetch_mongo_change_stream_traces(jaeger_host, limit):
  import requests
  import pandas as pd

  # curl -X GET "jaeger:16686/api/traces?service=write-home-timeline-service&prettyPrint=true
  params = (
    ('service', 'write-home-timeline-service-us'),
    ('limit', limit),
    ('lookback', '1h'),
    # ('prettyPrint', 'true'),
  )
  response = requests.get(f'{jaeger_host}/api/traces', params=params)

  # error if we do not get a 200 OK code
  if response.status_code != 200 :
    print("[ERROR] Could not fetch traces from Jaeger")
    exit(-1)

  # pick only the traces with the desired info
  content = response.json()
  missing_info = 0
  traces = []
  for trace in content['data']:
    trace_info = {
      'post_id': None,
      #
      'consistency_diff': None,
    }
    # search trace info in different spans
    for s in trace['spans']:
      if s['operationName'] == 'WriteHomeTimeline-MongoChangeStream':
        trace_info['post_id'] = int(_fetch_span_tag(s['tags'], 'composepost_id'))
        trace_info['consistency_diff'] = int(_fetch_span_tag(s['tags'], 'consistency_diff'))

    # skip if we still have -1 values
    if any(v is None for v in trace_info.values()):
      # print(f"[INFO] trace missing information: {trace_info}")
      missing_info += 1
      continue

    traces.append(trace_info)

  # Build dataframe with all the traces
  df = pd.DataFrame(traces)

  return df, missing_info


def gather(args):
  import pandas as pd
  from plumbum.cmd import sudo, hostname

  print("[INFO] Gather client output ...")
  getattr(sys.modules[__name__], f"gather__{args['app']}__{_deploy_type(args)}__client_output")(args)

  print("[INFO] Gather jaeger traces ...")
  # pd.set_option('display.float_format', lambda x: '%.3f' % x)
  pd.set_option('display.html.table_schema', True)
  pd.set_option('display.precision', 5)

  # no default configuration use deploy type
  configuration = _deploy_type(args)
  if args['configuration_path']:
    configuration = Path(args['configuration_path']).stem

  # create folder if needed
  wkld_data_parent_path = ROOT_PATH / 'deploy' / 'wkld-data' / configuration
  os.makedirs(wkld_data_parent_path, exist_ok=True)

  # get latest conf directory
  if os.listdir(wkld_data_parent_path):
    wkld_data_path = max(wkld_data_parent_path.iterdir())
  else:
    # create folder with current timestamp if none exist
    wkld_data_path = wkld_data_parent_path / time.strftime('%Y%m%d%H%M%S')
    os.makedirs(wkld_data_path)

  # force chmod of that dir
  sudo['chmod', 777, wkld_data_path] & FG

  os.chdir(wkld_data_path)

  try:
    # get host for each zone
    jaeger_host = getattr(sys.modules[__name__], f"gather__{args['app']}__{_deploy_type(args)}__jaeger_host")(args)

    limit = args['num_requests']
    if limit is None:
      limit = int(input(f"Visit {jaeger_host}/dependencies to check number of flowing requests: "))

    tag = args['tag']
    if tag is None:
      tag = input(f"Input any tag for this gather: ")

    df,missing_info = _fetch_compose_post_service_traces(jaeger_host, limit)

    # merge result with mongodb change stream consistency diff information
    # consistency_df,_ = _fetch_mongo_change_stream_traces(jaeger_host, limit)
    # df = df.join(consistency_df.set_index('post_id'), on='post_id')

    # remove unecessary columns
    del df['post_id']
    del df['poststorage_post_written_ts']
    del df['poststorage_read_notification_ts']
    del df['wht_start_queue_ts']
    del df['wht_start_worker_ts']

    # save to csv so we can plot a timeline later
    df.to_csv('traces.csv', sep=';', mode='w')
    print(f"[INFO] Save '{wkld_data_path}/traces.csv'")

    # compute extra info to output in info file
    consistent_df = df[df['consistency_bool'] == True]
    inconsistent_df = df[df['consistency_bool'] == False]
    inconsistent_count = len(inconsistent_df)
    consistent_count = len(consistent_df)

    # save to file
    with open('traces.info', 'w') as f:
      print(f"{missing_info} messages skipped due to missing information", file=f)
      if tag:
        print(f"GATHER TAG: {tag}", file=f)
      print("", file=f)
      print(f"% inconsistencies: {inconsistent_count/float(len(df))}", file=f)
      print("", file=f)
      print("TOTALS", file=f)
      print(df.describe(percentiles=PERCENTILES_TO_PRINT), file=f)
      print("", file=f)
      print("--- INCONSISTENCIES", file=f)
      print(inconsistent_df.describe(percentiles=PERCENTILES_TO_PRINT), file=f)
      print("", file=f)
      print("--- CONSISTENCIES", file=f)
      print(consistent_df.describe(percentiles=PERCENTILES_TO_PRINT), file=f)

    # print file to stdout
    print(f"[INFO] Save '{wkld_data_path}/traces.info'\n")
    with open('traces.info', 'r') as f:
      print(f.read())

  except KeyboardInterrupt:
    # if the compose gets interrupted we just continue with the script
    pass

def gather__socialNetwork__local__jaeger_host(args):
  from plumbum import FG, BG
  from plumbum.cmd import sudo, hostname

  public_ip = hostname['-I']().split()[1]
  return f'http://{public_ip}:16686'

def gather__socialNetwork__gsd__jaeger_host(args):
  import yaml

  filepath = args['configuration_path']
  with open(filepath, 'r') as f_conf:
    conf = yaml.load(f_conf, Loader=yaml.FullLoader)
    return f"http://{GSD_AVAILABLE_NODES[conf['services']['jaeger']]}:16686"

def gather__socialNetwork__gcp__jaeger_host(args):
  import yaml

  filepath = args['configuration_path']
  with open(filepath, 'r') as f_conf:
    conf = yaml.load(f_conf, Loader=yaml.FullLoader)
    inventory = _inventory_to_dict(ROOT_PATH / 'deploy' / 'gcp' / 'inventory.cfg')

    jaeger_public_ip = inventory[conf['services']['jaeger']]['external_ip']
    return f"http://{jaeger_public_ip}:16686"

def gather__socialNetwork__local__client_output(args):
  return None

def gather__socialNetwork__gsd__client_output(args):
  return None

def gather__socialNetwork__gcp__client_output(args):
  from plumbum.cmd import ansible_playbook

  filepath = args['configuration_path']
  conf_path = f"{filepath.stem}"

  os.chdir(ROOT_PATH / 'deploy' / 'gcp')
  ansible_playbook['wkld-gather.yml', '-i', 'clients_inventory_local.cfg', '-e', 'app=socialNetwork', '-e', f'conf_path={conf_path}' ] & FG

#-----------------
# MAIN
#-----------------
if __name__ == "__main__":
  import argparse

  # parse arguments
  main_parser = argparse.ArgumentParser()
  main_parser.add_argument("app", choices=AVAILABLE_APPLICATIONS, help="Application to deploy")
  # deploy type group
  deploy_type_group = main_parser.add_mutually_exclusive_group(required=True)
  for dt,info in AVAILABLE_DEPLOY_TYPES.items():
    deploy_type_group.add_argument(f'--{dt}', action='store_true', help=f"Deploy app to {info['name']}")
  # different commands
  subparsers = main_parser.add_subparsers(help='commands', dest='which')

  # build application
  build_parser = subparsers.add_parser('build', help='Build application')
  build_parser.add_argument('--strong', action='store_true', help="rebuild all containers")

  # clean application
  clean_parser = subparsers.add_parser('clean', help='Clean application')
  clean_parser.add_argument('-s', '--strong', action='store_true', help="delete images")
  clean_parser.add_argument('-r', '--restart', action='store_true', help="clean deployment by restarting containers")

  # delay application
  delay_parser = subparsers.add_parser('delay', help='Delay application')
  # deploy file group
  deploy_file_group = delay_parser.add_mutually_exclusive_group(required=False)
  deploy_file_group.add_argument('-l', '--latest', action='store_true', help="Use last used deploy file")
  deploy_file_group.add_argument('-f', '--file', type=argparse.FileType('r', encoding='UTF-8'), help="Use specific file")
  # other options
  delay_parser.add_argument('-d', '--delay', type=float, default='100', help="Delay in ms")
  delay_parser.add_argument('-j', '--jitter', type=float, default='0', help="Jitter in ms")
  delay_parser.add_argument('-c', '--correlation', type=int, default='0', help="Correlation in % (0-100)")
  delay_parser.add_argument('-dist', '--distribution', choices=[ 'uniform', 'normal', 'pareto', 'paretonormal' ], default='uniform', help="Delay distribution")

  # run application
  run_parser = subparsers.add_parser('run', help='Run application')
  run_parser.add_argument('-d', '--detached', action='store_true', help="detached")
  run_parser.add_argument('-ant', '--antipode', action='store_true', default=False, help="enable antipode")
  run_parser.add_argument('--info', action='store_true', help="build")
  # deploy file group
  deploy_file_group = run_parser.add_mutually_exclusive_group(required=False)
  deploy_file_group.add_argument('-l', '--latest', action='store_true', help="Use last used deploy file")
  deploy_file_group.add_argument('-f', '--file', type=argparse.FileType('r', encoding='UTF-8'), help="Use specific file")

  # deploy application
  deploy_parser = subparsers.add_parser('deploy', help='Deploy application')
  # deploy file group
  deploy_file_group = deploy_parser.add_mutually_exclusive_group(required=False)
  deploy_file_group.add_argument('-n', '--new', action='store_true', help="Build a new deploy file")
  deploy_file_group.add_argument('-l', '--latest', action='store_true', help="Use last used deploy file")
  deploy_file_group.add_argument('-f', '--file', type=argparse.FileType('r', encoding='UTF-8'), help="Use specific file")

  # workload application
  wkld_parser = subparsers.add_parser('wkld', help='Run HTTP workload generator')
  # deploy file group
  deploy_file_group = wkld_parser.add_mutually_exclusive_group(required=False)
  deploy_file_group.add_argument('-l', '--latest', action='store_true', help="Use last used deploy file")
  deploy_file_group.add_argument('-f', '--file', type=argparse.FileType('r', encoding='UTF-8'), help="Use specific file")
  # comparable with wrk2 > ./wrk options
  wkld_parser.add_argument('-N', '--node', action='append', default=[], help="Run wkld on the following nodes")
  wkld_parser.add_argument('-E', '--Endpoint', choices=[ e for app_list in AVAILABLE_WKLD_ENDPOINTS.values() for e in app_list ], help="Endpoints to generate workload for")
  wkld_parser.add_argument('-c', '--connections', type=int, default=1, help="Connections to keep open")
  wkld_parser.add_argument('-d', '--duration', type=int, default='1', help="Duration in s")
  wkld_parser.add_argument('-t', '--threads', type=int, default=1, help="Number of threads")
  wkld_parser.add_argument('-r', '--requests', type=int, default=1, required=True, help="Work rate (throughput) in request per second total")
  # Existing options:
  # -D, --dist             fixed, exp, norm, zipf
  # -P                     Print each request's latency
  # -p                     Print 99th latency every 0.2s to file
  # -L  --latency          Print latency statistics

  # gather application
  gather_parser = subparsers.add_parser('gather', help='Gather data from application')
  # deploy file group
  deploy_file_group = gather_parser.add_mutually_exclusive_group(required=False)
  deploy_file_group.add_argument('-l', '--latest', action='store_true', help="Use last used deploy file")
  deploy_file_group.add_argument('-f', '--file', type=argparse.FileType('r', encoding='UTF-8'), help="Use specific file")
  gather_parser.add_argument('-n', '--num-requests', type=int, default=None, help="Gather this amount of requests skipping the input")
  gather_parser.add_argument('-t', '--tag', type=str, default=None, help="Tags the input with the following string")


  args = vars(main_parser.parse_args())
  command = args.pop('which')

  # change application dir if necessary
  # if Path.cwd().name != args['app']:
  #   os.chdir(Path.cwd().joinpath(args['app']))

  deploy_type = _deploy_type(args)
  args['configuration_path'] = None
  if 'latest' in args and args['latest']:
    args['configuration_path'] = _last_configuration('socialNetwork', deploy_type)
  if 'file' in args and args['file']:
    args['configuration_path'] = ROOT_PATH / args['file'].name

  # call parser method dynamically
  getattr(sys.modules[__name__], command)(args)
