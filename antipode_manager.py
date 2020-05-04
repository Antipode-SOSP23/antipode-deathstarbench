#!/usr/bin/env python3

import os
import sys
from pprint import pprint
from pathlib import Path
from plumbum import local
from plumbum import FG, BG
from plumbum.cmd import docker_compose, docker
from plumbum.cmd import make
import itertools
import urllib.parse


#############################
# Pre-requisites
#
# > sudo apt-get install libssl-dev libz-dev luarocks python3
# > sudo luarocks install luasocket
# > pip install plumbum
#

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
      'uri': 'wrk2-api/post/compose',
      'script_path': './wrk2/scripts/social-network/compose-post.lua',
    },
    'read-home-timeline': {
      'uri': 'wrk2-api/home-timeline/read',
      'script_path': './wrk2/scripts/social-network/read-home-timeline.lua',
    },
    'read-user-timeline': {
      'uri': 'wrk2-api/user-timeline/read',
      'script_path': './wrk2/scripts/social-network/read-user-timeline.lua',
    },
  }
}

#############################
# BUILD
#
def build(args):
  app_dir = Path.cwd()
  try:
    # By default, the DeathStarBench pulls its containers from docker hub.
    # We need to override these with our modified X-Trace containers.
    # To do this, we will manually build the docker images for the modified components.

    thrift_microservice_args = ['build', '-t', 'yg397/thrift-microservice-deps', '.']
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
    docker['build', '-t', 'yg397/social-network-microservices', '.'] & FG

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

    docker_compose[run_args] & FG

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
    for endpoint in args['endpoints']:
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

  except KeyboardInterrupt:
    # if the compose gets interrupted we just continue with the script
    pass


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
  clean_parser.add_argument('--strong', action='store_true', help="delete images")

  # run application
  run_parser = subparsers.add_parser('run', help='Run application')
  run_parser.add_argument('-d', '--detached', action='store_true', help="detached")

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


  args = vars(main_parser.parse_args())
  command = args.pop('which')

  # change application dir if necessary
  if Path.cwd().name != args['app']:
    os.chdir(Path.cwd().joinpath(args['app']))

  # call parser method dynamically
  getattr(sys.modules[__name__], command)(args)