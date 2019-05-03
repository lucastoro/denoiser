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
  std::string alias;
  std::string target;
  std::vector<std::string> reference;
  denoiser<CharT> rules;
};

template <typename CharT>
struct Line {
  Line(const std::basic_string_view<CharT>& c, size_t n, const std::string& s, size_t t)
    : content(c), number(n), source(s), total(t) {}
  std::basic_string_view<CharT> content;
  size_t number;
  const std::string& source;
  size_t total;
};

template <typename R, typename T>
static inline R profile(const std::string& name, const T& lambda) {

  using namespace std::chrono;
  using clock = steady_clock;

  const auto a = clock::now();
  R r = lambda();
  const auto b = clock::now();

  if (duration_cast<microseconds>(b - a).count() < 1000) {
    debug(name << " done in " << duration_cast<microseconds>(b - a).count() << " us");
    return r;
  }

  if (duration_cast<milliseconds>(b - a).count() < 1000) {
    const auto ms = duration_cast<microseconds>(b - a).count() / 1000;
    const auto us = duration_cast<microseconds>(b - a).count() % 1000;
    debug(name << " done in " << ms << "." << us << " ms");
    return r;
  }

  if (duration_cast<seconds>(b - a).count() < 60) {
    const auto sec = duration_cast<milliseconds>(b - a).count() / 1000;
    const auto ms = duration_cast<milliseconds>(b - a).count() % 1000;
    debug(name << " done in " << sec << "." << ms << " sec");
    return r;
  }

  const auto min = duration_cast<seconds>(b - a).count() / 60;
  const auto sec = duration_cast<seconds>(b - a).count() % 60;

  debug(name << " done in " << min << " min, " << sec << " sec");
  return r;
}

template <typename T>
static inline void profile(const std::string& name, const T& lambda) {

  using namespace std::chrono;
  using clock = steady_clock;

  const auto a = clock::now();
  lambda();
  const auto b = clock::now();

  if (duration_cast<microseconds>(b - a).count() < 1000) {
    debug(name << " done in " << duration_cast<microseconds>(b - a).count() << " us");
    return;
  }

  if (duration_cast<milliseconds>(b - a).count() < 1000) {
    const auto ms = duration_cast<microseconds>(b - a).count() / 1000;
    const auto us = duration_cast<microseconds>(b - a).count() % 1000;
    debug(name << " done in " << ms << "." << us << " ms");
    return;
  }

  if (duration_cast<seconds>(b - a).count() < 60) {
    const auto sec = duration_cast<milliseconds>(b - a).count() / 1000;
    const auto ms = duration_cast<milliseconds>(b - a).count() % 1000;
    debug(name << " done in " << sec << "." << ms << " sec");
    return;
  }

  const auto min = duration_cast<seconds>(b - a).count() / 60;
  const auto sec = duration_cast<seconds>(b - a).count() % 60;

  debug(name << " done in " << min << " min, " << sec << " sec");
}

static std::mutex global_lock;

template <typename CharT, typename Lambda>
void process(
    const artifact<CharT>& artifact,
    Lambda lambda) {

  profile("processing " + artifact.alias, [&](){

    std::unordered_set<size_t> bucket;
    std::mutex mutex;

    const auto prepare = [](
        const std::string& url,
        const std::string& alias,
        const denoiser<CharT>& rules
        ) -> log::file<CharT> {

      auto file = profile<log::file<CharT>>("downloading " + alias, [&](){
        curlpp::Easy request;
        request.setOpt(curlpp::options::Url(url));
        return log::file<CharT>(request);
      });

      profile("filtering " + alias, [&](){
        for (const auto& pattern : rules.filters) {
          for (auto& line : file) {
            if (line.contains(pattern)) {
              line.suppress();
            }
          }
        }
      });

      profile("normalizing " + alias, [&](){
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
        const std::string& alias,
        const denoiser<CharT>& rules
        ) -> void {

      auto file = prepare(url, alias, rules);

      profile("filling the bucket " + alias, [&](){
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
      f_refs.emplace_back(std::async(do_process, ref, artifact.alias, artifact.rules) );
    }

    const auto file = prepare(artifact.target, artifact.alias, artifact.rules);

    for (const auto& f_ref : f_refs) {
      f_ref.wait();
    }

    const size_t total = file.size();
    size_t current = 0;

    profile("calculating hashes for " + artifact.alias, [&](){
      for (const auto& line : file) {
        line.hash();
      }
    });

    global_lock.lock();

    for (const auto& line : file) {
      ++current;
      if (0 == bucket.count(line.hash())) {
        lambda(Line<CharT>(line.str(), current, artifact.alias, total));
      }
    }

    global_lock.unlock();
  });
}

template <typename CharT>
artifact<CharT> decodeArtifact(const YAML::Node& node) {

  artifact<CharT> out;

  out.alias = node["alias"].as<std::string>();
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
          debug(" - " << artifact.alias << "(" << artifact.target << ")");
          for (const auto& ref : artifact.reference) {
            debug("   - " << ref);
          }
        }
      }

      if (not artifacts.empty()) {

        std::vector<std::future<void>> future;
        future.reserve(artifacts.size());

        const curlpp::Cleanup curlpp_;

        std::string current;

        for (const auto& artifact : artifacts) {
          future.emplace_back( std::async([&artifact, &current](){
            process(artifact, [&current](const auto& line){
              if (current != line.source) {
                if (!current.empty()) {
                  std::cout << "--- end " << current << " ---" << std::endl;
                }
                current = line.source;
                std::cout << "--- begin " << current << " ---" << std::endl;
              }
              std::wcout << line.number << " " << line.content << std::endl;
            });
          }));
        }

        if (!current.empty()) {
          std::cout << "--- end " << current << " ---" << std::endl;
        }
      }
    });

  } catch (const std::exception& ex) {
    std::cerr << ex.what() << std::endl;
  }
  return 0;
}
