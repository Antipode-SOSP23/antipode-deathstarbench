#!/bin/bash
echo "*********************************"
echo "Starting the replica set"
echo "*********************************"

dockerize -wait tcp://post-storage-mongodb-eu:27017 -wait tcp://post-storage-mongodb-us:27017 -wait-retry-interval 30s echo "[INFO] post-storage-mongodb cluster ready!"

mongo mongodb://post-storage-mongodb-eu:27017 replicaSet.js
done=$?

if [ "$done" -ne 0 ]; then
  echo "*********************************"
  echo "Replica set FAILED!"
  echo "*********************************"
  exit $done
else
  echo "*********************************"
  echo "Replica set DONE!"
  echo "*********************************"


  echo "*********************************"
  echo "Opening HTTP:8000 server for dockerize coordination"
  echo "*********************************"

  python3 -m http.server

  echo "*********************************"
  echo "HTTP:8000 server for dockerize coordination DONE!"
  echo "*********************************"
fi
