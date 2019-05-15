#pragma once

#include "log-reader.hpp"
#include "profile.hpp"
#include "config.hpp"

#include <vector>
#include <unordered_set>
#include <future>

template <typename CharT>
class denoiser {
public:
  denoiser(const std::vector<configuration<CharT>>& art) : artifacts(art) {}

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
      future.reserve(artifacts.size() - 1);

      for (size_t i = 0; i < artifacts.size() - 1; ++i) {
        future.emplace_back(std::async(std::launch::async, [this, i, &lambda](){
          process(artifacts[i], lambda);
        }));
      }

      process(artifacts.back(), lambda);

      for (auto& f : future) {
        f.wait();
      }
    });
  }

private:

  /**
   * Execute the whole process of downloading and simplifying files, preparing the bucket
   * and performing the final filtering.
   * @param artifact the descriptor of the artifact to analyze
   * @param lambda the lambda that will be invoked for each line emitted
   * @note the signature of the lambda is void lambda(const log::basic_line<CharT>& line)
  */
  template <typename Lambda>
  void process(const configuration<CharT>& artifact, const Lambda& lambda) {

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

    /* the output operation must be serialized to avoid
       interleaving lines coming from different files */

    const std::lock_guard<std::mutex> lock(mutex);

    for (const auto& line : file) {
      if (0 == bucket.count(line.hash())) {
        lambda(line);
      }
    }
  }

  /**
   * Use prepare() to download and normalize a log file, and then use it to fill a bucket with
   * its hashes.
   * @param url the remote url to download the file from
   * @param alias an alias for the file
   * @param rules there rules to apply to normalize the file
  */
  void fill_bucket(const std::string& url, const std::string& alias, const patterns<CharT>& rules) {

    const auto file = prepare(url, alias, rules);

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
#if 0
#else
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
#endif
    return file;
  }

  const std::vector<configuration<CharT>>& artifacts;
  std::unordered_set<size_t> bucket;
  std::mutex mutex;
  curlpp::Cleanup curlpp_;
};
