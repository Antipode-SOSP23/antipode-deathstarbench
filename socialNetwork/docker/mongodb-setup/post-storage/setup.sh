#!/bin/bash
echo "*********************************"
echo "Starting the replica set"
echo "*********************************"

sleep 60 | echo "Sleeping"

# Primary
mongo mongodb://post-storage-mongodb:27017 replicaSet.js

echo "*********************************"
echo "Replica set DONE!"
echo "*********************************"