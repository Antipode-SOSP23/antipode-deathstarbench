#!/usr/bin/env python3

import os
import sys
import stat
import re
from pprint import pprint
from pathlib import Path
from plumbum import local
from plumbum import FG, BG
from plumbum.cmd import docker_compose, docker, ansible_playbook
from plumbum.cmd import make
from plumbum.cmd import python3
import itertools
import urllib.parse
import requests
from datetime import datetime
import pandas as pd
import click
import time
import yaml
import glob
from jinja2 import Environment
import textwrap
from shutil import copyfile

#############################
# Pre-requisites
#
# > sudo apt-get install libssl-dev libz-dev lua5.1 lua5.1-dev luarocks python3
# > sudo luarocks install luasocket
# > sudo luarocks install json-lua
# > sudo luarocks install penlight
# > pip install plumbum ansible
#

ROOT_PATH = Path(os.path.abspath(os.path.dirname(sys.argv[0])))

# pandas global settings
pd.set_option('display.float_format', lambda x: '%.3f' % x)

#############################
# HELPER
#
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

#############################
# CONSTANTS
#
# available deploys
AVAILABLE_DEPLOY_TYPES = {
  'local': { 'name': 'Localhost' },
  'gsd': { 'name': 'GSD Cluster' },
}
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
    'init-social-graph': {
      'type': 'python',
      'script_path': './scripts/init_social_graph.py',
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
SWARM_MANAGER_NODE = { 'node34': '10.100.0.44' }

# socialNetwork
SOCIAL_NETWORK_DEFAULT_SERVICES = {
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
  'nginx-thrift-us': 'HOSTNAME',
  'jaeger': 'HOSTNAME',
  'xtrace-server': 'HOSTNAME',
  'mongodb-admin': 'HOSTNAME',
  'post-storage-mongodb-setup': 'HOSTNAME',
}

#############################
# BUILD
#
def build(args):
  try:
    app_dir = Path.cwd()
    getattr(sys.modules[__name__], f"build__{args['app']}__{_deploy_type(args)}")(args)
    os.chdir(app_dir)
  except KeyboardInterrupt:
    # if the compose gets interrupted we just continue with the script
    pass


def build__socialNetwork__local(args):
  app_dir = Path.cwd()

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
  os.chdir(app_dir)

  # Build the nginx server image. We modified this to add X-Trace and protocol buffers
  os.chdir(app_dir.joinpath('docker', 'openresty-thrift'))
  docker[openresty_thrift_args] & FG
  os.chdir(app_dir)

  # Build the mongodb setup image
  os.chdir(app_dir.joinpath('docker', 'mongodb-setup', 'post-storage'))
  docker['build', '-t', 'mongodb-setup', '.'] & FG
  os.chdir(app_dir)

  # Build the wrk2 image
  os.chdir(app_dir.joinpath('docker', 'wrk2'))
  docker['build', '-t', 'wrk2:antipode', '.'] & FG
  os.chdir(app_dir)

  # Build the redis-im image
  os.chdir(app_dir.joinpath('docker', 'redis-im'))
  docker['build', '-t', 'redis-im:antipode', '.'] & FG
  os.chdir(app_dir)

  # Build the social network docker image
  docker['build', '-t', 'yg397/social-network-microservices:antipode', '.'] & FG

def build__socialNetwork__gsd(args):
  # change path to playbooks folder
  app_dir = Path.cwd()
  os.chdir(app_dir.joinpath('..', 'deploy', 'playbooks'))

  ansible_playbook['containers-build.yml', '-e', 'app=socialNetwork'] & FG
  ansible_playbook['containers-backup.yml', '-e', 'app=socialNetwork'] & FG


#############################
# DEPLOY
#
def deploy(args):
  try:
    app_dir = Path.cwd()
    getattr(sys.modules[__name__], f"deploy__{args['app']}__{_deploy_type(args)}")(args)
    os.chdir(app_dir)
  except KeyboardInterrupt:
    # if the compose gets interrupted we just continue with the script
    pass

def deploy__socialNetwork__local(args):
  return None

def deploy__socialNetwork__gsd(args):
  app_dir = Path.cwd()
  os.chdir(app_dir.joinpath('..', 'deploy'))

  # base path of configuration file
  conf_base_path = Path(Path.cwd(), 'configurations')
  if args['new']:
    new_filename = f"socialNetwork-gsd-{time.strftime('%Y%m%d%H%M%S')}.yml"
    filepath = conf_base_path / new_filename

    # write default configuration to file
    with open(filepath, 'w') as f_conf:
      yaml.dump(SOCIAL_NETWORK_DEFAULT_SERVICES, f_conf)
      print(f"\t [SAVED] '{filepath.abspath()}'")

    # wait for editor to close
    print("[INFO] Waiting for editor to close new configuration file ...")
    click.edit(filename=filepath)

    # check if all nodes are known
    with open(filepath, 'r') as f_conf:
      conf = yaml.load(f_conf, Loader=yaml.FullLoader)
      unknonwn_nodes = [ n for n in set(conf.values()) if n not in AVAILABLE_NODES.keys() ]
      if unknonwn_nodes:
        print("[ERROR] Found unknown nodes in GSD cluster: " + ', '.join(unknonwn_nodes))
        exit()

  if args['latest']:
    filepath = _last_configuration('socialNetwork', 'gsd')
  if args['file']:
    filepath = ROOT_PATH / args['file'].name

  # Update docker-compose.yml
  deploy_nodes = {}
  with open(filepath, 'r') as f_conf, open(Path(app_dir, 'docker-compose.yml'), 'r') as f_compose:
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
  new_compose_filepath = Path('docker-compose-swarm.yml')
  with open(new_compose_filepath, 'w') as f_compose:
    yaml.dump(compose, f_compose)
  print(f"\t [SAVED] '{new_compose_filepath.resolve()}'")

  # copy file to socialNetwork folder
  end_docker_compose_swarm = Path(app_dir, 'docker-compose-swarm.yml').resolve()
  copyfile(new_compose_filepath.resolve(), end_docker_compose_swarm)
  print(f"\t [INFO] Copied '{str(new_compose_filepath.resolve()).split('antipode-deathstarbench/')[1]}' to '{str(end_docker_compose_swarm).split('antipode-deathstarbench/')[1]}'")

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
    'deploy_nodes': dict(sorted(deploy_nodes.items())),
  })

  inventory_filepath = Path('playbooks/inventory.cfg')
  with open(inventory_filepath, 'w') as f:
    # remove empty lines and dedent for easier read
    f.write(textwrap.dedent(inventory))

  print(f"\t [SAVED] '{inventory_filepath.resolve()}'")

  # run playbooks
  os.chdir(app_dir.joinpath('..', 'deploy', 'playbooks'))
  # first restore all images to everyone
  ansible_playbook['containers-restore.yml', '-e', 'app=socialNetwork'] & FG
  # then deploy everywhere
  ansible_playbook['deploy-swarm.yml', '-e', 'app=socialNetwork'] & FG

  print("[INFO] Deploy Complete!")


#############################
# RUN
#
def run(args):
  try:
    app_dir = Path.cwd()
    getattr(sys.modules[__name__], f"run__{args['app']}__{_deploy_type(args)}")(args)
    os.chdir(app_dir)
  except KeyboardInterrupt:
    # if the compose gets interrupted we just continue with the script
    pass

def run__socialNetwork__local(args):
  run_args = ['up']
  # run containers in detached mode
  if args['detached']:
    run_args.insert(1, '-d')
  if args['build']:
    run_args.insert(1, '--build')

  # Fixes error: "WARNING: Connection pool is full, discarding connection: localhost"
  # ref: https://github.com/docker/compose/issues/6638#issuecomment-576743595
  with local.env(COMPOSE_PARALLEL_LIMIT=99):
    docker_compose[run_args] & FG

def run__socialNetwork__gsd(args):
  # change path to playbooks folder
  app_dir = Path.cwd()
  os.chdir(app_dir.joinpath('..', 'deploy', 'playbooks'))

  ansible_playbook['start-portainer.yml'] & FG
  print(f"[INFO] Portainer link (u/pwd: admin/antipode): http://{next(iter(SWARM_MANAGER_NODE))}:9000 ")

  ansible_playbook['start-dsb.yml', '-e', 'app=socialNetwork'] & FG
  print("[INFO] Run Complete!")


#############################
# CLEAN
#
def clean(args):
  try:
    app_dir = Path.cwd()
    getattr(sys.modules[__name__], f"clean__{args['app']}__{_deploy_type(args)}")(args)
    os.chdir(app_dir)
  except KeyboardInterrupt:
    # if the compose gets interrupted we just continue with the script
    pass

def clean__socialNetwork__local(args):
  app_dir = Path.cwd()
  # first stops the containers
  docker_compose['stop'] & FG

  if args['strong']:
    docker_compose['down', '--rmi', 'all', '--remove-orphans'] & FG
  elif args['jaeger']:
    docker_compose['rm', '-f', 'jaeger'] & FG
  else:
    docker_compose['down'] & FG

def clean__socialNetwork__gsd(args):
  # change path to playbooks folder
  app_dir = Path.cwd()
  os.chdir(app_dir.joinpath('..', 'deploy', 'playbooks'))

  ansible_playbook['undeploy-swarm.yml', '-e', 'app=socialNetwork'] & FG
  print("[INFO] Clean Complete!")


#############################
# WORKLOAD
#
def wkld(args):
  try:
    app_dir = Path.cwd()
    # get host for each zone
    hosts = getattr(sys.modules[__name__], f"wkld__{args['app']}__{_deploy_type(args)}__hosts")(args)

    # scripts just need you to set the env variables
    if args['app'] == 'socialNetwork':
      endpoint = AVAILABLE_WKLD_ENDPOINTS[args['app']][args['Endpoint']]

      if endpoint['type'] == 'wrk2':
        # build arguments
        wrk_args = []
        if 'connections' in args:
          wrk_args.extend(['--connections', args['connections']])
        if 'duration' in args:
          wrk_args.extend(['--duration', f"{args['duration']}s"])
        if 'threads' in args:
          wrk_args.extend(['--threads', args['threads']])

        # get details for the script and uri path
        script_path = app_dir.joinpath(endpoint['script_path'])
        script_path = '/scripts/' + str(script_path).split('wrk2/scripts/')[1]
        uri = urllib.parse.urljoin(hosts['host_eu'], endpoint['uri'])
        # add arguments to previous list
        wrk_args.extend(['--latency', '--script', str(script_path), uri, '--rate', args['requests'] ])

        # add docker arguments
        wrk_args = ['run',
          '--rm', '-it',
          '--network=host',
          '-e', f"HOST_EU={hosts['host_eu']}",
          '-e', f"HOST_US={hosts['host_us']}",
          '-v', f"{app_dir.joinpath('wrk2','scripts')}:/scripts",
          'wrk2:antipode',
          './wrk'
        ] + wrk_args

        exe_path = 'docker'
        exe_args = wrk_args

      elif endpoint['type'] == 'python':
        script_path = app_dir.joinpath(endpoint['script_path'])
        exe_path = 'python3'
        exe_args = [script_path]

      os.chdir(app_dir)
      # run workload for endpoint
      with local.env(HOST_EU=hosts['host_eu']), local.env(HOST_US=hosts['host_us']):
        getattr(sys.modules[__name__], f"wkld__{args['app']}__{_deploy_type(args)}__run")(args, hosts, exe_path, exe_args)

    os.chdir(app_dir)
  except KeyboardInterrupt:
    # if the compose gets interrupted we just continue with the script
    pass

def wkld__socialNetwork__local__hosts(args):
  return {
    'host_eu': 'http://127.0.0.1:8080',
    'host_us': 'http://127.0.0.1:8082',
  }

def wkld__socialNetwork__gsd__hosts(args):
  # eu - nginx-thrift: node23
  # us - nginx-thrift-us: node24§

  if args['latest']:
    filepath = _last_configuration('socialNetwork', 'gsd')
  if args['file']:
    filepath = ROOT_PATH / args['file'].name

  with open(filepath, 'r') as f_conf:
    conf = yaml.load(f_conf, Loader=yaml.FullLoader)
    return {
      'host_eu': f"http://{conf['nginx-thrift']}:8080",
      'host_us': f"http://{conf['nginx-thrift-us']}:8082",
    }

def wkld__socialNetwork__local__run(args, hosts, exe_path, exe_args):
  exe = local[exe_path]
  exe[exe_args] & FG

def wkld__socialNetwork__gsd__run(args, hosts, exe_path, exe_args):
  if not args['node']:
    print("[INFO] No nodes passed onto gsd deployment workload. Using current node with remote hosts.")
    wkld__socialNetwork__local__run(args, hosts, exe_path, exe_args)
    return

  app_dir = Path.cwd()
  os.chdir(app_dir.joinpath('..', 'deploy'))

  if args['latest']:
    conf_filepath = Path(_last_configuration('socialNetwork', 'gsd'))
  if args['file']:
    conf_filepath = ROOT_PATH / args['file'].name

  # Create inventory for clients
  template = """
    [clients]
    {% for k,v in client_nodes.items() %}{{k}} ansible_host={{v}} ansible_user=jfloff ansible_ssh_private_key_file=~/.ssh/id_rsa_inesc_cluster_jfloff
    {% endfor %}
  """
  inventory = Environment().from_string(template).render({
    'client_nodes': { n:AVAILABLE_NODES[n] for n in set(args['node']) },
  })
  inventory_filepath = Path('playbooks/clients_inventory.cfg')
  with open(inventory_filepath, 'w') as f:
    # remove empty lines and dedent for easier read
    f.write(textwrap.dedent(inventory))

  print(f"\t [SAVED] '{inventory_filepath.resolve()}'")

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
  script_filepath = Path('playbooks/wkld-start.sh')
  with open(script_filepath, 'w') as f:
    # remove empty lines and dedent for easier read
    f.write(textwrap.dedent(script))
  # add executable permissions
  script_filepath.chmod(script_filepath.stat().st_mode | stat.S_IEXEC)

  print(f"\t [SAVED] '{script_filepath.resolve()}'")

  # change path to playbooks folder
  os.chdir(app_dir.joinpath('..', 'deploy', 'playbooks'))
  ansible_playbook['wkld-start.yml', '-i', 'clients_inventory.cfg', '-e', 'app=socialNetwork'] & FG

  # sleep before gather
  if args['duration'] is not None:
    sleep_for = args['duration'] + 60
    print(f"[INFO] Sleeping for {sleep_for} seconds ...")
    time.sleep(sleep_for)
  else:
    input("Press any key when done ...")

  ansible_playbook['wkld-gather.yml', '-i', 'clients_inventory.cfg', '-e', 'app=socialNetwork', '-e', f'conf_id={conf_id}' ] & FG

#############################
# GATHER
#
def _fetch_span_tag(tags, tag_to_search):
  return next(item for item in tags if item['key'] == tag_to_search)['value']

def gather(args):
  try:
    app_dir = Path.cwd()
    # get host for each zone
    jaeger_host = getattr(sys.modules[__name__], f"gather__{args['app']}__{_deploy_type(args)}")(args)

    limit = int(input(f"Visit {jaeger_host}/dependencies to check number of flowing requests: "))
    traces = []

    if args['antipode_ts']:
      # curl -X GET "jaeger:16686/api/traces?service=write-home-timeline-service&prettyPrint=true
      params = (
        ('service', 'antipode-oracle'),
        ('limit', limit),
        ('lookback', '1h'),
        # ('prettyPrint', 'true'),
      )
      response = requests.get(f'{jaeger_host}/api/traces', params=params)

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
      i = 0
      for trace in content['data']:
        i += 1
        if i % 1000 == 0: print(f"\t{i}/{limit}", flush=True)

        trace_info = {
          # 'trace_id': trace['spans'][0]['traceID'],
          'post_id': -1,
          'antipode_isvisible_duration': -1,
          'antipode_isvisible_attempts': -1,
          'wht_antipode_duration': -1,
          'wht_worker_duration': -1,
          'wht_worker_per_antipode': -1,
        }

        # search trace info in different spans
        for s in trace['spans']:
          if s['operationName'] == '_ComposeAndUpload':
            trace_info['post_id'] = int(_fetch_span_tag(s['tags'], 'composepost_id'))

          if s['operationName'] == 'IsVisible':
            trace_info['antipode_isvisible_duration'] = float(_fetch_span_tag(s['tags'], 'antipode_isvisible_duration'))
            trace_info['antipode_isvisible_attempts'] = int(_fetch_span_tag(s['tags'], 'antipode_isvisible_attempts'))

          if s['operationName'] == 'FanoutHomeTimelines':
            trace_info['wht_antipode_duration'] = float(_fetch_span_tag(s['tags'], 'wht_antipode_duration'))
            trace_info['wht_worker_duration'] = float(_fetch_span_tag(s['tags'], 'wht_worker_duration'))

        # percentage of time spent in antipode
        trace_info['wht_worker_per_antipode'] = trace_info['wht_antipode_duration'] / trace_info['wht_worker_duration']
        traces.append(trace_info)

      df = pd.DataFrame(traces)
      df = df.set_index('post_id')
      print(df.describe())

    elif args['visibility_latency']:
      # curl -X GET "jaeger:16686/api/traces?service=write-home-timeline-service&prettyPrint=true
      params = (
        ('service', 'post-storage-service'),
        ('limit', limit),
        ('lookback', '1h'),
        # ('prettyPrint', 'true'),
      )
      response = requests.get(f'{jaeger_host}/api/traces', params=params)

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
      i = 0
      for trace in content['data']:
        i += 1
        if i % 1000 == 0: print(f"\t{i}/{limit}", flush=True)

        trace_info = {
          # 'trace_id': trace['spans'][0]['traceID'],
          'post_id': -1,
          'poststorage_post_visible': -1,
          'wht_notification_ts': -1,
          'post_notification_diff_ms': -1,
        }

        # search trace info in different spans
        for s in trace['spans']:
          if s['operationName'] == '_ComposeAndUpload':
            trace_info['post_id'] = int(_fetch_span_tag(s['tags'], 'composepost_id'))

          if s['operationName'] == 'FanoutHomeTimelines':
            trace_info['wht_notification_ts'] = int(_fetch_span_tag(s['tags'], 'wht_notification_ts'))

          if s['operationName'] == 'StorePost':
            trace_info['poststorage_post_visible'] = int(_fetch_span_tag(s['tags'], 'poststorage_post_visible'))

        # computes the different in ms from post to notification
        diff = datetime.fromtimestamp(trace_info['poststorage_post_visible']/1000.0) - datetime.fromtimestamp(trace_info['wht_notification_ts']/1000.0)
        trace_info['post_notification_diff_ms'] = float(diff.total_seconds() * 1000)

        traces.append(trace_info)

      df = pd.DataFrame(traces)
      df = df.set_index('post_id')
      print(df.describe())
      print("")
      num_posts_before_notifications = len([n for n in df['post_notification_diff_ms'] if n >= 0])
      num_notifications_before_posts = len([n for n in df['post_notification_diff_ms'] if n < 0])
      print(f"% posts ready before notifications: {num_posts_before_notifications/float(len(df))}")
      print(f"% notifications ready before posts: {num_notifications_before_posts/float(len(df))}")

  except KeyboardInterrupt:
    # if the compose gets interrupted we just continue with the script
    pass

def gather__socialNetwork__local(args):
  return 'http://localhost:16686'

def gather__socialNetwork__gsd(args):
  filepath = None

  if args['latest']:
    filepath = _last_configuration('socialNetwork', 'gsd')
  if args['file']:
    filepath = ROOT_PATH / args['file'].name

  with open(filepath, 'r') as f_conf:
    conf = yaml.load(f_conf, Loader=yaml.FullLoader)
    return f"http://{conf['jaeger']}:16686"

#############################
# MAIN
#
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
  clean_parser.add_argument('-j', '--jaeger', action='store_true', help="delete jaeger container")

  # run application
  run_parser = subparsers.add_parser('run', help='Run application')
  run_parser.add_argument('-d', '--detached', action='store_true', help="detached")
  run_parser.add_argument('--build', action='store_true', help="build")

  # deploy application
  deploy_parser = subparsers.add_parser('deploy', help='Deploy application')
  # deploy file group
  deploy_file_group = deploy_parser.add_mutually_exclusive_group(required=True)
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
  # different metics to gather
  gather_parser.add_argument('-vl', '--visibility-latency', action='store_true', help="gather visibility latency information")
  gather_parser.add_argument('-ats', '--antipode-ts', action='store_true', help="gathers antipode timestamps and duration information")


  args = vars(main_parser.parse_args())
  command = args.pop('which')

  # change application dir if necessary
  if Path.cwd().name != args['app']:
    os.chdir(Path.cwd().joinpath(args['app']))

  # call parser method dynamically
  getattr(sys.modules[__name__], command)(args)
