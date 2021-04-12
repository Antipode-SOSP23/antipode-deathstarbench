#ifndef SOCIAL_NETWORK_MICROSERVICES_ANTIPODE_H
#define SOCIAL_NETWORK_MICROSERVICES_ANTIPODE_H

#include <string>
#include <fstream>
#include <iostream>

// mongolib
#include <bson/bson.h>
#include <mongoc/mongoc.h>

// to generate id
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "logger.h"

namespace social_network {

class AntipodeMongodb {
  static const std::string ANTIPODE_COLLECTION;

  mongoc_client_t* _client;
  std::string _dbname;
  mongoc_database_t* _db;
  mongoc_collection_t* _collection;

  public:
    AntipodeMongodb(mongoc_client_t*, std::string);
    ~AntipodeMongodb();
    static void init_store(std::string, std::string);
    std::string gen_cscope_id();
    bool inject(std::string, mongoc_client_session_t*);
    void barrier(std::string);
};

const std::string AntipodeMongodb::ANTIPODE_COLLECTION = "antipode";

AntipodeMongodb::AntipodeMongodb(mongoc_client_t* client, std::string dbname) {
  _client = client;
  _dbname = dbname;
  _db = mongoc_client_get_database(_client, _dbname.c_str());
  _collection = mongoc_database_get_collection (_db, AntipodeMongodb::ANTIPODE_COLLECTION.c_str());
}

AntipodeMongodb::~AntipodeMongodb() {
  mongoc_collection_destroy(_collection);
  mongoc_database_destroy(_db);
}

/* static */ void AntipodeMongodb::init_store (std::string uri, std::string dbname) {
  LOG(debug) << "[AntipodeMongodb] URI = " << uri;

  mongoc_init();
  mongoc_client_t* client = mongoc_client_new(uri.c_str());

  bson_error_t error;
  bson_t* b;

  // ensure client has connected
  b = BCON_NEW("ping", BCON_INT32(1));
  if(!mongoc_client_command_simple(client, dbname.c_str(), b, NULL, NULL, &error)){
    throw std::runtime_error("[AntipodeMongo] Failed to ping client" + std::string(error.message));
  }
  bson_free(b);

  // fetch server description
  mongoc_server_description_t* sd = mongoc_client_get_server_description(client, 1);
  if (sd == NULL) {
    throw std::runtime_error("[AntipodeMongo] Failed to fetch server description");
  }

  // look for is_master flag
  auto ismaster_description_json = bson_as_json(mongoc_server_description_ismaster(sd), nullptr);
  if (ismaster_description_json == NULL) {
    throw std::runtime_error("[AntipodeMongo] Unable to fetch cluster is_master flag");
  }
  mongoc_server_description_destroy(sd);

  // parse bson to json and get the ismaster flag
  bool is_master = json::parse(ismaster_description_json)["ismaster"];

  // only create collection on the master
  if (is_master) {
    LOG(debug) << "[AntipodeMongodb] Master found. Performing DB init calls";
    // ref: https://docs.mongodb.com/manual/core/transactions/#transactions-and-operations
    // ref: https://github.com/mongodb/mongo-c-driver/blob/master/src/libmongoc/examples/example-transaction.c
    mongoc_database_t* db = mongoc_client_get_database(client, dbname.c_str());

    // create collection for antipode `cscope_id`
    //    inserting into a nonexistent collection normally creates it, but a
    //    collection can't be created in a transaction - create it now
    mongoc_collection_t* collection = mongoc_database_create_collection(db, AntipodeMongodb::ANTIPODE_COLLECTION.c_str(), NULL, &error);
    if (!collection) {
      // code 48 is NamespaceExists, see error_codes.err in mongodb source
      if (error.code == 48) {
        collection = mongoc_database_get_collection (db, AntipodeMongodb::ANTIPODE_COLLECTION.c_str());
      } else {
        throw std::runtime_error("[AntipodeMongo] Failed to create collection: " + std::string(error.message));
      }
    }

    LOG(debug) << "[AntipodeMongodb] Created collection";

    // create unique index for CTX_ID
    bson_t keys;
    bson_init (&keys);

    BSON_APPEND_INT32(&keys, "cscope_id", 1);
    char* index_name = mongoc_collection_keys_to_index_string(&keys);
    bson_t* create_indexes = BCON_NEW (
        "createIndexes", BCON_UTF8(dbname.c_str()),
        "indexes", "[", "{",
            "key", BCON_DOCUMENT (&keys),
            "name", BCON_UTF8 ("cscope_id"),
            "unique", BCON_BOOL(true),
        "}", "]");

    if (!mongoc_database_write_command_with_opts (db, create_indexes, NULL, NULL /* &reply */, &error)) {
      throw std::runtime_error("[AntipodeMongo] Error creating cscope_id Index: " + std::string(error.message));
    }

    LOG(debug) << "[AntipodeMongodb] Created Index";
    bson_free (index_name);
    bson_destroy (create_indexes);
    mongoc_database_destroy(db);
  }
  else {
    LOG(debug) << "[AntipodeMongodb] Skipped master actions";
  }

  mongoc_client_destroy (client);
}

std::string AntipodeMongodb::gen_cscope_id() {
  boost::uuids::uuid id = boost::uuids::random_generator()();
  return boost::uuids::to_string(id);
}

bool AntipodeMongodb::inject(std::string cscope_id, mongoc_client_session_t* session) {
  bson_t* cscope_id_doc;
  bson_error_t error;

  cscope_id_doc = BCON_NEW ("cscope_id", BCON_UTF8 (cscope_id.c_str()));
  bool r = mongoc_collection_insert_one (_collection, cscope_id_doc, nullptr, nullptr, &error);
  if (!r) {
    MONGOC_ERROR ("[Antipode] Inject failed: %s", error.message);
  }

  return r;
}

void AntipodeMongodb::barrier(std::string cscope_id) {
  bson_t* filter = BCON_NEW ("cscope_id", BCON_UTF8(cscope_id.c_str()));
  bson_t* opts = BCON_NEW ("limit", BCON_INT64(1));
  mongoc_cursor_t* cursor;

  // blocking behaviour
  bool cscope_id_visible = false;
  while(!cscope_id_visible) {
    const bson_t* doc;
    cursor = mongoc_collection_find_with_opts(_collection, filter, opts, NULL);
    cscope_id_visible = mongoc_cursor_next(cursor, &doc);

    char* str = bson_as_canonical_extended_json (doc, NULL);
    LOG(debug) << " IS_VISIBLE " << str;
    bson_free (str);
  }

  mongoc_cursor_destroy (cursor);
  bson_destroy (filter);
  bson_destroy (opts);
}

} //namespace social_network

#endif //SOCIAL_NETWORK_MICROSERVICES_UTILS_H
