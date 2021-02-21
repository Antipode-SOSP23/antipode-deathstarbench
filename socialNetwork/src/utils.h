#ifndef SOCIAL_NETWORK_MICROSERVICES_UTILS_H
#define SOCIAL_NETWORK_MICROSERVICES_UTILS_H

#include <string>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "logger.h"

namespace social_network{
using json = nlohmann::json;

const std::vector<std::string> ALL_ZONES({ "eu", "us" });

int load_config_file(const std::string &file_name, json *config_json) {
  std::ifstream json_file;
  json_file.open(file_name);
  if (json_file.is_open()) {
    json_file >> *config_json;
    json_file.close();
    return 0;
  }
  else {
    LOG(error) << "Cannot open service-config.json";
    return -1;
  }
};

std::string load_zone() {
  std::string z = (std::getenv("ZONE") == NULL) ? "eu" : std::getenv("ZONE");
  return z;
}

std::vector<std::string> load_interest_zones() {
  std::string z = load_zone();
  std::vector<std::string> interest_zones({ "eu", "us" });
  auto izones = std::find(interest_zones.begin(), interest_zones.end(), z);
  if (izones != interest_zones.end()) interest_zones.erase(izones);
  return interest_zones;
}

} //namespace social_network

#endif //SOCIAL_NETWORK_MICROSERVICES_UTILS_H
