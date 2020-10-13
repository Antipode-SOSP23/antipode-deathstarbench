#!/usr/bin/env python3

import os
import sys
from pprint import pprint
from pathlib import Path
from plumbum import local
from plumbum import FG, BG
from plumbum.cmd import docker_compose, docker #, ansible_playbook
from plumbum.cmd import make
from plumbum.cmd import python
import itertools
import urllib.parse
import requests
from datetime import datetime
import pandas as pd
# import click
import time
import yaml
import glob
from jinja2 import Environment
import textwrap

#############################
# Pre-requisites
#
# > sudo apt-get install libssl-dev libz-dev luarocks python3
# > sudo luarocks install luasocket json-lua
# > pip install plumbum ansible
#

#############################
# HELPER
#
def _index_containing_substring(the_list, substring):
  for i, s in enumerate(the_list):
    if substring in s:
      return i
  return -1

#############################
# CONSTANTS
#

# name of the folder where the app root is
AVAILABLE_APPLICATIONS = [
  'socialNetwork',
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
  }
}
AVAILABLE_NODES = {
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
SWARM_MANAGER_NODE = { 'node34': '10.100.0.44' }

#############################
# BUILD
#
def build(args):
  app_dir = Path.cwd()
  try:
    # By default, the DeathStarBench pulls its containers from docker hub.
    # We need to override these with our modified X-Trace containers.
    # To do this, we will manually build the docker images for the modified components.

    thrift_microservice_args = ['build', '-t', 'yg397/thrift-microservice-deps:antipode', '.']
    openresty_thrift_args = ['build', '-t', 'yg397/openresty-thrift', '-f', 'xenial/Dockerfile', '.']

    # adds --no-cache option to build so it rebuilds everything
    if args['strong']:
      thrift_microservice_args.insert(1, '--no-cache')
      openresty_thrift_args.insert(1, '--no-cache')

    # 1. Build the base docker image that contains all the dependent libraries.  We modified this to add X-Trace and protocol buffers.
    os.chdir(app_dir.joinpath('docker', 'thrift-microservice-deps', 'cpp'))
    docker[thrift_microservice_args] & FG
    os.chdir(app_dir)

    # 2. Build the nginx server image. We modified this to add X-Trace and protocol buffers
    os.chdir(app_dir.joinpath('docker', 'openresty-thrift'))
    docker[openresty_thrift_args] & FG
    os.chdir(app_dir)

    # 3. Build the social network docker image
    docker['build', '-t', 'yg397/social-network-microservices:antipode', '.'] & FG

  except KeyboardInterrupt:
    # if the compose gets interrupted we just continue with the script
    pass


#############################
# RUN
#
def run(args):
  app_dir = Path.cwd()
  try:
    run_args = ['up']

    # run containers in detached mode
    if args['detached']:
      run_args.insert(1, '-d')
    if args['build']:
      run_args.insert(1, '--build')

    docker_compose[run_args] & FG

  except KeyboardInterrupt:
    # if the compose gets interrupted we just continue with the script
    pass


#############################
# DEPLOY
#
def deploy(args):
  app_dir = Path.cwd()
  try:
    # figure out deploy type
    if args['gsd']:
      deploy_type = 'gsd'
    if args['gcp']:
      deploy_type = 'gcp'
    # base path of configuration file
    conf_base_path = Path(app_dir, '../deploy/configurations')
    if args['new']:
      filename = f"{args['app']}-{deploy_type}-{time.strftime('%Y%m%d%H%M%S')}.yml"
      filepath = conf_base_path / filename

      # services per app
      default_services = {}
      if args['app'] == 'socialNetwork':
        default_services = {
          'social-graph-service': 'HOSTNAME',
          'social-graph-mongodb': 'HOSTNAME',
          'social-graph-redis': 'HOSTNAME',
          'write-home-timeline-service': 'HOSTNAME',
          'write-home-timeline-rabbitmq': 'HOSTNAME',
          'home-timeline-redis': 'HOSTNAME',
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
          'jaeger': 'HOSTNAME',
          'xtrace-server': 'HOSTNAME',
          'mongodb-admin': 'HOSTNAME',
          'post-storage-mongodb-setup': 'HOSTNAME',
        }
      else:
        print(f"[ERROR] App '{args['app']}' has no default services to deploy!")
        exit(-1)
    if args['lastest']:
      pattern = conf_base_path / f"{args['app']}-{deploy_type}-*.yml"
      files = glob.glob(str(pattern))

      # not files found
      if not files:
        print("[ERROR] No file found!")
        exit(-1)

      # find most recent one
      files.sort(key=os.path.getctime)
      filepath = files[-1]

    # Update docker-compose.yml
    deploy_nodes = {}
    with open(filepath, 'r') as f_conf, open('docker-compose.yml', 'r') as f_compose:
      conf = yaml.load(f_conf, Loader=yaml.FullLoader)
      compose = yaml.load(f_compose, Loader=yaml.FullLoader)

      # get all the nodes that will be used in deploy
      deploy_nodes = { n:AVAILABLE_NODES[n] for n in set(conf.values()) }

      for sid,hostname in conf.items():
        # get all the constraints
        deploy_constraints = compose['services'][sid]['deploy']['placement']['constraints']
        # get he id of constraint of the node hostname
        node_constraint_index = _index_containing_substring(deploy_constraints, 'node.hostname')
        # replace docker-compose with that constraint
        deploy_constraints[node_constraint_index] = f'node.hostname == {hostname}'

    # create new docker compose
    new_compose_filepath = Path(app_dir, 'docker-compose-swarm.yml')
    with open(new_compose_filepath, 'w') as f_compose:
      yaml.dump(compose, f_compose)
    print(f"\t [SAVED] '{new_compose_filepath}'")

    template = """
      [swarm_manager]
      {% for k,v in swarm_manager.items() %}{{k}} ansible_host={{v}} ansible_user=jfloff ansible_ssh_private_key_file=~/.ssh/id_rsa_inesc_cluster_jfloff
      {% endfor %}

      [cluster]
      {% for k,v in deploy_nodes.items() %}{{k}} ansible_host={{v}} ansible_user=jfloff ansible_ssh_private_key_file=~/.ssh/id_rsa_inesc_cluster_jfloff
      {% endfor %}
    """
    inventory = Environment().from_string(template).render({
      'swarm_manager': SWARM_MANAGER_NODE,
      'deploy_nodes': deploy_nodes,
    })

    inventory_filepath = Path(app_dir, '../deploy/playbooks/inventory.cfg')
    with open(inventory_filepath, 'w') as f:
      # remove empty lines and dedent for easier read
      f.write(textwrap.dedent(inventory))

    print(f"\t [SAVED] '{inventory_filepath}'")
    print("[INFO] Deploy Complete!")

    #
    # ansible-playbook undeploy-swarm.yml
    # ansible-playbook deploy-swarm.yml
    # ansible-playbook start-portainer.yml
    # ansible-playbook start-dsb.yml
    #


  except KeyboardInterrupt:
    # if the compose gets interrupted we just continue with the script
    pass


#############################
# CLEAN
#
def clean(args):
  app_dir = Path.cwd()
  try:
    # first stops the containers
    docker_compose['stop'] & FG

    if args['strong']:
      docker_compose['down', '--rmi', 'all', '--remove-orphans'] & FG
    if args['jaeger']:
      docker_compose['rm', '-f', 'jaeger'] & FG
    else:
      docker_compose['down'] & FG

  except KeyboardInterrupt:
    # if the compose gets interrupted we just continue with the script
    pass


#############################
# WORKLOAD
#
def wkld(args):
  app_dir = Path.cwd()
  try:
    wrk2_endpoints = [ endpoint for endpoint in args['endpoints'] if AVAILABLE_WKLD_ENDPOINTS[args['app']][endpoint]['type'] == 'wrk2' ]
    py_endpoints = [ endpoint for endpoint in args['endpoints'] if AVAILABLE_WKLD_ENDPOINTS[args['app']][endpoint]['type'] == 'python' ]

    if wrk2_endpoints:
      os.chdir(app_dir.joinpath('wrk2'))
      # make workload
      make & FG
      # load bin
      wrk = local["./wrk"]

      # load existing options
      wrk_args = []
      if 'connections' in args:
        wrk_args.extend(['--connections', args['connections']])
      if 'duration' in args:
        wrk_args.extend(['--duration', args['duration']])
      if 'threads' in args:
        wrk_args.extend(['--threads', args['threads']])

      # we run the workload for each endpoint
      for endpoint in wrk2_endpoints:
        # get details for the script and uri path
        details = AVAILABLE_WKLD_ENDPOINTS[args['app']][endpoint]
        script_path = app_dir.joinpath(details['script_path'])
        uri = urllib.parse.urljoin('http://localhost:8080', details['uri'])
        # add arguments to previous list
        wrk_args.extend(['--latency', '--script', str(script_path), uri, '--rate', args['requests'] ])
        # run workload for each endpoint
        wrk[wrk_args] & FG

      # go back to the app directory
      os.chdir(app_dir)

    if py_endpoints:
      # we run the workload for each endpoint
      for endpoint in py_endpoints:
        # get details for the script and uri path
        details = AVAILABLE_WKLD_ENDPOINTS[args['app']][endpoint]
        script_path = app_dir.joinpath(details['script_path'])

        # run workload for each endpoint
        python[script_path] & FG

    exit()

  except KeyboardInterrupt:
    # if the compose gets interrupted we just continue with the script
    pass


#############################
# GATHER
#
def gather(args):
  if args['visibility_latency']:
    # visit http://146.193.41.235:16686/dependencies to check number of flowing requests
    limit = 7000
    next_round = 0
    traces = []

    # curl -X GET "jaeger:16686/api/traces?service=write-home-timeline-service&prettyPrint=true
    params = (
      ('service', 'write-home-timeline-service'),
      ('limit', limit),
      # ('offset', total_num), -- offset is not working
      ('lookback', '1h'),
      # ('prettyPrint', 'true'),
    )
    response = requests.get('http://localhost:16686/api/traces', params=params)

    # error if we do not get a 200 OK code
    if response.status_code != 200 :
      print("[ERROR] Could not fetch traces from Jaeger")
      exit(-1)

    # error if we do not retreive all traces
    if len(traces) == limit:
      print(f"[WARN] Fetched the same amount of traces as the limit ({limit}). Increase the limit to fetch all traces.")

    # read returned traces
    content = response.json()

    # pick only the traces with the desired info
    for trace in content['data']:
      trace_info = {
        'trace_id': trace['spans'][0]['traceID'],
        'wht_worker_duration': -1,
        'wht_worker_endts': -1,
      }

      for s in trace['spans']:
        for t in s['tags']:
          if t['key'] == 'wht_worker_duration':
            trace_info['wht_worker_duration'] = float(t['value'])
          if t['key'] == 'wht_worker_endts':
            # v = datetime.fromtimestamp(v / 1000.0)
            trace_info['wht_worker_endts'] = t['value']

      traces.append(trace_info)

    df = pd.DataFrame(traces)
    df = df.set_index('trace_id')
    print(df.describe())


#############################
# MAIN
#
if __name__ == "__main__":
  import argparse

  # parse arguments
  main_parser = argparse.ArgumentParser()
  main_parser.add_argument("app", choices=AVAILABLE_APPLICATIONS, help="Application to deploy")
  subparsers = main_parser.add_subparsers(help='commands', dest='which')

  # build application
  build_parser = subparsers.add_parser('build', help='Build application')
  build_parser.add_argument('--strong', action='store_true', help="rebuild all containers")

  # clean application
  clean_parser = subparsers.add_parser('clean', help='Clean application')
  clean_parser.add_argument('-s', '--strong', action='store_true', help="delete images")
  clean_parser.add_argument('-j', '--jaeger', action='store_true', help="delete jaeger container")

  # run application
  run_parser = subparsers.add_parser('run', help='Run application')
  run_parser.add_argument('-d', '--detached', action='store_true', help="detached")
  run_parser.add_argument('--build', action='store_true', help="build")

  # deploy application
  deploy_parser = subparsers.add_parser('deploy', help='Deploy application')
  # deploy type group
  deploy_type_group = deploy_parser.add_mutually_exclusive_group(required=True)
  deploy_type_group.add_argument('--gcp',action='store_true', help='Deploy app to GCP')
  deploy_type_group.add_argument('--gsd',action='store_true', help='Deploy app to GSD cluster')
  # deploy file group
  deploy_file_group = deploy_parser.add_mutually_exclusive_group(required=True)
  deploy_file_group.add_argument('-n', '--new', action='store_true', help="Build a new deploy file")
  deploy_file_group.add_argument('-l', '--lastest', action='store_true', help="Use last used deploy file")
  deploy_file_group.add_argument('-f', '--file', type=argparse.FileType('r', encoding='UTF-8'), help="Use specific file")

  # workload application
  wkld_parser = subparsers.add_parser('wkld', help='Run HTTP workload generator')
  # comparable with wrk2 > ./wrk options
  wkld_parser.add_argument('-E', '--endpoints', nargs='+', choices=[ e for app_list in AVAILABLE_WKLD_ENDPOINTS.values() for e in app_list ], help="Endpoints to generate workload for")
  wkld_parser.add_argument('-c', '--connections', type=int, default=1, help="Connections to keep open")
  wkld_parser.add_argument('-d', '--duration', type=str, default='1m', help="Duration in s / m / h")
  wkld_parser.add_argument('-t', '--threads', type=int, default=1, help="Number of threads")
  wkld_parser.add_argument('-r', '--requests', type=int, default=1, required=True, help="Work rate (throughput) in request per second total")
  # Existing options:
  # -D, --dist             fixed, exp, norm, zipf
  # -P                     Print each request's latency
  # -p                     Print 99th latency every 0.2s to file
  # -L  --latency          Print latency statistics

  # gather application
  gather_parser = subparsers.add_parser('gather', help='Gather data from application')
  gather_parser.add_argument('-vl', '--visibility-latency', action='store_true', help="detached")


  args = vars(main_parser.parse_args())
  command = args.pop('which')

  # change application dir if necessary
  if Path.cwd().name != args['app']:
    os.chdir(Path.cwd().joinpath(args['app']))

  # call parser method dynamically
  getattr(sys.modules[__name__], command)(args)
