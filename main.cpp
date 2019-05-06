#include <iostream>
#include <unordered_set>
#include <future>

#include "curlpp/Easy.hpp"
#include "yaml-cpp/yaml.h"
#include "log-reader.hpp"

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

class arguments {
public:
  arguments(int argc, char** argv)
    : argc(argc), argv(argv) {}
  bool has_flag(const char* name) const {
    for (int i = 1; i < argc; ++i) {
      if (0 == strcmp(name, argv[i])) {
        return true;
      }
    }
    return false;
  }
  template <typename ...Args>
  bool has_flag(const char* first, Args... more) const {
    return has_flag(first) or has_flag(more...);
  }
  bool has_option(const char* name) const {
    for (int i = 1; i < argc -1; ++i) {
      if (0 == strcmp(name, argv[i])) {
        return true;
      }
    }
    return false;
  }
  template <typename ...Args>
  bool has_option(const char* first, Args... more) const {
    return has_option(first) or has_option(more...);
  }
  std::string_view get_option(const char* name) const {
    for (int i = 1; i < argc; ++i) {
      if (0 == strcmp(name, argv[i])) {
        if (i != argc - 1) {
          return argv[i + 1];
        }
      }
    }
    return {};
  }
  template <typename ...Args>
  std::string_view get_option(const char* first, Args... more) const {
    const auto opt = get_option(first);
    return opt.size() ? opt : get_option(first, more...);
  }

  std::string_view back() const {
    return argv[argc - 1];
  }

  std::string_view front() const {
    return argv[1];
  }

private:
  int argc;
  char** argv;
};

static inline void print_help(const char* self, std::ostream& os) {
  const auto ptr = strrchr(self, '/');
  os << "Usage: " << (ptr ? ptr + 1 : self) << "[OPTIONS]" << std::endl;
  os << "OPTIONS:" << std::endl;
  os << "  --verbose -v: print information regarding the process to stderr" << std::endl;
  os << "  --debug   -d: print even more information to stderr" << std::endl;
  os << "  --config  -c: read the configuration from the given filename" << std::endl;
  os << "  --stdin   - : read the configuration from the std. input stream" << std::endl;
}

int main(int argc, char** argv)
{
  try {

    const arguments args(argc, argv);

    if (args.has_flag("--help", "-h")) {
      print_help(argv[0], std::cout);
      return 0;
    }

    const bool log_debug = args.has_flag("--debug", "-d");
    const bool log_verbose = args.has_flag("--verbose", "-v");
    const bool read_stdin = args.has_flag("--stdin") or args.back() == "-";
    const auto config = args.get_option("--config", "-c");

    if (log_verbose) {
      set_log_level(LOG_INFO);
    }

    if (log_debug) {
      set_log_level(LOG_DEBUG);
    }

    if (not read_stdin and config.empty()) {
      error("Missing argument, --stdin or --config must be speficied");
      print_help(argv[0], std::cerr);
      return 1;
    }

    const auto artifacts = read_stdin
      ? decode<wchar_t>(YAML::LoadFile(std::string(config)))
      : decode<wchar_t>(YAML::Load(std::cin));

    if (not artifacts.empty()) {
      error("Empty configuration");
      return 1;
    }

    if (log_debug) {
      debug(artifacts.size() << " artifacts:");
      for (const auto& artifact : artifacts) {
        debug(" - " << artifact.alias << "(" << artifact.target << ")");
        for (const auto& ref : artifact.reference) {
          debug("   - " << ref);
        }
      }
    }

    profile("all", [&](){

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
    });

  } catch (const std::exception& ex) {
    std::cerr << ex.what() << std::endl;
  }
  return 0;
}
