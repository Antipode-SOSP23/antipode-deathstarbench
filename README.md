# Antipode @ DeathStarBench


## Prerequisites

You need to install the following dependencies before runnning:
- Python 3.7+
- The requisites for our `maestro` script. Simply run `pip install -r requirements.txt`
- Docker
- Pull submodules: `git submodule update --init --recursive`

Prerequesites for **Local** deployment:
- Docker Compose

Prerequesites for **GCP** deployment:
- Google Cloud Platform account
- Create a credentials JSON file ([instructions here](https://developers.google.com/workspace/guides/create-credentials)) and place the generated JSON file in `gcp` folder and name it `credentials.json`
- You might need to [increase your Compute Engine quotas](https://console.cloud.google.com/iam-admin/quotas).

Optional dependencies:
- `netem` kernel module in your **host machine** for `delay` command

## Usage
All the deployment is controlled by our `./maestro` script. Type `./maestro -h` to look into all the commands and option, and also each command comes with its own help, e.g. `./maestro run -h`.

### Local Deployment

### Cluster Deployment
For this deployment we are using INESC-ID DPSS cluster, although it should be easy to adapt to other clusters. We are making the following assumptions:
- `maestro` is ran from a machine within the cluster -- not necessarly one you will be using for your deployment
- You have cluster hosts information available at `~/.ssh/config`
- You have a centralized datastore available across all your cluster (e.g. NAS).

These configuration can be found in `gsd/config.yml`:
```yml
ssh_config_filepath: ~/.ssh/config
default_ssh_user: jfloff
nas_path: /mnt/nas/inesc/jfloff
```

Run `maestro` to build and deploy the GCP instances:
```zsh
./maestro --gcp socialNetwork build
./maestro --gcp socialNetwork deploy -config CONFIG_FILE -clients NUM_CLIENTS
```
You can either build your own deployment configuration file, or you one already existing.
For instance, for SOSP'23 plots you should use the `configs/gcp/socialNetwork/us-eu.yml` config.

### GCP Deploymnet
Make sure you have created a Google Cloud Platform (GCP) account and have setup a new project.
Go to `gcp/config.yml` file and add your GCP project and user information, for example:
```yml
project_id: antipode
default_ssh_user: jfloff
```

Set the following firewall rules on GCP [web console](https://console.cloud.google.com/networking/firewalls/list) (`maestro` will warn you of any missing keys):
  - `portainer`
      - IP Addresses: `0.0.0.0/0`
      - TCP ports: 9000,8000,9090,9091,9100
  - `swarm`
      - IP Addresses: `0.0.0.0/0`
      - TCP ports: 2376,2377,7946
      - UDP ports: 7946,4789
  - `nodes`
      - IP Addresses: `0.0.0.0/0`
      - TCP ports: 9001,16686,8080,8081,8082,1234,4080,5563,15672,5672,5778,14268,14250,9411,9100,15673,5673
      - UDP ports: 5775,6831,6832

Run `maestro` to build and deploy the GCP instances:
```zsh
./maestro --gcp socialNetwork build
./maestro --gcp socialNetwork deploy -config CONFIG_FILE -clients NUM_CLIENTS
```
You can either build your own deployment configuration file, or you one already existing.
For instance, for SOSP'23 plots you should use the `configs/gcp/socialNetwork/us-eu.yml` config.

After deploy is done you can start the DeathStarBench services with:
```zsh
./maestro --gcp socialNetwork run -antipode
```
In order to run the original TrainTicket application remove the `-antipode` parameter.

You feed the application the initial social graph with users and their followers with:
```zsh
./maestro --gcp socialNetwork wkld -E init-social-graph -r 1
```

And after you can run the `compose-post` workloads to evaluate inconsistencies and gather its results:
```zsh
./maestro --gcp socialNetwork wkld -E compose-post -r RATE -d DURATION
./maestro --gcp gather
```
For instance, we can the workload for 300 seconds (`300`), and with a rate of `100` requests per second.

At the end, you can clean your experiment and destroy your GCP instance with:
```zsh
./maestro --gcp socialNetwork clean -strong
```
In order to keep your GCP instances and just undeploy undeploy DeathStarBench, remove the `-strong` parameter (you can also restart by passing the `-restart` parameter).

There are other commands available (for details do `-h`), namely:
- `./maestro --gcp socialNetwork delay` adds artificial delay to the replication between `post-storage` mongo instances.
- `./maestro --gcp info` that has multiple options from links to admin panels, logs and others


## Plots



## Paper References

João Loff, Daniel Porto, João Garcia, Jonathan Mace, Rodrigo Rodrigues\
Antipode: Enforcing Cross-Service Causal Consistency in Distributed Applications\
To appear.\
[Download]()

Gan et al\
An Open-Source Benchmark Suite for Microservices and Their Hardware-Software Implications for Cloud and Edge Systems\
ASPLOS 2019\
[Download](http://www.csl.cornell.edu/~delimitrou/papers/2019.asplos.microservices.pdf)