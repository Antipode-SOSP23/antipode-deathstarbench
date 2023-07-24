#!/usr/bin/env python3

import os
from plumbum import local, path, FG
from datetime import datetime
import time
from pprint import pprint as pp
from pathlib import Path
import sys
import stat

#-----------------
# HELPER
#-----------------
def _load_yaml(path):
  import ruamel.yaml
  with open(path, 'r') as f:
    yaml = ruamel.yaml.YAML()
    yaml.preserve_quotes = True
    return yaml.load(f)  or {}

def _dump_yaml(path, d):
  from ruamel.yaml import YAML
  yaml=YAML()
  path.parent.mkdir(exist_ok=True, parents=True)
  with open(path, 'w+') as f:
    yaml.default_flow_style = False
    yaml.dump(d, f)

def _put_last(deploy_type,k,v):
  doc = {}
  # if file exists parse yaml otherwise create empty dict to write to
  if Path(LAST_DEPLOY_FILE[deploy_type]).exists():
    doc = _load_yaml(LAST_DEPLOY_FILE[deploy_type])
  # write new value and save to file
  doc[k] = v
  # create path dirs if needed
  _dump_yaml(LAST_DEPLOY_FILE[deploy_type],doc)

def _get_last(deploy_type,k):
  # if file does not exist create empty dict to write to
  if Path(LAST_DEPLOY_FILE[deploy_type]).exists():
    doc = _load_yaml(LAST_DEPLOY_FILE[deploy_type])
  else:
    # create file with default entries
    doc = {
      'cleaned': True
    }
    _dump_yaml(LAST_DEPLOY_FILE[deploy_type],doc)

  # default last files entries
  return doc.get(k)

def _get_config(deploy_type, k):
  doc = _load_yaml(ROOT_PATH / deploy_type / 'config.yml')
  return doc.get(k)

def _deploy_dir(args):
  return DEPLOY_PATH / args['deploy_type'] / args['app'] / args['tag']

def _service_ip(deploy_type, app, service):
  config = _load_yaml(ROOT_PATH / _get_last(args['deploy_type'],'config'))
  tag = _get_last(deploy_type, 'tag')

  if deploy_type == 'local':
    from plumbum.cmd import hostname
    public_ip = hostname['-I']().split()[0]
  elif deploy_type == 'gsd':
    public_ip = GSD_AVAILABLE_NODES[config['services'][service]]
  elif deploy_type == 'gcp':
    inventory = _load_inventory(DEPLOY_PATH / 'gcp' / app / tag / 'inventory.cfg')
    services_ips = {
      'jaeger': inventory[config['services']['jaeger']]['external_ip'],
      'rabbitmq-eu': inventory[config['services']['write-home-timeline-rabbitmq-eu']]['external_ip'],
      'rabbitmq-us': inventory[config['services']['write-home-timeline-rabbitmq-us']]['external_ip'],
      'portainer': inventory['manager-dsb']['external_ip'],
      'prometheus': inventory['manager-dsb']['external_ip'],
      'frontend-eu': inventory[config['services']['nginx-thrift']]['external_ip'],
      'frontend-us': inventory[config['services']['nginx-thrift-us']]['external_ip'],
    }
    public_ip = services_ips[service]
  # return the ip with the common port
  return f'http://{public_ip}:{SERVICE_PORTS[app][service]}'

def _wrk2_params(args, endpoint, main_host):
  import urllib.parse

  params = []
  # optional arguments
  if 'connections' in args:
    params.extend(['--connections', args['connections']])
  if 'duration' in args:
    params.extend(['--duration', f"{args['duration']}s"])
  if 'threads' in args:
    params.extend(['--threads', args['threads']])
  # add rate --> requests per second
  params.extend(['--rate', args['rate']])
  # we want latency by default
  params.append('--latency')
  # we add the script -- relative to wrk2 folder
  params.extend(['--script', './' + endpoint['script_path'].split('wrk2/scripts/')[1]])
  # url host
  params.append(urllib.parse.urljoin(main_host, endpoint['uri']))
  return params

def _index_containing_substring(the_list, substring):
  for i, s in enumerate(the_list):
    if substring in s:
      return i
  return -1

def _flat_list(l):
  return [item for sublist in l for item in sublist]

def _is_inside_docker():
  return os.path.isfile('/.dockerenv')

# Set env var REBUILD_GCP_DOCKER_IMAGE=1 to force rebuilt
def _force_gcp_docker():
  if not _is_inside_docker():
    import platform
    import subprocess
    from plumbum.cmd import docker

    # if image is not built, we do it
    if bool(int(os.environ.get('REBUILD_GCP_DOCKER_IMAGE',0))) or docker['images', GCP_DOCKER_IMAGE_NAME, '--format', '"{{.ID}}"']().strip() == '':
      with local.cwd(ROOT_PATH / 'gcp'):
        docker['build', '-t', GCP_DOCKER_IMAGE_NAME, '.'] & FG

    args = list()
    args.extend(['docker', 'run', '--rm', '-it',
      # env variables
      '-e', f"HOST_ROOT_PATH={ROOT_PATH}",
      # run docker from host inside the container
      '-v', '/var/run/docker.sock:/var/run/docker.sock',
      '-v', '/usr/bin/docker:/usr/bin/docker',
      # mount code volumes
      '-v', f"{ROOT_PATH}:/code",
      '-v', f"{ROOT_PATH / 'gcp' / '.ssh'}:/root/.ssh",
      '-w', '/code',
      GCP_DOCKER_IMAGE_NAME
    ])
    # force first argument to be a relative path -- important for plumbum local exec
    sys.argv[0] = './' + sys.argv[0].rpartition('/')[-1]
    # append arguments
    args = args + sys.argv
    # DEBUG:
    # print(' '.join(args)); exit()
    subprocess.call(args)
    exit()

def _gcp_vm_create(zone, config):
  import googleapiclient.discovery
  import json

  compute = googleapiclient.discovery.build('compute', 'v1')
  try:
    compute.instances().insert(project=GCP_PROJECT_ID, zone=zone, body=config).execute()
    i = _gcp_vm_wait_for_status(zone, config['name'], 'RUNNING')
    print(f"[INFO] Instance '{config['name']}' created")
    return i
  except googleapiclient.errors.HttpError as e:
    error_info = json.loads(e.args[1])['error']
    if error_info['code'] == 409 and error_info['errors'][0]['reason'] == 'alreadyExists':
      print(f"[WARN] Instance '{config['name']}' already exists")
      # get the existing instance details
      return _gcp_vm_get(zone=zone, name=config['name'])
    else:
      pp(error_info)
      exit(-1)

# Status available:
#   PROVISIONING, STAGING, RUNNING, STOPPING, SUSPENDING, SUSPENDED, REPAIRING, TERMINATED
def _gcp_vm_wait_for_status(zone, name, status):
  import googleapiclient.discovery
  import json

  while True:
    try:
      r = _gcp_vm_get(zone=zone, name=name)
      if r['status'] == status.upper():
        time.sleep(1)
        return r
    except googleapiclient.errors.HttpError as e:
      error_info = json.loads(e.args[1])['error']
      if error_info['code'] == 404:
        time.sleep(1)
      else:
        raise e

def _gcp_vm_get(zone, name):
  import googleapiclient.discovery
  compute = googleapiclient.discovery.build('compute', 'v1')
  return compute.instances().get(project=GCP_PROJECT_ID, zone=zone, instance=name).execute()

def _gcp_vm_start(zone, name):
  import googleapiclient.discovery
  import json

  compute = googleapiclient.discovery.build('compute', 'v1')
  while True:
    try:
      # start the vm if not running
      compute.instances().start(project=GCP_PROJECT_ID, zone=zone, instance=name).execute()
      # wait for start to be completed
      return _gcp_vm_wait_for_status(zone, name, 'RUNNING')
    except googleapiclient.errors.HttpError as e:
      error_info = json.loads(e.args[1])['error']
      raise(e)

def _gcp_vm_wait_for_ip(zone, name):
  while True:
    metadata = _gcp_vm_get(zone, name)
    try:
      network_interface = metadata['networkInterfaces'][0]
      return network_interface['accessConfigs'][0]['natIP'], network_interface['networkIP']
    except KeyError:
      time.sleep(1)

def _gcp_vm_delete(zone, name):
  import googleapiclient.discovery
  import json

  compute = googleapiclient.discovery.build('compute', 'v1')
  try:
    compute.instances().delete(project=GCP_PROJECT_ID, zone=zone, instance=name).execute()
    # wait for delete to be completed
    while True:
      _gcp_vm_get(zone, name)
      time.sleep(1)
  except googleapiclient.errors.HttpError as e:
    error_info = json.loads(e.args[1])['error']
    if error_info['code'] == 404:
      print(f"[INFO] Instance '{name}' deleted")
      return
    else:
      raise(e)

def _reverse_dict(d):
  rev = {}
  for k,vs in d.items():
    # if not a list, make it a list with a single entry
    if not isinstance(vs, list):
      vs = [vs]
    # loop list of values
    for v in vs:
      if v not in rev:
        rev[v] = []
      rev[v].append(k)
  return rev

def _load_inventory(filepath):
  from ansible.parsing.dataloader import DataLoader
  from ansible.inventory.manager import InventoryManager
  from ansible.vars.manager import VariableManager

  loader = DataLoader()
  inventory = InventoryManager(loader=loader, sources=str(filepath))
  variable_manager = VariableManager(loader=loader, inventory=inventory)

  hostname_to_groups = _reverse_dict(inventory.get_groups_dict())
  # other methods:
  # - inventory.get_host('manager.antipode-296620').vars

  loaded_inventory = {}
  for gcp_hostname,info in inventory.hosts.items():
    # index by service name
    e = {}
    # load hostname manually since its the key for each host
    e['gcp_hostname'] = gcp_hostname
    e['groups'] = hostname_to_groups[gcp_hostname]
    # load all remaining keys
    for k,v in info.vars.items():
      e[k] = v
    # refactor some entries
    e['external_ip'] = e['ansible_host']
    e['internal_ip'] = e['gcp_host']
    del e['ansible_host']
    del e['gcp_host']
    # assign new entry
    loaded_inventory[info.vars['gcp_name']] = e

  return loaded_inventory

def _wait_url_up(url):
  import urllib.request
  while True:
    try:
      return_code = urllib.request.urlopen(url).getcode()
      if return_code == 200:
        return
    except urllib.error.URLError as e:
      pass

def _build_gather_tag():
  config_name = Path(_get_last(args['deploy_type'], 'config')).stem
  rate = _get_last(args['deploy_type'], 'wkld_rate')
  antipode = '-antipode' if _get_last(args['deploy_type'], 'antipode') else ''
  return f"{config_name}-{rate}{antipode}"

#-----------------
# BUILD
#-----------------
def build(args):
  getattr(sys.modules[__name__], f"build__{args['app']}__{args['deploy_type']}")(args)
  print(f"[INFO] {args['app']} @ {args['deploy_type']} built successfully!")

def build__socialNetwork__local(args):
  from plumbum.cmd import docker, docker_compose

  # By default, the DeathStarBench pulls its containers from docker hub.
  # We need to override these with our modified X-Trace containers.
  # To do this, we will manually build the docker images for the modified components.

  # Build the base docker image that contains all the dependent libraries.  We modified this to add X-Trace and protocol buffers.
  with local.cwd(args['app_dir'] / 'docker' / 'thrift-microservice-deps' / 'cpp'):
    docker['build',
      '--no-cache' if args['no_cache'] else None,
      '-t', 'yg397/thrift-microservice-deps:antipode',
      '.'
    ] & FG

  # Build the nginx server image. We modified this to add X-Trace and protocol buffers
  with local.cwd(args['app_dir'] / 'docker' / 'openresty-thrift'):
    docker['build',
      '--no-cache' if args['no_cache'] else None,
      '-f', 'xenial/Dockerfile',
      '-t', 'yg397/openresty-thrift:latest',
      '.'
    ] & FG

  # Build the mongodb-delayed setup image
  with local.cwd(args['app_dir'] / 'docker' / 'mongodb-delayed'):
    docker['build',
      '--no-cache' if args['no_cache'] else None,
      '-t', 'mongodb-delayed:4.4.6',
      '.'
    ] & FG

  # Build the mongodb setup image
  with local.cwd(args['app_dir'] / 'docker' / 'mongodb-setup' / 'post-storage'):
    docker['build',
      '--no-cache' if args['no_cache'] else None,
      '-t', 'mongodb-setup:4.4.6',
      '.'
    ] & FG

  # Build the rabbitmq setup image
  with local.cwd(args['app_dir'] / 'docker' / 'rabbitmq-setup' / 'write-home-timeline'):
    docker['build',
      '--no-cache' if args['no_cache'] else None,
      '-t', 'rabbitmq-setup:3.8',
      '.'
    ] & FG

  # Build the wrk2 image
  with local.cwd(args['app_dir'] / 'docker' / 'wrk2'):
    docker['build',
      '--no-cache' if args['no_cache'] else None,
      '-t', 'wrk2:antipode',
      '.'
    ] & FG

  # Build the python-wkld image
  with local.cwd(args['app_dir'] / 'docker' / 'python-wkld'):
    docker['build',
      '--no-cache' if args['no_cache'] else None,
      '-t', 'python-wkld:antipode',
      '.'
    ] & FG

  # Build the redis-im image
  with local.cwd(args['app_dir'] / 'docker' / 'redis-im'):
    docker['build',
      '--no-cache' if args['no_cache'] else None,
      '-t', 'redis-im:antipode',
      '.'
    ] & FG

  # Build the social network docker image
  with local.cwd(args['app_dir']):
    docker['build',
      '--no-cache' if args['no_cache'] else None,
      '-t', 'yg397/social-network-microservices:antipode',
      '.'
    ] & FG

  # Build docker compose images to download remaining images
  with local.cwd(args['app_dir']):
    docker_compose['build'] & FG

def build__socialNetwork__gsd(args):
  from plumbum.cmd import ansible_playbook

  # change path to playbooks folder
  os.chdir(ROOT_PATH / 'gsd')

  ansible_playbook['containers-build.yml', '-e', 'app=socialNetwork'] & FG
  ansible_playbook['containers-backup.yml', '-e', 'app=socialNetwork'] & FG

def build__socialNetwork__gcp(args):
  from plumbum.cmd import docker

  # build local images first
  if not args['skip_images']:
    if not _is_inside_docker():
      build__socialNetwork__local(args)

  # then force docker
  _force_gcp_docker()
  import googleapiclient.discovery
  from plumbum.cmd import gcloud, ls, rm
  import json
  from jinja2 import Environment
  import textwrap

  # tag images built localy with GCP tag
  if not args['skip_images']:
    for image in CONTAINERS_BUILT:
      # These images are tag with gcp namespace but then are retagged when deploying
      gcp_image_name = f"{GCP_DOCKER_IMAGE_NAMESPACE}/{image}"
      docker['tag', image, gcp_image_name] & FG
      docker['push', gcp_image_name] & FG
      docker['rmi', gcp_image_name] & FG

  # Create env file for base_vm script
  with local.cwd(ROOT_PATH / 'gcp'):
    template = """
    NAMESPACE={{ namespace }}
    """
    env_file = Environment().from_string(template).render({
      'namespace': GCP_DOCKER_IMAGE_NAMESPACE,
    })
    with open('base_vm_env', 'w') as f:
      # remove empty lines and dedent for easier read
      f.write(textwrap.dedent(env_file))
    print(f"[SAVED] 'base_vm_env'")

  # Create base VM
  compute = googleapiclient.discovery.build('compute', 'v1')
  image_response = compute.images().getFromFamily(project='debian-cloud', family='debian-10').execute()
  source_disk_image = image_response['selfLink']
  zone = 'us-east1-b'
  config = {
    'name': GCP_BUILD_IMAGE_NAME,
    'machineType': f"zones/{zone}/machineTypes/f1-micro",
    'disks': [
      {
        'boot': True,
        'autoDelete': True,
        'initializeParams': {
          'sourceImage': source_disk_image,
          'disk_size_gb': 50,
        }
      }
    ],
    'hostname': 'antipode-dev.dsb',
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
      "items": [],
    },
  }
  r = _gcp_vm_create(zone, config)
  _gcp_vm_start(zone, r['id'])
  time.sleep(10) # extra time to avoid ssh errors

  # delete any old keys ssh keys from older deployments
  with local.cwd(ROOT_PATH / 'gcp' / '.ssh'):
    rm['-f', 'google_compute_engine', 'google_compute_engine.pub', 'google_compute_known_hosts', 'known_hosts', 'known_hosts.old'] & FG
    # ls['-la'] & FG

  # In order to get the .ssh folder via GCP instance
  # 1) Start a local GCP container
  # 2) Go the the GCP console, and in the SSH context menu, copy the gcloud command, it will look something like:
  #    $ gcloud compute ssh --zone "europe-west3-c" "antipode-dev-tt" --project "pluribus"
  # 3) Paste the above command in the container and let it run
  # 4) Copy the files from the container to outside
  #       docker cp <CONTAINER ID>:/root/.ssh/google_compute_engine .
  #       docker cp <CONTAINER ID>:/root/.ssh/google_compute_engine.pub .

  # copy and execute base_vm script -- retry needed sometimes
  while True:
    try:
      # first connection to create ssh keys
      gcloud['compute', 'ssh', f"{GCP_DEFAULT_SSH_USER}@{GCP_BUILD_IMAGE_NAME}", '--command', 'echo "Connected!"'] & FG
      # copy key and scripts
      gcloud['compute', 'scp', 'gcp/.ssh/google_compute_engine.pub', 'gcp/base_vm_env', 'gcp/base_vm.sh', 'gcp/credentials.json', f"{GCP_DEFAULT_SSH_USER}@{GCP_BUILD_IMAGE_NAME}:/tmp/"] & FG
      gcloud['compute', 'ssh', f"{GCP_DEFAULT_SSH_USER}@{GCP_BUILD_IMAGE_NAME}", '--command', 'bash /tmp/base_vm.sh'] & FG
      # if got here everything worked and we break
      break
    except Exception as e:
      # known Connection closed error
      if '255' in str(e).split('\n')[0]:
        # give a bit of time before retying
        time.sleep(5)
      else:
        # not known exception we raise exception
        raise e

  # stop machine
  compute.instances().stop(project=GCP_PROJECT_ID, zone=zone, instance=r['id']).execute()
  _gcp_vm_wait_for_status(zone, r['id'], 'TERMINATED')

  # built image based on the image of this machine
  image_config = {
    "kind": "compute#image",
    "name": GCP_MACHINE_IMAGE_NAME,
    "sourceDisk": f"projects/pluribus/zones/us-east1-b/disks/{GCP_BUILD_IMAGE_NAME}",
    "storageLocations": [
      "us"
    ]
  }

  # check if there is already an image - delete if necessary
  try:
    compute.images().get(project=GCP_PROJECT_ID, image=image_config['name']).execute()
    # if no exception delete the image
    print("[INFO] Image already exists - deleting ...", flush=True)
    compute.images().delete(project=GCP_PROJECT_ID, image=image_config['name']).execute()
    # wait for image to be deleted
    while True:
      # 404 exceptions will result into going to except and pass - means we deleted the image sucessfully
      compute.images().get(project=GCP_PROJECT_ID, image=image_config['name']).execute()
  except googleapiclient.errors.HttpError as e:
    error_info = json.loads(e.args[1])['error']
    if error_info['code'] == 404:
      # wait a bit more time after 404 error
      time.sleep(5)
      print("[INFO] Image deleted!")
      pass
    else:
      raise(e)

  # create the new image
  compute.images().insert(project=GCP_PROJECT_ID, body=image_config).execute()
  # wait for image to be ready
  while True:
    if compute.images().get(project=GCP_PROJECT_ID, image=image_config['name']).execute()['status'] == 'READY':
      break

  # find all used ports
  firewall_rules = compute.firewalls().list(project=GCP_PROJECT_ID, filter=None).execute()['items']
  for frule in firewall_rules:
    if frule['name'] == 'portainer':
      print("[INFO] Firewall rule for portainer found")
    elif frule['name'] == 'swarm':
      print("[INFO] Firewall rule for swarm found")
    elif frule['name'] == 'nodes':
      print("[INFO] Firewall rule for nodes found")
      tcp_rule = [ e for e in frule['allowed'] if e['IPProtocol'] == 'tcp' ][0]
      ports = [ str(e) for e in sorted(set([ int(r) for r in tcp_rule['ports'] ])) ]
      # check ports
      docker_compose_ports = [ e.split(':')[0] for e in _flat_list([ sinfo['ports'] for sname, sinfo in _load_yaml(DSB_PATH / args['app'] / 'docker-compose.yml')['services'].items() if 'ports' in sinfo]) ]
      missing_ports = set(docker_compose_ports) - set(ports)
      if (missing_ports):
        print(f"[ERROR] Missing ports on firewall rule: {missing_ports}")
        exit(-1)


#-----------------
# DEPLOY
#-----------------
def deploy(args):
  _put_last(args['deploy_type'], 'app', args['app'])

  if not args['tag']:
    args['tag'] = f"{datetime.now().strftime('%Y%m%d%H%M')}"
  _put_last(args['deploy_type'], 'tag', args['tag'])

  args['deploy_dir'] = _deploy_dir(args)
  _put_last(args['deploy_type'], 'config', args['config'])

  getattr(sys.modules[__name__], f"deploy__{args['app']}__{args['deploy_type']}")(args)
  print(f"[INFO] {args['app']} @ {args['deploy_type']} deployed successfully with '{args['tag']}' tag!")

def deploy__socialNetwork__local(args):
  import shutil
  from plumbum import local, FG

  deploy_dir = args['deploy_dir']
  config = _load_yaml(args['config'])

  print(f"[INFO] Copying deploy files... ", flush=True)
  os.makedirs(deploy_dir, exist_ok=True)
  shutil.copytree(DSB_PATH / args['app'], deploy_dir, dirs_exist_ok=True)

def deploy__socialNetwork__gsd(args):
  from plumbum.cmd import ansible_playbook
  from jinja2 import Environment
  import click
  import textwrap
  from shutil import copyfile

  app_dir = DSB_PATH / args['app']
  os.chdir(ROOT_PATH / 'deploy')

  # Update docker-compose.yml
  deploy_nodes = {}
  with open(filepath, 'r') as f_conf, open(Path(app_dir, 'docker-compose.yml'), 'r') as f_compose:
    conf = yaml.load(f_conf, Loader=yaml.FullLoader)
    compose = yaml.load(f_compose, Loader=yaml.FullLoader)

    # add network that is created on deploy playbook
    compose['networks'] = {
      DOCKER_COMPOSE_NETWORK: {
        'external': {
          'name': DOCKER_COMPOSE_NETWORK
        }
      }
    }

    # get all the nodes that will be used in deploy
    deploy_nodes = { n:GSD_AVAILABLE_NODES[n] for n in set(conf['services'].values()) }

    for sid,hostname in conf['services'].items():
      # replaces existing networks with new one
      compose['services'][sid]['networks'] = [ DOCKER_COMPOSE_NETWORK ]

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
  _force_gcp_docker()
  from plumbum.cmd import ansible_playbook, ls
  import shutil
  from jinja2 import Environment
  import textwrap

  config = _load_yaml(args['config'])

  print(f"[INFO] Copying deploy files... ", flush=True)
  os.makedirs(args['deploy_dir'], exist_ok=True)
  shutil.copytree(DSB_PATH / args['app'], args['deploy_dir'], dirs_exist_ok=True)
  shutil.copy(ROOT_PATH / 'gcp' / 'vars.yml', args['deploy_dir'])

  print(f"[INFO] Generating docker-compose-swarm.yml ... ", flush=True)
  # replace hostname in docker_compose_swarm
  with local.cwd(args['deploy_dir']):
    app_compose = _load_yaml('docker-compose.yml')

    # add network that is created on deploy playbook
    app_compose.setdefault('networks', {})[DOCKER_COMPOSE_NETWORK] = {
      'external': {
        'name': DOCKER_COMPOSE_NETWORK
      }
    }

    for service, node_id in config['services'].items():
      compose_service = app_compose['services'][service]

      # replaces existing networks with new one
      if 'networks' not in compose_service: compose_service['networks'] = []
      compose_service['networks'].append(DOCKER_COMPOSE_NETWORK)

      # replace the deploy constraints with new nodes
      if 'deploy' not in compose_service: compose_service['deploy'] = {}
      if 'placement' not in compose_service['deploy']: compose_service['deploy']['placement'] = {}
      if 'constraints' not in compose_service['deploy']['placement']: compose_service['deploy']['placement']['constraints'] = []
      deploy_constraints = compose_service['deploy']['placement']['constraints']
      # get the id of constraint of the node hostname
      node_constraint_index = _index_containing_substring(deploy_constraints, 'node.hostname')
      # replace docker-compose with that constraint
      deploy_constraints[node_constraint_index] = f"node.hostname == {node_id}"

    # now write the compose into a new file
    _dump_yaml(local.cwd / 'docker-compose-swarm.yml', app_compose)
  print(f"[SAVED] 'docker-compose-swarm.yml'")

  # Keep track of all created vms
  vms = { 'swarm_manager': [], 'cluster': [], 'clients': [] }

  print(f"[INFO] Creating GCP instances ... ", flush=True)
  # create the swarm manager node
  manager_config = {
    'name': config['manager']['name'],
    'machineType': f"zones/{config['manager']['zone']}/machineTypes/{config['manager']['machine_type']}",
    'disks': [
      {
        'boot': True,
        'autoDelete': True,
        'initializeParams': {
          'sourceImage': GCP_MACHINE_IMAGE_LINK,
        }
      }
    ],
    'hostname': config['manager']['hostname'],
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
      "items": ['swarm', 'portainer'],
    },
  }
  vms['swarm_manager'].append(_gcp_vm_create(config['manager']['zone'], manager_config))

  # app nodes
  for node_name, node_info in config['nodes'].items():
    node_config = {
      'name': node_name,
      'machineType': f"zones/{node_info['zone']}/machineTypes/{node_info['machine_type']}",
      'disks': [
        {
          'boot': True,
          'autoDelete': True,
          'initializeParams': {
            'sourceImage': GCP_MACHINE_IMAGE_LINK,
          }
        }
      ],
      'hostname': node_info['hostname'],
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
        "items": ['swarm', 'nodes'],
      },
    }
    vms['cluster'].append(_gcp_vm_create(node_info['zone'], node_config))

  # client nodes
  for i in range(args['clients']):
    node_config = {
      'name': f"client{i:02}",
      'machineType': f"zones/{config['clients']['zone']}/machineTypes/{config['clients']['machine_type']}",
      'disks': [
        {
          'boot': True,
          'autoDelete': True,
          'initializeParams': {
            'sourceImage': GCP_MACHINE_IMAGE_LINK,
          }
        }
      ],
      'hostname': f"client{i:02}.dsb",
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
        "items": ['swarm', 'nodes'],
      },
    }
    vms['clients'].append(_gcp_vm_create(node_info['zone'], node_config))

  # Build inventory for ansible playbooks
  print(f"[INFO] Generating inventory... ", flush=True)
  inventory = {}
  for group_name, group_nodes in vms.items():
    for vm in group_nodes:
      zone = vm['zone'].rsplit('/', 1)[1]
      external_ip, internal_ip = _gcp_vm_wait_for_ip(zone, vm['name'])

      if group_name not in inventory:
        inventory[group_name] = []
      inventory[group_name].append({
        'name': vm['name'],
        'hostname': vm['hostname'],
        'zone': zone,
        'external_ip': external_ip,
        'internal_ip': internal_ip,
      })

  template = """
    [swarm_manager]
    {{ swarm_manager['hostname'] }} ansible_host={{ swarm_manager['external_ip'] }} gcp_zone={{ swarm_manager['zone'] }} gcp_name={{ swarm_manager['name'] }} gcp_host={{ swarm_manager['internal_ip'] }} ansible_user=root ansible_ssh_private_key_file={{ private_key_path }}

    [cluster]
    {% for node in cluster %}{{ node['hostname'] }} ansible_host={{ node['external_ip'] }} gcp_zone={{ node['zone'] }} gcp_name={{ node['name'] }} gcp_host={{ node['internal_ip'] }} ansible_user=root ansible_ssh_private_key_file={{ private_key_path }}
    {% endfor %}

    [clients]
    {% for node in clients %}{{ node['hostname'] }} ansible_host={{ node['external_ip'] }} gcp_zone={{ node['zone'] }} gcp_name={{ node['name'] }} gcp_host={{ node['internal_ip'] }} ansible_user=root ansible_ssh_private_key_file={{ private_key_path }}
    {% endfor %}
  """
  template_render = Environment().from_string(template).render({
    'swarm_manager': inventory['swarm_manager'][0], # only one swarm manager
    'cluster': inventory['cluster'],
    'clients': inventory['clients'],
    'private_key_path': str(ROOT_PATH / 'gcp' / '.ssh' / 'google_compute_engine'), # ROOT_PATH here will be the container path
  })
  inventory_filepath = args['deploy_dir'] / 'inventory.cfg'
  with open(inventory_filepath, 'w') as f:
    # remove empty lines and dedent for easier read
    f.write(textwrap.dedent(template_render))
  print(f"[SAVED] '{inventory_filepath}'")

  print(f"[INFO] Replace vars.yml entries... ", flush=True)
  vars_filepath = args['deploy_dir'] / 'vars.yml'
  vars_yaml = _load_yaml(vars_filepath)
  # new vars
  vars_yaml['app'] = args['app']
  vars_yaml['swarm_overlay_network'] = DOCKER_COMPOSE_NETWORK
  vars_yaml['docker_image_namespace'] = GCP_DOCKER_IMAGE_NAMESPACE
  vars_yaml['deploy_path'] = str(ROOT_PATH / 'gcp')
  vars_yaml['deploy_dir'] = str(args['deploy_dir'])
  # used to sync deployment
  vars_yaml['compose_post_service_instance'] = config['nodes'][config['services']['compose-post-service']]['hostname']
  vars_yaml['num_services'] = NUM_SERVICES[args['app']]
  # dump
  _dump_yaml(vars_filepath, vars_yaml)
  print(f"[SAVED] '{vars_filepath}'")

  print(f"[INFO] Run deploy playbooks ... ", flush=True)
  # sleep to give extra time for nodes to be ready to accept requests
  time.sleep(30)
  with local.cwd(ROOT_PATH / 'gcp'):
    ansible_playbook['deploy-setup.yml',
      '-i', inventory_filepath,
      '--extra-vars', f"@{vars_filepath}"
    ] & FG
    ansible_playbook['deploy-swarm.yml',
      '-i', inventory_filepath,
      '--extra-vars', f"@{vars_filepath}"
    ] & FG


#-----------------
# INFO
#-----------------
def info(args):
  print(f"[INFO] {args['app']} @ {args['deploy_type']} services:")
  getattr(sys.modules[__name__], f"info__{args['app']}")(args)

def info__socialNetwork(args):
  print(f"Jaeger:\t\t{_service_ip(args['deploy_type'], args['app'], 'jaeger')}")
  print(f"Portainer:\t{_service_ip(args['deploy_type'], args['app'], 'portainer')}\t\t(admin / antipodeantipode)")
  print(f"Prometheus:\t{_service_ip(args['deploy_type'], args['app'], 'prometheus')}")
  print(f"RabbitMQ-EU:\t{_service_ip(args['deploy_type'], args['app'], 'rabbitmq-eu')}\t\t(admin / admin)")
  print(f"RabbitMQ-US:\t{_service_ip(args['deploy_type'], args['app'], 'rabbitmq-us')}\t\t(admin / admin)")


#-----------------
# RUN
#-----------------
def run(args):
  args['tag'] = _get_last(args['deploy_type'], 'tag')
  args['deploy_dir'] = _deploy_dir(args)
  _put_last(args['deploy_type'], 'antipode', args['antipode'])
  _put_last(args['deploy_type'], 'portainer', args['portainer'])
  _put_last(args['deploy_type'], 'prometheus', args['prometheus'])

  getattr(sys.modules[__name__], f"run__{args['app']}__{args['deploy_type']}")(args)
  print(f"[INFO] {args['app']} @ {args['deploy_type']} ran successfully!")

def run__socialNetwork__local(args):
  from plumbum import FG
  from plumbum.cmd import docker_compose, ls
  from jinja2 import Environment
  import textwrap

  print(f"[INFO] Generating .env ...", flush=True)
  with local.cwd(args['deploy_dir']):
    template = """
    ANTIPODE={{ antipode }}
    """
    template_render = Environment().from_string(template).render({
      'antipode': int(args['antipode']),
    })
    with open('.env', 'w') as f:
      # remove empty lines and dedent for easier read
      f.write(textwrap.dedent(template_render))
    print(f"[SAVED] '.env'")

    # Fixes error: "WARNING: Connection pool is full, discarding connection: localhost"
    # ref: https://github.com/docker/compose/issues/6638#issuecomment-576743595
    with local.env(COMPOSE_PARALLEL_LIMIT=99):
      docker_compose['up',
        '-d' if args['detached'] else None, # run containers in detached mode
      ] & FG

  if args['portainer']:
    with local.cwd(ROOT_PATH / 'local'):
      docker_compose[
        '-f', 'docker-compose-portainer.yml',
        'up',
        '-d' # portainer always runs detached
      ] & FG

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
  _force_gcp_docker()
  from plumbum.cmd import ansible_playbook

  vars_filepath = args['deploy_dir'] / 'vars.yml'
  inventory_filepath = args['deploy_dir'] / 'inventory.cfg'
  inventory = _load_inventory(inventory_filepath)

  if args['portainer']:
    _put_last('gcp', 'portainer', True)
    with local.cwd(ROOT_PATH / 'gcp'):
      ansible_playbook['start-portainer.yml',
        '-i', inventory_filepath,
        '--extra-vars', f"@{vars_filepath}"
      ] & FG
      portainer_url = _service_ip(args['deploy_type'], args['app'], 'portainer')
      _wait_url_up(portainer_url)
      print(f"[INFO] Portainer link (u/pwd: admin/antipode): {portainer_url}")

  if args['prometheus']:
    _put_last('gcp', 'prometheus', True)
    with local.cwd(ROOT_PATH / 'gcp'):
      ansible_playbook['deploy-prometheus.yml',
        '-i', inventory_filepath,
        '--extra-vars', f"@{vars_filepath}"
      ] & FG
      ansible_playbook['start-prometheus.yml',
        '-i', inventory_filepath,
        '--extra-vars', f"@{vars_filepath}"
      ] & FG
      prometheus_url = _service_ip(args['deploy_type'], args['app'], 'prometheus')
      _wait_url_up(prometheus_url)
      print(f"[INFO] Prometheus link: {prometheus_url}")

  # start dsb services
  with local.cwd(ROOT_PATH / 'gcp'):
    ansible_playbook['start-dsb.yml',
      '-i', inventory_filepath,
      '--extra-vars', f"@{vars_filepath}"
    ] & FG

  # init social graph
  # ansible_playbook['init-social-graph.yml', '-e', 'app=socialNetwork', '-e', f"configuration={filepath}"] & FG

  print("[INFO] Run Complete!")


#-----------------
# DELAY
#-----------------
def delay(args):
  if args['jitter'] == 0 and args['distribution'] in [ 'normal', 'pareto', 'paretonormal']:
      raise argparse.ArgumentTypeError(f"{args['distribution']} does not allow for 0ms jitter")

  args['tag'] = _get_last(args['deploy_type'], 'tag')
  args['deploy_dir'] = _deploy_dir(args)

  args['delay'] = f"{args['delay']}ms"
  args['jitter'] = f"{args['jitter']}ms"
  args['correlation'] = f"{args['correlation']}%"
  args['src_container'] = 'post-storage-mongodb-eu'
  args['dst_container'] = 'post-storage-mongodb-us'

  getattr(sys.modules[__name__], f"delay__{args['app']}__{args['deploy_type']}")(args)
  print(f"[INFO] {args['app']} @ {args['deploy_type']} delay activated successfully!")

def delay__socialNetwork__local(args):
  from plumbum.cmd import docker_compose, docker

  with local.cwd(args['deploy_dir']):
    # get ip of the dst container since delay only accepts ips
    # dst_container_id = docker_compose['ps', '-q', dst_container].run()[1].rstrip()
    # dst_ip = docker['inspect', '-f' '{{range.NetworkSettings.Networks}}{{.IPAddress}}{{end}}', dst_container_id].run()[1].rstrip()

    docker_compose['exec',
      args['src_container'],
      '/home/delay.sh',
      args['dst_container'],
      args['delay'],
      args['jitter'],
      args['correlation'],
      args['distribution']
    ] & FG

def delay__socialNetwork__gcp(args, src_container, dst_container, delay_ms, jitter_ms, correlation_per, distribution):
  _force_docker()
  from plumbum.cmd import ansible_playbook
  from jinja2 import Environment
  import textwrap

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
def _wkld_docker_args(endpoint, args, hosts, app_wd):
  docker_args = ['run',
    '--rm', '-it',
    '--name', WKLD_CONTAINER_NAME,
    '--network=host',
    '-w', '/scripts',
  ]
  if endpoint['type'] == 'wrk2':
    wrk2_params = _wrk2_params(args, endpoint, hosts[MAIN_ZONE])
    # prepare docker env
    docker_args += [
      '-v', f"{app_wd}/wrk2/scripts:/scripts",
      '-v', f"{app_wd}/datasets:/scripts/datasets",
    ]
    # add hosts env vars so the previous vars that were set are captured
    for k,v in hosts.items():
      docker_args += [ '-e', f"HOST_{k.upper()}"]
    # add remaing args
    docker_args += [
      'wrk2:antipode',
      '/wrk2/wrk'
    ] + wrk2_params
    # run docker
  elif endpoint['type'] == 'python':
    script_path = app_wd / endpoint['script_path']
    # prepare docker env
    docker_args += [
      '-v', f"{script_path.parent}:/scripts",
      '-v', f"{app_wd}/datasets:/scripts/datasets",
    ]
    # add hosts env vars so the previous vars that were set are captured
    for k,v in hosts.items():
      docker_args += [ '-e', f"HOST_{k.upper()}"]
    # add remaing args
    docker_args += [
      'python-wkld:antipode',
      'python',
      script_path.name,
    ] + endpoint['args']
  #
  return docker_args

#-----------------

def wkld(args):
  args['tag'] = _get_last(args['deploy_type'], 'tag')
  args['deploy_dir'] = _deploy_dir(args)
  args['endpoint'] = WKLD_ENDPOINTS[args['app']][args['Endpoint']]
  args['wkld_tag'] = f"{datetime.now().strftime('%Y%m%d%H%M')}"
  _put_last(args['deploy_type'], 'wkld_tag', args['wkld_tag'])
  _put_last(args['deploy_type'], 'wkld_rate', args['rate'])
  _put_last(args['deploy_type'], 'wkld_endpoint', args['Endpoint'])

  getattr(sys.modules[__name__], f"wkld__{args['deploy_type']}__run")(args)
  print(f"[INFO] {args['app']} @ {args['deploy_type']} workload ran successfully!")

def wkld__local__run(args):
  from plumbum.cmd import docker, python

  hosts = {
    'eu': 'http://127.0.0.1:8080',
    'us': 'http://127.0.0.1:8082',
  }
  # run workload in deploy dir
  with local.cwd(args['deploy_dir']):
    # run workload for hosts as env variables
    with local.env(**{ f"HOST_{k.upper()}":v for k,v in hosts.items() }):
      docker_args = _wkld_docker_args(args['endpoint'], args, hosts, args['deploy_dir'])
      docker[docker_args] & FG

#-----------------

def wkld__socialNetwork__gsd__hosts(args):
  # eu - nginx-thrift: node23
  # us - nginx-thrift-us: node24§

  filepath = args['configuration_path']
  with open(filepath, 'r') as f_conf:
    conf = yaml.load(f_conf, Loader=yaml.FullLoader)
    return {
      'eu': f"http://{conf['services']['nginx-thrift']}:8080",
      'us': f"http://{conf['services']['nginx-thrift-us']}:8082",
    }

def wkld__socialNetwork__gcp__hosts(args):
  filepath = args['configuration_path']
  with open(filepath, 'r') as f_conf:
    conf = yaml.load(f_conf, Loader=yaml.FullLoader)
    inventory = _inventory_to_dict(ROOT_PATH / 'deploy' / 'gcp' / 'inventory.cfg')
    return {
      'eu': f"http://{inventory[conf['services']['nginx-thrift']]['external_ip']}:8080",
      'us': f"http://{inventory[conf['services']['nginx-thrift-us']]['external_ip']}:8082",
    }

def wkld__gsd__run(args, hosts, exe_path, exe_args):
  from plumbum.cmd import ansible_playbook
  from jinja2 import Environment
  import textwrap


  if not args['node']:
    print("[INFO] No nodes passed onto gsd deployment workload. Using current node with remote hosts.")
    wkld__socialNetwork__local__run(args, hosts, exe_path, exe_args)
    return

  app_dir = DSB_PATH / args['app']
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
    'host_eu': hosts['eu'],
    'host_us': hosts['us'],
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
      'host_eu': hosts['eu'],
      'host_us': hosts['us'],
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

#-----------------

def gather(args):
  from plumbum.cmd import sudo
  import pandas as pd
  # pd.set_option('display.float_format', lambda x: '%.3f' % x)
  pd.set_option('display.html.table_schema', True)
  pd.set_option('display.precision', 5)

  args['tag'] = _get_last(args['deploy_type'], 'tag')
  args['deploy_dir'] = _deploy_dir(args)
  args['wkld_endpoint'] = _get_last(args['deploy_type'], 'wkld_endpoint')

  print("[INFO] Download client output ...")
  getattr(sys.modules[__name__], f"gather__{args['app']}__{args['deploy_type']}__download")(args)

  print("[INFO] Gather jaeger traces ...")
  # load jaeger host
  jaeger_host = _service_ip(args['deploy_type'], args['app'], 'jaeger')
  # build gather path
  rqs = _get_last(args['deploy_type'], 'wkld_rate')
  wkld_tag =_get_last(args['deploy_type'], 'wkld_tag')
  gather_path = GATHER_PATH / args['deploy_type'] / args['app'] / args['wkld_endpoint'] / _build_gather_tag() / wkld_tag
  # create folder if needed
  os.makedirs(gather_path, exist_ok=True)
  # force chmod of that dir
  sudo['chmod', 777, gather_path] & FG

  # set number of requests to gather from jaeger
  limit = args['num_requests']
  if limit is None:
    limit = int(input(f"Visit {jaeger_host}/dependencies to check number of flowing requests: "))

  # read traces from jaeger
  df, missing_info = _fetch_compose_post_service_traces(jaeger_host, limit)

  # merge result with mongodb change stream consistency diff information
  # consistency_df,_ = _fetch_mongo_change_stream_traces(jaeger_host, limit)
  # df = df.join(consistency_df.set_index('post_id'), on='post_id')

  # remove unecessary columns
  del df['post_id']
  del df['poststorage_post_written_ts']
  del df['poststorage_read_notification_ts']
  del df['wht_start_queue_ts']
  del df['wht_start_worker_ts']

  print(f"[INFO] Save '{local.cwd}/traces.csv'")
  with local.cwd(gather_path):
    # save to csv so we can plot a timeline later
    df.to_csv('traces.csv', sep=';', mode='w')

  # compute extra info to output in info file
  consistent_df = df[df['consistency_bool'] == True]
  inconsistent_df = df[df['consistency_bool'] == False]
  inconsistent_count = len(inconsistent_df)
  consistent_count = len(consistent_df)

  with local.cwd(gather_path):
    # save datatraces to info
    print(f"[INFO] Save '{local.cwd}/traces.info'\n")
    with open('traces.info', 'w') as f:
      print(f"{missing_info} messages skipped due to missing information", file=f)
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

    # build info file
    print(f"[INFO] Save '{local.cwd}/info.yml'\n")
    plot_info = {
      'por_inconsistencies': inconsistent_count / float(len(df)),
      'requests': limit,
      'type': 'antipode' if _get_last(args['deploy_type'], 'antipode') else 'baseline',
      'rps': rqs,
      'zone_pair': _load_yaml(ROOT_PATH / _get_last(args['deploy_type'], 'config'))['replication_pair'].upper(),
    }
    _dump_yaml(local.cwd / 'info.yml', plot_info)

    # copy last file
    print(f"[INFO] Save '{local.cwd}/last.yml'\n")
    path.utils.copy(Path(LAST_DEPLOY_FILE[args['deploy_type']]), local.cwd)

    # print file to stdout at the end of gather
    with open('traces.info', 'r') as f:
        print(f.read())
  #
  print(f"[INFO] {args['app']} @ {args['deploy_type']} gathered successfully!")

def gather__socialNetwork__local__download(args):
  return None

def gather__socialNetwork__gsd__download(args):
  return None

def gather__socialNetwork__gcp__download(args):
  from plumbum.cmd import ansible_playbook

  filepath = args['configuration_path']
  conf_path = f"{filepath.stem}"

  os.chdir(ROOT_PATH / 'deploy' / 'gcp')
  ansible_playbook['wkld-gather.yml', '-i', 'clients_inventory_local.cfg', '-e', 'app=socialNetwork', '-e', f'conf_path={conf_path}' ] & FG


#-----------------
# CLEAN
#-----------------
def clean(args):
  args['tag'] = _get_last(args['deploy_type'], 'tag')
  args['deploy_dir'] = _deploy_dir(args)

  getattr(sys.modules[__name__], f"clean__{args['app']}__{args['deploy_type']}")(args)
  print(f"[INFO] {args['app']} @ {args['deploy_type']} cleaned successfully!")

def clean__socialNetwork__local(args):
  from plumbum.cmd import docker_compose, docker

  with local.cwd(args['deploy_dir']):
    # first stops the containers
    docker_compose['stop'] & FG

    if args['strong']:
      docker_compose['down',
        '--rmi', 'all', '--remove-orphans'
      ] & FG
    else:
      docker_compose['down'] & FG

  if _get_last(args['deploy_type'], 'portainer'):
    _put_last('gcp', 'portainer', False)
    with local.cwd(ROOT_PATH / 'local'):
      if args['strong']:
        docker_compose[
          '-f', 'docker-compose-portainer.yml',
          'down',
          '--rmi', 'all', '--remove-orphans'
        ] & FG
      else:
        docker_compose[
          '-f', 'docker-compose-portainer.yml',
          'down'
        ] & FG

  # extra clean operations
  docker['system', 'prune', '-f', '--volumes'] & FG

def clean__socialNetwork__gsd(args):
  from plumbum.cmd import ansible_playbook

  # change path to playbooks folder
  os.chdir(ROOT_PATH / 'deploy' / 'gsd')

  ansible_playbook['undeploy-swarm.yml', '-e', 'app=socialNetwork'] & FG
  print("[INFO] Clean Complete!")

def clean__socialNetwork__gcp(args):
  _force_gcp_docker()
  from plumbum.cmd import ansible_playbook

  vars_filepath = args['deploy_dir'] / 'vars.yml'
  inventory_filepath = args['deploy_dir'] / 'inventory.cfg'
  inventory = _load_inventory(inventory_filepath)

  if args['strong']:
    print("[INFO] Removing GCP nodes ...")
    _gcp_vm_delete('us-east1-b', GCP_BUILD_IMAGE_NAME)
    for _, host in inventory.items():
      _gcp_vm_delete(host['gcp_zone'], host['gcp_name'])
  elif args['restart']:
    with local.cwd(ROOT_PATH / 'gcp'):
      ansible_playbook['restart-dsb.yml',
        '-i', inventory_filepath,
        '--extra-vars', f"@{vars_filepath}"
      ] & FG
  else:
    if _get_last('gcp', 'prometheus'):
      with local.cwd(ROOT_PATH / 'gcp'):
        ansible_playbook['undeploy-prometheus.yml',
          '-i', inventory_filepath,
          '--extra-vars', f"@{vars_filepath}"
        ] & FG
        _put_last('gcp', 'prometheus', False) # already cleaned so we remove flag

    if _get_last('gcp', 'portainer'):
      with local.cwd(ROOT_PATH / 'gcp'):
        ansible_playbook['undeploy-portainer.yml',
          '-i', inventory_filepath,
          '--extra-vars', f"@{vars_filepath}"
        ] & FG
        _put_last('gcp', 'portainer', False) # already cleaned so we remove flag

    with local.cwd(ROOT_PATH / 'gcp'):
      ansible_playbook['undeploy-swarm.yml',
        '-i', inventory_filepath,
        '--extra-vars', f"@{vars_filepath}"
      ] & FG

  print("[INFO] Clean Complete!")


#-----------------
# CONSTANTS
#-----------------
ROOT_PATH = Path(os.path.abspath(os.path.dirname(sys.argv[0])))
DSB_PATH = ROOT_PATH / 'DeathStarBench'
DEPLOY_PATH = ROOT_PATH / 'deploy'
GATHER_PATH = ROOT_PATH / 'gather'
# available deploys
DEPLOY_TYPES = {
  'local': { 'name': 'Localhost' },
  'gsd': { 'name': 'GSD Cluster' },
  'gcp': { 'name': 'Google Cloud Platform' },
}
LAST_DEPLOY_FILE = { dp : DEPLOY_PATH / dp / f".last.yml" for dp in DEPLOY_TYPES }
DSB_APPLICATIONS = [
  'socialNetwork',
]
MAIN_ZONE = 'eu'
WKLD_ENDPOINTS = {
  'socialNetwork': {
    # wrk2
    'compose-post': {
      'type': 'wrk2',
      'script_path': './wrk2/scripts/social-network/compose-post.lua',
      'uri': 'wrk2-api/post/compose',
    },
    'read-home-timeline': {
      'type': 'wrk2',
      'script_path': './wrk2/scripts/social-network/read-home-timeline.lua',
      'uri': 'wrk2-api/home-timeline/read',
    },
    'read-user-timeline': {
      'type': 'wrk2',
      'script_path': './wrk2/scripts/social-network/read-user-timeline.lua',
      'uri': 'wrk2-api/user-timeline/read',
    },
    'sequence-compose-post-read-home-timeline': {
      'type': 'wrk2',
      'script_path': './wrk2/scripts/social-network/sequence-compose-post-read-home-timeline.lua',
      'uri': 'wrk2-api/home-timeline/read',
    },
    # python scripts
    'antipode-wht-error': {
      'type': 'python',
      'script_path': './scripts/antipode-wht-error.py',
      'args': [],
    },
    'init-social-graph': {
      'type': 'python',
      'script_path': './scripts/init_social_graph.py',
      'args': [],
    },
  },
}
SERVICE_PORTS = {
  'socialNetwork': {
    'jaeger': 16686,
    'portainer': 9000,
    'prometheus': 9090,
    'rabbitmq-eu': 15672,
    'rabbitmq-us': 15673,
    'frontend-eu': 8080,
    'frontend-us': 8082,
  }
}
CONTAINERS_BUILT = [
  'mongodb-delayed:4.4.6',
  'mongodb-setup:4.4.6',
  'rabbitmq-setup:3.8',
  'yg397/openresty-thrift:latest',
  'yg397/social-network-microservices:antipode',
  'redis-im:antipode',
  'wrk2:antipode',
  'python-wkld:antipode',
]
NUM_SERVICES = {
  'socialNetwork': 40,
}
# docker
DOCKER_COMPOSE_NETWORK = 'deathstarbench_network'
# gcp
GCP_DOCKER_IMAGE_NAME = 'gcp-manager:antipode'
GCP_BUILD_IMAGE_NAME = 'antipode-dev-dsb'
GCP_MACHINE_IMAGE_NAME = 'antipode-dev-dsb'
GCP_CREDENTIALS_FILE = ROOT_PATH / 'gcp' / 'pluribus.json'
GCP_PROJECT_ID = _get_config('gcp','project_id')
GCP_DEFAULT_SSH_USER = _get_config('gcp','default_ssh_user')
GCP_DOCKER_IMAGE_NAMESPACE = f"gcr.io/{GCP_PROJECT_ID}/dsb"
GCP_DOCKER_IMAGE_TAG = 'antipode'
GCP_MACHINE_IMAGE_LINK = f"https://www.googleapis.com/compute/v1/projects/{GCP_PROJECT_ID}/global/images/{GCP_MACHINE_IMAGE_NAME}"
# gather
WKLD_CONTAINER_NAME = 'dsb-wkld'
PERCENTILES_TO_PRINT = [.25, .5, .75, .90, .99]


#-----------------
# MAIN
#-----------------
if __name__ == "__main__":
  import argparse

  # parse arguments
  main_parser = argparse.ArgumentParser()
  main_parser.add_argument("app", choices=DSB_APPLICATIONS, help="Application to deploy")
  # deploy type group
  deploy_type_group = main_parser.add_mutually_exclusive_group(required=True)
  for dt, dt_info in DEPLOY_TYPES.items():
    deploy_type_group.add_argument(f'--{dt}', action='store_true', help=f"Deploy app to {dt_info['name']}")
  # different commands
  subparsers = main_parser.add_subparsers(help='commands', dest='which')

  # build application
  build_parser = subparsers.add_parser('build', help='Build application')
  build_parser.add_argument('--no-cache', action='store_true', help="Rebuild all containers")
  build_parser.add_argument('--skip-images', action='store_true', help="Skip building local images")

  # deploy application
  deploy_parser = subparsers.add_parser('deploy', help='Deploy application')
  deploy_parser.add_argument('-config', required=True, help="Deploy configuration")
  deploy_parser.add_argument('-tag', required=False, help="Deploy with already existing tag")
  deploy_parser.add_argument('-clients', required=True, type=int, help="Number of clients to run on")

  # info application
  info_parser = subparsers.add_parser('info', help='Deployment info')

  # run application
  run_parser = subparsers.add_parser('run', help='Run application')
  run_parser.add_argument('-prometheus', action='store_true', help="Run with prometheus enabled")
  run_parser.add_argument('-portainer', action='store_true', help="Run with portainer enabled")
  run_parser.add_argument('-detached', action='store_true', help="detached")
  run_parser.add_argument('-antipode', action='store_true', default=False, help="enable antipode")

  # delay application
  delay_parser = subparsers.add_parser('delay', help='Delay application')
  delay_parser.add_argument('-d', '--delay', type=float, default='100', help="Delay in ms")
  delay_parser.add_argument('-j', '--jitter', type=float, default='0', help="Jitter in ms")
  delay_parser.add_argument('-c', '--correlation', type=int, default='0', help="Correlation in % (0-100)")
  delay_parser.add_argument('-dist', '--distribution', choices=[ 'uniform', 'normal', 'pareto', 'paretonormal' ], default='uniform', help="Delay distribution")

  # workload application
  wkld_parser = subparsers.add_parser('wkld', help='Run HTTP workload generator')
  # comparable with wrk2 > ./wrk options
  wkld_parser.add_argument('-N', '--node', action='append', default=[], help="Run wkld on the following nodes")
  wkld_parser.add_argument('-E', '--Endpoint', choices=[ e for app_list in WKLD_ENDPOINTS.values() for e in app_list ], help="Endpoints to generate workload for")
  wkld_parser.add_argument('-c', '--connections', type=int, default=1, help="Connections to keep open")
  wkld_parser.add_argument('-d', '--duration', type=int, default='1', help="Duration in s")
  wkld_parser.add_argument('-t', '--threads', type=int, default=1, help="Number of threads")
  wkld_parser.add_argument('-r', '--rate', type=int, default=1, required=True, help="Work rate (throughput) in request per second total")
  # Existing options:
  # -D, --dist             fixed, exp, norm, zipf
  # -P                     Print each request's latency
  # -p                     Print 99th latency every 0.2s to file
  # -L  --latency          Print latency statistics

  # gather application
  gather_parser = subparsers.add_parser('gather', help='Gather data from application')
  gather_parser.add_argument('-n', '--num-requests', type=int, default=None, help="Gather this amount of requests skipping the input")

  # clean application
  clean_parser = subparsers.add_parser('clean', help='Clean application')
  clean_parser.add_argument('-strong', action='store_true', help="delete images")
  clean_parser.add_argument('-restart', action='store_true', help="clean deployment by restarting containers")

  ##
  args = vars(main_parser.parse_args())
  command = args.pop('which')

  # set the app dir
  args['app_dir'] = DSB_PATH / args['app']

  # parse deploy type
  args['deploy_type'] = None
  for dt in DEPLOY_TYPES:
    if args[dt]:
      args['deploy_type'] = dt
    del args[dt]

  # call parser method dynamically
  getattr(sys.modules[__name__], command)(args)
