#include <iostream>
#include <unordered_set>
#include <future>

#include "gflags/gflags.h"
#include "curlpp/Easy.hpp"
#include "yaml-cpp/yaml.h"
#include "log-reader.hpp"
//#include "thread-pool.hpp"

namespace gflags {

class Context final {
public:
  inline Context(int argc, char** argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
  }
  inline ~Context() {
    gflags::ShutDownCommandLineFlags();
  }
};
}

template <typename CharT>
struct artifact {
  std::string target;
  std::vector<std::string> reference;
  std::vector<std::basic_regex<CharT>> filters;
  std::vector<std::basic_regex<CharT>> normalizers;
};


template <typename CharT, typename Lambda>
void process(
    const artifact<CharT>& artifact,
    Lambda lambda) {

  using regex = std::basic_regex<CharT>;

  std::unordered_set<size_t> bucket;
  std::mutex mutex;

  const auto prepare = [](
      const std::string& url,
      const std::vector<regex>& filters,
      const std::vector<regex>& normalizers
      ) -> log::file<CharT> {

    debug("downloading " << url << "...");
    curlpp::Easy request;
    request.setOpt(curlpp::options::Url(url));
    log::file<CharT> file(request);

    debug("applying " << filters.size() << " filters");
    for (const auto& pattern : filters) {
      for (auto& line : file) {
        if (line.contains(pattern)) {
          line.suppress();
        }
      }
    }

    debug("applying " << normalizers.size() << " normalizers");
    for (const auto& pattern : normalizers) {
      for (auto& line : file) {
        line.remove(pattern);
      }
    }

    return file;
  };

  const auto do_process = [&bucket, &mutex, &prepare](
      const std::string& url,
      const std::vector<regex>& filters,
      const std::vector<regex>& normalizers
      ) -> void {

    auto file = prepare(url, filters, normalizers);

    debug("filling the bucket");
    std::lock_guard<std::mutex> lock(mutex);
    const auto bucket_size = (file.size() * 3) / 2;
    if(bucket.size() < bucket_size) {
      bucket.reserve(bucket_size);
    }
    for (auto& line : file) {
      bucket.insert(line.hash());
    }
  };

  std::vector<std::future<void>> f_refs;
  f_refs.reserve(artifact.reference.size());
  for (const auto& ref : artifact.reference) {
    f_refs.emplace_back(std::async(do_process, ref, artifact.filters, artifact.normalizers) );
  }

  const auto file = prepare(artifact.target, artifact.filters, artifact.normalizers);

  for (const auto& f_ref : f_refs) {
    f_ref.wait();
  }

  for (const auto& line : file) {
    if (0 == bucket.count(line.hash())) {
      lambda(line.original());
    }
  }
}

template <typename CharT>
artifact<CharT> decodeArtifact(const YAML::Node& node) {

  artifact<CharT> out;

  out.target = node["target"].as<std::string>();

  for (const auto& ref : node["reference"]) {
    out.reference.push_back(ref.as<std::string>());
  }

  return out;
}

template <typename CharT>
std::vector<artifact<CharT>> decode(const YAML::Node& node) {

  std::vector<artifact<CharT>> artifacts;

  for (const auto& entry : node["artifacts"]) {
    artifacts.push_back(decodeArtifact<CharT>(entry));
  }

  return artifacts;
}

DEFINE_string(config, "config.yaml", "Configuration file");
DEFINE_bool(verbose, false, "Verbose output");
DEFINE_bool(debug, false, "Very verbose output");

int main(int argc, char** argv)
{
  try {
    const gflags::Context gfc_(argc, argv);

    if (FLAGS_verbose) {
      set_log_level(LOG_INFO);
    }

    if (FLAGS_debug) {
      set_log_level(LOG_DEBUG);
    }

    const auto artifacts = decode<wchar_t>(YAML::LoadFile(FLAGS_config));

    if (FLAGS_debug) {
      debug(artifacts.size() << " artifacts:");
      for (const auto& artifact : artifacts) {
        debug(" - " << artifact.target);
      }
    }

    if (artifacts.empty()) {
      return 1;
    }

    std::vector<std::future<void>> future;
    future.reserve(artifacts.size());

    const curlpp::Cleanup curlpp_;

    for (const auto& artifact : artifacts) {
      future.emplace_back( std::async([&artifact](){
        process(artifact, [](const auto& line){
          std::wcout << line << std::endl;
        });
      }));
    }
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << std::endl;
  }
  return 0;
}
