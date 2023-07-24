FROM python:3.9-slim

LABEL maintainer="jfloff@dpss.inesc-id.pt"

# install needed packages
RUN set -ex ;\
    apt-get update ;\
    apt-get install -y --no-install-recommends \
        # gcp
        apt-transport-https \
        ca-certificates \
        openssl \
        gnupg \
        curl \
        jq \
        # others
        sudo \
        rsync \
        openssh-client \
        git \
        ;\
    rm -rf /var/lib/apt/lists/*

#-------------
# install gcloud
#-------------
ARG AUTH_FILE=credentials.json
COPY $AUTH_FILE /configs/$AUTH_FILE
ENV GOOGLE_APPLICATION_CREDENTIALS /configs/$AUTH_FILE
ENV CLOUDSDK_PYTHON=/usr/local/bin/python3
RUN set -ex ;\
    echo "deb [signed-by=/usr/share/keyrings/cloud.google.gpg] https://packages.cloud.google.com/apt cloud-sdk main" | tee -a /etc/apt/sources.list.d/google-cloud-sdk.list ;\
    curl https://packages.cloud.google.com/apt/doc/apt-key.gpg | apt-key --keyring /usr/share/keyrings/cloud.google.gpg add - ;\
    apt-get update ;\
    apt-get install -y --no-install-recommends \
        google-cloud-sdk \
        ;\
    rm -rf /var/lib/apt/lists/*

RUN set -ex ;\
    # vars
    export GCP_PROJECT_ID=$(jq -r '.project_id' ${GOOGLE_APPLICATION_CREDENTIALS}) ;\
    # config
    gcloud config configurations list ;\
    gcloud config configurations activate default ;\
    gcloud auth activate-service-account --key-file=${GOOGLE_APPLICATION_CREDENTIALS} ;\
    gcloud config set project $GCP_PROJECT_ID ;\
    gcloud auth configure-docker ;\
    gcloud config list

#-------------
# install python packages
#-------------
RUN set -ex ;\
    pip install --no-cache-dir --upgrade \
        google-api-python-client \
        plumbum \
        pandas \
        jinja2 \
        click \
        ansible \
        python-dotenv \
        ruamel.yaml \
        # error with mounting docker-compose bin as volume
        docker-compose \
        ;

# switch user to install zsh with a some plugins
CMD bash