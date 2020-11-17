#!/bin/bash
echo "*********************************"
echo "Starting the replica set"
echo "*********************************"

sleep 60 | echo "Sleeping"

mongo mongodb://post-storage-mongodb:27017 replicaSet.js
done=$?

echo "*********************************"
if [ "$done" -ne 0 ]; then
  echo "Replica set FAILED!"
else
  echo "Replica set DONE!"
fi
echo "*********************************"

exit $done