rsconf = {
  _id: "rs0",
  members: [
    {
      _id: 0,
      host: "post-storage-mongodb:27017",
      priority: 1000
    },
    {
      _id: 1,
      host: "post-storage-mongodb-us:27017",
      priority: 0
    },
  ]
}

rs.initiate(rsconf);
rs.conf();