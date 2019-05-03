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
struct denoiser {
  std::vector<std::basic_regex<CharT>> filters;
  std::vector<std::basic_regex<CharT>> normalizers;
};

template <typename CharT>
struct artifact {
  std::string target;
  std::vector<std::string> reference;
  denoiser<CharT> rules;
};

template <typename CharT>
struct Line {
  Line(const std::basic_string_view<CharT>& c, size_t n, size_t t)
    : content(c), number(n), total(t) {}
  std::basic_string_view<CharT> content;
  size_t number;
  size_t total;
};

template <typename T, typename I = std::chrono::milliseconds>
static inline void profile(const char* name, const T& lambda) {
  const auto a = std::chrono::steady_clock::now();
  lambda();
  const auto b = std::chrono::steady_clock::now();
  debug(name << " done in " << std::chrono::duration_cast<I>(b - a).count() << " ms");
}

static std::mutex global_lock;

template <typename CharT, typename Lambda>
void process(
    const artifact<CharT>& artifact,
    Lambda lambda) {

  std::unordered_set<size_t> bucket;
  std::mutex mutex;

  const auto prepare = [](
      const std::string& url,
      const denoiser<CharT>& rules
      ) -> log::file<CharT> {

    debug("downloading " << url << "...");
    curlpp::Easy request;
    request.setOpt(curlpp::options::Url(url));
    log::file<CharT> file(request);

    profile("filtering", [&](){
      for (const auto& pattern : rules.filters) {
        for (auto& line : file) {
          if (line.contains(pattern)) {
            line.suppress();
          }
        }
      }
    });

    profile("normalizing", [&](){
      for (const auto& pattern : rules.normalizers) {
        for (auto& line : file) {
          line.remove(pattern);
        }
      }
    });

    return file;
  };

  const auto do_process = [&bucket, &mutex, &prepare](
      const std::string& url,
      const denoiser<CharT>& rules
      ) -> void {

    auto file = prepare(url, rules);

    profile("filling the bucket", [&](){
      std::lock_guard<std::mutex> lock(mutex);
      const auto bucket_size = (file.size() * 3) / 2;
      if(bucket.size() < bucket_size) {
        bucket.reserve(bucket_size);
      }
      for (auto& line : file) {
        bucket.insert(line.hash());
      }
    });
  };

  std::vector<std::future<void>> f_refs;
  f_refs.reserve(artifact.reference.size());
  for (const auto& ref : artifact.reference) {
    f_refs.emplace_back(std::async(do_process, ref, artifact.rules) );
  }

  const auto file = prepare(artifact.target, artifact.rules);

  for (const auto& f_ref : f_refs) {
    f_ref.wait();
  }

  const size_t total = file.size();
  size_t current = 0;

  profile("calculating hashes", [&](){
    for (const auto& line : file) {
      line.hash();
    }
  });

  global_lock.lock();

  debug("outputting: " << artifact.target);

  for (const auto& line : file) {
    ++current;
    if (0 == bucket.count(line.hash())) {
      lambda(Line<CharT>(line.str(), current, total));
    }
  }

  global_lock.unlock();
}

template <typename CharT>
artifact<CharT> decodeArtifact(const YAML::Node& node) {

  artifact<CharT> out;

  out.target = node["target"].as<std::string>();

  for (const auto& ref : node["reference"]) {
    out.reference.push_back(ref.as<std::string>());
  }

  {

    static constexpr const wchar_t* filters[] = {
      LR"(Seen branch in repository)",
      LR"(Checking out Revision)",
      LR"(> git checkout)",
      LR"(Commit message:)",
      LR"(Notified Stash for commit with id)",
      LR"(docker exec --tty --user 0:0)",
      LR"(No custom packages to build)",
      LR"(INFO:)",
      LR"(^Fetched)",
      LR"(^\s*Active:)",
      LR"(^\s*Main PID:)",
      LR"(^\s*Tasks:)",
      LR"(^\s*Memory:)",
      LR"(^\s*CPU:)",
      LR"(^\s*[├└]─\d+)",
      LR"((ECDSA) to the list of known hosts)",
      LR"(/\s+#{10,}$)",
      LR"(You can use following commands to access the host)",
      LR"(HISTCONTROL=ignorespace)",
      LR"(cat >/tmp/sshKey<<EOF)",
      LR"(-----BEGIN RSA PRIVATE KEY-----)",
      LR"(\s+[\w+\/]{52,}$)",
      LR"(-----END RSA PRIVATE KEY-----)",
      LR"(EOF)",
      LR"(chmod 600 /tmp/sshKey)",
      LR"(ssh -oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null -i/tmp/sshKey -lroot)",
    };

    static constexpr const wchar_t* normalizers[] = {
      LR"([a-zA-Z]{3} \d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2} CET)",
      LR"(\d{2}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})",
      LR"([a-zA-Z]{3} [a-zA-Z]{3} \d{1,2} \d{2}:\d{2}:\d{2} \d{4})",
      LR"([A-Z][a-z]{2} \d{2} \d{2}:\d{2}:\d{2})",
      LR"(\[\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z\])",
      LR"(\d{2}:\d{2}:\d{2})",
      LR"((?:[A-F0-9]{2}:){5}[A-F0-9]{2})", // FA:16:3E:B8:82:61
      LR"(JENKINS-\w+-\w+-\w+-\d+)",
      LR"(http:\/\/jenkins\.brightcomputing\.com\/job\/[^\s]+)",
      LR"([0-9a-f-]{32,128})",
      LR"((?:\d{1,3}\.){3}\d{1,3}(?:\\/\d{1,2})?)",
      LR"(Seen \d+ remote branches)",
      LR"(build number \d+)",
      LR"(ci-node\d{3})",
      LR"(IMAGE:\s+[^:]+:\d+)",
      LR"(hyper\d+)",
      LR"(dev[-:]\d+)",
      LR"(\[\d+(?:[,.][\d]+)? k?B\])",
      LR"(Get:\d+)",
      LR"(cmd\[\d+\])",
      LR"([TaskInitializer::reinitialize], automatic: \d+)"
    };

    out.rules.filters.reserve(std::size(filters));
    out.rules.normalizers.reserve(std::size(normalizers));

    for (const wchar_t* pattern : filters) {
      out.rules.filters.emplace_back(pattern, std::regex::optimize);
    }

    for (const wchar_t* pattern : normalizers) {
      out.rules.normalizers.emplace_back(pattern, std::regex::optimize);
    }
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

    profile("all", [&](){
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

      if (not artifacts.empty()) {

        std::vector<std::future<void>> future;
        future.reserve(artifacts.size());

        const curlpp::Cleanup curlpp_;

        for (const auto& artifact : artifacts) {
          future.emplace_back( std::async([&artifact](){
            process(artifact, [](const auto& line){
              std::wcout << line.number << " " << line.content << std::endl;
            });
          }));
        }
      }
    });

  } catch (const std::exception& ex) {
    std::cerr << ex.what() << std::endl;
  }
  return 0;
}
