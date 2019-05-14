#include <iostream>
#include <unordered_set>
#include <future>

#include "yaml-cpp/yaml.h"
#include "log-reader.hpp"
#include "profile.hpp"
#include "arguments.hpp"

template <typename CharT>
struct patterns {
  std::vector<log::basic_pattern<CharT>> filters;
  std::vector<log::basic_pattern<CharT>> normalizers;
};

template <typename CharT>
struct artifact {
  std::string alias;
  std::string target;
  std::vector<std::string> reference;
  patterns<CharT> rules;
};

template <typename CharT>
class denoiser {
public:
  denoiser(const std::vector<artifact<CharT>>& art) : artifacts(art) {}

  /**
   * Executes the denoising procedure on each artifact
   * @param lambda the lambda that will be invoked for each line emitted
   * @note the signature of the lambda is void lambda(const log::basic_line<CharT>& line)
   * @note the lambda will be invoked in a thread-safe and serialized way in respect of the
   *       line numbering but the order in which the files will be emitted will not necessarely be
   *       the order in which they are listed in the artifact vector.
   */
  template <typename Lambda>
  void run(const Lambda& lambda) {

    profile("all", [&](){
      std::vector<std::future<void>> future;
      future.reserve(artifacts.size());

      for (const auto& artifact : artifacts) {
        future.emplace_back(std::async(std::launch::async, [this, &artifact, &lambda](){
          process(artifact, lambda);
        }));
      }

      for (auto& f : future) {
        f.wait();
      }
    });
  }
private:

  /**
   * Downloads the file and applies filters and normalizers
   * @param url the remote url to download the file from
   * @param alias an alias for the file
   * @param rules there rules to apply to normalize the file
   * @return the file ready for analysis
   */
  log::basic_file<CharT> prepare(const std::string& url, const std::string& alias, const patterns<CharT>& rules) {

    auto file = profile<log::basic_file<CharT>>("downloading " + url, [&](){
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
  }

  /**
   * Use prepare() to download and normalize a log file, and then use it to fill a bucket with
   * its hashes.
   * @param url the remote url to download the file from
   * @param alias an alias for the file
   * @param rules there rules to apply to normalize the file
  */
  void fill_bucket(const std::string& url, const std::string& alias, const patterns<CharT>& rules) {

    auto file = prepare(url, alias, rules);

    std::lock_guard<std::mutex> lock(mutex);

    const auto bucket_size = (file.size() * 3) / 2;

    if(bucket.size() < bucket_size) {
      bucket.reserve(bucket_size);
    }

    for (auto& line : file) {
      bucket.insert(line.hash());
    }
  }

  /**
   * Execute the whole process of downloading and simplifying files, preparing the bucket
   * and performing the final filtering.
   * @param artifact the descriptor of the artifact to analyze
   * @param lambda the lambda that will be invoked for each line emitted
   * @note the signature of the lambda is void lambda(const log::basic_line<CharT>& line)
  */
  template <typename Lambda>
  void process(const artifact<CharT>& artifact, const Lambda& lambda) {

    std::vector<std::future<void>> future;
    future.reserve(artifact.reference.size());

    for (const auto& url : artifact.reference) {
      future.emplace_back(std::async(std::launch::async, [this, &url, &artifact](){
        fill_bucket(url, artifact.alias, artifact.rules);
      }));
    }

    auto file = prepare(artifact.target, artifact.alias, artifact.rules);

    for (const auto& line : file) {
      line.hash();
    }

    for (auto& f : future) {
      f.wait();
    }

    const std::lock_guard<std::mutex> lock(mutex);

    for (const auto& line : file) {
      if (0 == bucket.count(line.hash())) {
        lambda(line);
      }
    }
  }

  const std::vector<artifact<CharT>>& artifacts;
  std::unordered_set<size_t> bucket;
  std::mutex mutex;
  curlpp::Cleanup curlpp_;
};

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

    std::string current = {};

    denoiser<wchar_t>(artifacts).run([&current](const log::wline& line){
      if (current != line.source().name()) {
        if (!current.empty()) {
          std::cout << "--- end " << current << " ---" << std::endl;
        }
        current = line.source().name();
        std::cout << "--- begin " << current << " ---" << std::endl;
      }
      std::wcout << line.number() << " " << line.str() << std::endl;
    });

  } catch (const std::exception& ex) {
    std::cerr << "exception got: " << ex.what() << std::endl << std::flush;
  }
  return 0;
}
