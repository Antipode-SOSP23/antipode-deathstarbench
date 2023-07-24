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

### GCP Deploymnet
Make sure you have created a Google Cloud Platform (GCP) account and have setup a new project.
Go to the `config.yml` file and add your GCP project and user information, for example:
```yml
gcp:
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

Then run *maestro* to build and deploy the GCP instances:
```zsh
./maestro --gcp build
./maestro --gcp deploy -config CONFIG_FILE -clients NUM_CLIENTS
```
You can either build your own deployment configuration file, or you one already existing.
For instance, for SOSP'23 plots you should use the `configs/sosp23.yml` config and `1` as number of clients.


## Plots



## Paper References

João Loff, Daniel Porto, João Garcia, Jonathan Mace, Rodrigo Rodrigues
Antipode: Enforcing Cross-Service Causal Consistency in Distributed Applications
To appear.
[Download]()

Gan et al.
An Open-Source Benchmark Suite for Microservices and Their Hardware-Software Implications for Cloud and Edge Systems
ASPLOS 2019
[Download](http://www.csl.cornell.edu/~delimitrou/papers/2019.asplos.microservices.pdf)