# Antipode @ DeathStarBench


## Prerequisites

You need to install the following dependencies before runnning:
- Python 3.7+
- The requisites for our `maestro` script. Simply run `pip install -r requirements.txt`
- Docker > 20.10
- Docker Compose > 2.14
- Pull submodules: `git submodule update --init --recursive`

Prerequesites for **Local** deployment:
- Docker Compose
    - Make sure the exec `docker-compose` (with hyphen) is available. If you only have the `docker compose` (no hyphen) version, you can create a symlink like this: `sudo ln -s /usr/libexec/docker/cli-plugins/docker-compose /usr/local/bin/docker-compose`

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
./maestro --gsd socialNetwork build
./maestro --gsd socialNetwork deploy -config CONFIG_FILE -clients NUM_CLIENTS
```
You can either build your own deployment configuration file, or you one already existing.

After deploy is done you can start the DeathStarBench services with:
```zsh
./maestro --gsd socialNetwork run -antipode
```
In order to run the original TrainTicket application remove the `-antipode` parameter.

Then you can run the `compose-post` workloads to evaluate inconsistencies and gather its results:
```zsh
./maestro --gsd socialNetwork wkld -E compose-post -r RATE -d DURATION
./maestro --gsd socialNetwork gather
```
For instance, we can the workload for 300 seconds (`300`), and with a rate of `100` requests per second.

At the end, you can clean your experiment:
```zsh
./maestro --gsd socialNetwork clean
```

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
For instance, for SOSP'23 plots both `configs/gcp/socialNetwork/us-eu.yml` and `configs/gcp/socialNetwork/us-sg.yml` configurations where used with `1` client.

After deploy is done you can start the DeathStarBench services with:
```zsh
./maestro --gcp socialNetwork run -antipode
```
In order to run the original TrainTicket application remove the `-antipode` parameter.

Then you can run the `compose-post` workloads to evaluate inconsistencies and gather its results:
```zsh
./maestro --gcp socialNetwork wkld -E compose-post -d DURATION -r RATE -c CONNECTIONS -t THREADS
./maestro --gcp socialNetwork gather
```

For instance, we can the workload for 300 seconds (`300`), and with a rate of `100` requests per second, with `4` connections and `2` threads.

At the end, you can clean your experiment and destroy your GCP instance with:
```zsh
./maestro --gcp socialNetwork clean -strong
```
In order to keep your GCP instances and just undeploy DeathStarBench, remove the `-strong` parameter (you can also restart by passing the `-restart` parameter).

Although `maestro` run a single deployment end-to-end, in order to run all the necessary workloads for plots you need to repeat these steps for several different combinations.
To ease that process we provide `maestrina`, a convenience script that executes all combinations of workloads to plot after. In order to change the combinations just edit the code get your own combinations in place. There might be instances where `maestrina` is not able to run a specific endpoint, and in those scenarios you might need to rerun or run `maestro` individually -- which is always the safest method.
We provide a pre-populated `maestrina` with the combinations and rounds configurations for the SOSP'23 plots.


There are other commands available (for details do `-h`), namely:
- `./maestro --gcp socialNetwork delay` adds artificial delay to the replication between `post-storage` mongo instances.
- `./maestro --gcp info` that has multiple options from links to admin panels, logs and others


## Plots

The first step to build plots is making sure that you have all the datapoints first.
After that you need to set those datapoints into a *plot config file*, similar to the one provided at `plots/configs/sample.yml`.

With a configuration set with your datapoint, you simply run:
```zsh
./plot CONFIG --plots throughput_latency_with_consistency_window
```
To generate a throughput/latency plot in combination with our consistency window metric (similar to SOSP'23 results).
For more information type `./plot -h`


## Paper References

João Loff, Daniel Porto, João Garcia, Jonathan Mace, Rodrigo Rodrigues\
Antipode: Enforcing Cross-Service Causal Consistency in Distributed Applications\
To appear.\
[Download](https://dl.acm.org/doi/10.1145/3600006.3613176)

Gan et al\
An Open-Source Benchmark Suite for Microservices and Their Hardware-Software Implications for Cloud and Edge Systems\
ASPLOS 2019\
[Download](http://www.csl.cornell.edu/~delimitrou/papers/2019.asplos.microservices.pdf)
