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

  static std::vector<configuration<CharT>> load(const std::string& filename) {
    return parse(YAML::LoadFile(filename));
  }

  static std::vector<configuration<CharT>> read(std::istream& istream) {
    return parse(YAML::Load(istream));
  }

private:
  configuration(const YAML::Node& node) {

    alias = node["alias"].as<std::string>();
    target = node["target"].as<std::string>();

    for (const auto& ref : node["reference"]) {
      reference.push_back(ref.as<std::string>());
    }
  }

  static std::vector<configuration<CharT>> parse(const YAML::Node& node) {

    std::vector<configuration<CharT>> artifacts;

    for (const auto& entry : node["artifacts"]) {
      artifacts.push_back(configuration<CharT>(entry));
    }

    const auto extract_patterns = [&node](const char* name) -> std::vector<artifact::basic_pattern<CharT>> {
      std::vector<artifact::basic_pattern<CharT>> list;
      for (const auto& entry : node[name]) {
        if (entry["r"]) {
          list.emplace_back(std::basic_regex<CharT>(convert<CharT>(entry["r"].as<std::string>())));
        } else if (entry["s"]) {
          list.emplace_back(convert<CharT>(entry["s"].as<std::string>()));
        } else {
          throw std::runtime_error("hmmmmm");
        }
      }
      return list;
    };

    const auto filters = extract_patterns("filters");
    const auto normalizers = extract_patterns("normalizers");

    for(auto& artifact : artifacts) {
      artifact.rules.filters = filters;
      artifact.rules.normalizers = normalizers;
    }

    return artifacts;
  }
};
