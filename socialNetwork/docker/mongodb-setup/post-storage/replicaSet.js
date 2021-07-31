rsconf = {
  _id: "rs0",
  writeConcernMajorityJournalDefault: false,
  members: [
    {
      _id: 0,
      host: "post-storage-mongodb-eu:27017",
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