#pragma once

#include "denoiser.hpp"

#include "yaml-cpp/yaml.h"

#include <type_traits>
#include <string>

template <typename CharT>
typename std::enable_if<not std::is_same<char, CharT>::value, std::basic_string<CharT>>::type
static inline convert(const std::string& str) {
  std::basic_string<CharT> ret;
  std::transform(str.begin(), str.end(), std::back_inserter(ret), [](char c) { return CharT(c); });
  return ret;
}

template <typename CharT>
typename std::enable_if<std::is_same<char, CharT>::value, const std::string&>::type
static inline convert(const std::string& str) {
  return str;
}

template <typename CharT>
struct patterns {
  std::vector<artifact::basic_pattern<CharT>> filters;
  std::vector<artifact::basic_pattern<CharT>> normalizers;
};

template <typename CharT>
class configuration {
public:
  std::string alias;
  std::string target;
  std::vector<std::string> reference;
  patterns<CharT> rules;

  static configuration<CharT> load(const std::string& filename) {
    return configuration<CharT>(YAML::LoadFile(filename));
  }

  static configuration<CharT> read(std::istream& istream) {
    return configuration<CharT>(YAML::Load(istream));
  }

private:
  configuration(const YAML::Node& node) {

    alias = node["alias"].as<std::string>();
    target = node["target"].as<std::string>();

    for (const auto& ref : node["reference"]) {
      reference.push_back(ref.as<std::string>());
    }

    extract_patterns(node, "filters", rules.filters);
    extract_patterns(node, "normalizers", rules.normalizers);
  }

  void extract_patterns(const YAML::Node& node,
                        const char* name,
                        std::vector<artifact::basic_pattern<CharT>>& list) {
    for (const auto& entry : node[name]) {
      if (entry["r"]) {
        list.emplace_back(std::basic_regex<CharT>(convert<CharT>(entry["r"].as<std::string>())));
      } else if (entry["s"]) {
        list.emplace_back(convert<CharT>(entry["s"].as<std::string>()));
      } else {
        throw std::runtime_error("hmmmmm");
      }
    }
  }
};
