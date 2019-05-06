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
  std::vector<log::pattern<CharT>> filters;
  std::vector<log::pattern<CharT>> normalizers;
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
typename std::enable_if<not std::is_same<char,CharT>::value, std::basic_string<CharT>>::type
static inline convert(const std::string& str) {
  std::basic_string<CharT> ret;
  for (char c : str) {
    ret.push_back(CharT(c));
  }
  return ret;
}

template <typename CharT>
typename std::enable_if<std::is_same<char,CharT>::value, const std::string&>::type
static inline convert(const std::string& str) {
  return str;
}

template <typename CharT>
artifact<CharT> decodeArtifact(const YAML::Node& node) {

  artifact<CharT> out;

  out.alias = node["alias"].as<std::string>();
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

  const auto extract_patterns = [&node](const char* name) -> std::vector<log::pattern<CharT>> {
    std::vector<log::pattern<CharT>> list;
    for (const auto& entry : node[name]) {
      const auto r = entry["r"];
      if (not r.IsNull()) {
        list.emplace_back(std::basic_regex<CharT>(convert<CharT>(r.as<std::string>())));
      } else {
        list.emplace_back(convert<CharT>(entry["s"].as<std::string>()));
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
