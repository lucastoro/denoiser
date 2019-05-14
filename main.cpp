#include <iostream>
#include <unordered_set>
#include <future>

#include "yaml-cpp/yaml.h"
#include "log-reader.hpp"
#include "profile.hpp"
#include "arguments.hpp"

template <typename CharT>
struct denoiser {
  std::vector<log::basic_pattern<CharT>> filters;
  std::vector<log::basic_pattern<CharT>> normalizers;
};

template <typename CharT>
struct artifact {
  std::string alias;
  std::string target;
  std::vector<std::string> reference;
  denoiser<CharT> rules;
};

static std::mutex global_lock;

template <typename CharT>
log::basic_file<CharT> prepare(
    const std::string& url,
    const std::string& alias,
    const denoiser<CharT>& rules
    ) {

  auto file = profile<log::basic_file<CharT>>("downloading " + alias, [&](){
    return log::basic_file<CharT>::download(url, alias);
  });

  profile("filtering " + alias, [&](){
    for (const auto& pattern : rules.filters) {
      for (auto& line : file) {
        line.suppress(pattern);
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

template <typename CharT, typename Lambda>
void process(
    const artifact<CharT>& artifact,
    Lambda lambda) {

  profile("processing " + artifact.alias, [&](){

    std::unordered_set<size_t> bucket;
    std::mutex mutex;

    const auto do_process = [&bucket, &mutex](
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
      f_refs.emplace_back(std::async(std::launch::async, do_process, ref, artifact.alias, artifact.rules) );
    }

    const auto file = prepare(artifact.target, artifact.alias, artifact.rules);

    for (const auto& f_ref : f_refs) {
      f_ref.wait();
    }

    size_t current = 0;

    profile("calculating hashes for " + artifact.alias, [&](){
      for (const auto& line : file) {
        line.hash();
      }
    });

    const std::lock_guard<std::mutex> lock(global_lock);

    for (const auto& line : file) {
      ++current;
      if (0 == bucket.count(line.hash())) {
        lambda(line);
      }
    }
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

  const auto extract_patterns = [&node](const char* name) -> std::vector<log::basic_pattern<CharT>> {
    std::vector<log::basic_pattern<CharT>> list;
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

static inline void print_help(const char* self, std::ostream& os) {
  const auto ptr = strrchr(self, '/');
  os << "Usage: " << (ptr ? ptr + 1 : self) << "[OPTIONS]" << std::endl;
  os << "OPTIONS:" << std::endl;
  os << "  --config  -c: read the configuration from the given filename" << std::endl;
  os << "  --stdin   - : read the configuration from the input stream" << std::endl;
  os << "  --verbose -v: print information regarding the process to stderr" << std::endl;
  os << "  --debug   -d: print even more information to stderr" << std::endl;
}

int main(int argc, char** argv)
{
  try {

    const arguments args(argc, argv);

    if (args.flag("--help", "-h")) {
      print_help(argv[0], std::cout);
      return 0;
    }

    if (args.flag("--verbose", "-v")) {
      log_enable(LOG_INFO);
    }

    if (args.flag("--debug", "-d")) {
      log_enable(LOG_DEBUG);
    }

    if (args.flag("--profile", "-p")) {
      log_enable(LOG_PROFILE);
    }

    const bool read_stdin = args.flag("--stdin") or args.back() == "-";
    const auto config = args.get("--config", "-c");

    if (not read_stdin and config.empty()) {
      log_error("Missing argument, --stdin or --config must be speficied");
      print_help(argv[0], std::cerr);
      return 1;
    }

    if (read_stdin and config.size()) {
      log_error("Invalid arguments, cannot specify both --stdin and --config");
      print_help(argv[0], std::cerr);
      return 1;
    }

    const auto artifacts = read_stdin
      ? decode<wchar_t>(YAML::Load(std::cin))
      : decode<wchar_t>(YAML::LoadFile(std::string(config)));

    if (artifacts.empty()) {
      log_error("Empty configuration");
      return 1;
    }

    if (log_has(LOG_DEBUG)) {
      log_debug(artifacts.size() << " artifacts:");
      for (const auto& artifact : artifacts) {
        log_debug(" - " << artifact.alias << "(" << artifact.target << ")");
        for (const auto& ref : artifact.reference) {
          log_debug("   - " << ref);
        }
      }
    }

    profile("all", [&](){

      std::vector<std::future<void>> future;
      future.reserve(artifacts.size());

      const curlpp::Cleanup curlpp_;

      std::string current;

      for (const auto& artifact : artifacts) {
        future.emplace_back(std::async(std::launch::async, [&artifact, &current](){
          process(artifact, [&current](const auto& line){
            if (current != line.source().name()) {
              if (!current.empty()) {
                std::cout << "--- end " << current << " ---" << std::endl;
              }
              current = line.source().name();
              std::cout << "--- begin " << current << " ---" << std::endl;
            }
            std::wcout << line.number() << " " << line.str() << std::endl;
          });
        }));
      }

      for (auto& f : future) {
        f.wait();
      }

      if (!current.empty()) {
        std::cout << "--- end " << current << " ---" << std::endl;
      }
    });

  } catch (const std::exception& ex) {
    std::cerr << "exception got: " << ex.what() << std::endl << std::flush;
  }
  return 0;
}
