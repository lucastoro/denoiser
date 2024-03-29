#pragma once

#include "artifact.hpp"
#include "profile.hpp"
#include "config.hpp"

#include <vector>
#include <unordered_set>
#include <future>

#define USE_THREAD_POOL 1

#ifdef WITH_THREAD_POOL
#include "thread-pool.hpp"
#endif

template <typename CharT>
class denoiser {
public:
  explicit denoiser(const configuration<CharT>& art) : config(art) {}

  /**
   * Executes the whole process of downloading and simplifying files, preparing the bucket
   * and performing the final filtering.
   * \param artifact the descriptor of the artifact to analyze
   * \param lambda the lambda that will be invoked for each line emitted
   * \note the signature of the lambda is void lambda(const artifact::basic_line<CharT>& line)
  */
  template <typename Lambda>
  void run(const Lambda& lambda) {

    profile("all", [&](){

      std::vector<std::future<void>> future;
      future.reserve(config.reference.size());
      size_t count = 0;
      for (const auto& url : config.reference) {
        future.emplace_back(std::async(std::launch::async, [this, &url, &count](){
          fill_bucket(url, config.rules);
        }));
      }

      auto file = prepare(config.target, config.rules);

      for (auto& f : future) {
        f.wait();
      }

      profile("output", [&](){
        for (const auto& line : file) {
          if (0 == bucket.count(line.hash())) {
            lambda(line);
          }
        }
      });
    });
  }

private:

  /**
   * Downloads the file and applies filters and normalizers
   * \param url the remote url to download the file from
   * \param rules there rules to apply to normalize the file
   * \return the file ready for analysis
   */
  artifact::basic_file<CharT> prepare(const std::string& url,
                                      const patterns<CharT>& rules) {

    artifact::basic_file<CharT> file;

    profile("fetching " + url, [&](){
      file = artifact::basic_file<CharT>::fetch(url);
    });

    profile("filtering " + url, [&](){
      filter(file, rules);
    });

    profile("normalizing " + url, [&](){
      normalize(file, rules);
    });

    profile("calculating hashes for " + url, [&](){
      compute_hashes(file);
    });

    return file;
  }

  /**
   * Uses prepare() to download and normalize a log file, and then uses it to fill a bucket with
   * its hashes.
   * \param url the remote url to download the file from
   * \param rules there rules to apply to normalize the file
  */
  void fill_bucket(const std::string& url,
                   const patterns<CharT>& rules) {

    const auto file = prepare(url, rules);

    // access to the bucket must be synchronized
    std::lock_guard<std::mutex> lock(mutex);

    const auto bucket_size = (file.size() * 3) / 2;

    if(bucket.size() < bucket_size) {
      bucket.reserve(bucket_size);
    }

    for (auto& line : file) {
      bucket.insert(line.hash());
    }
  }

#if USE_THREAD_POOL

  template <typename Container, typename Lambda>
  void loop(Container& container, const Lambda& lambda) {
    pool.for_each(container, 1000, lambda);
  }

#else

  template <typename Container, typename Lambda>
  void loop(Container& container, const Lambda& lambda) {
    for (auto& entry : container) {
      lambda(entry);
    }
  }

#endif

  void filter(artifact::basic_file<CharT>& file, const patterns<CharT>& rules) {
    loop(file, [&rules](auto& line){
      for (const auto& pattern : rules.filters) {
        line.suppress(pattern);
      }
    });
  }

  void normalize(artifact::basic_file<CharT>& file, const patterns<CharT>& rules) {
    loop(file, [&rules](auto& line){
      for (const auto& pattern : rules.normalizers) {
        line.remove(pattern);
      }
    });
  }

  void compute_hashes(artifact::basic_file<CharT>& file) {
    for (const auto& line : file) {
      line.hash(); // here I'm spoiling the hash caching of artifact::basic_line
    }
  }

  const configuration<CharT>& config;
  std::unordered_set<size_t> bucket;
  std::mutex mutex;
  curlpp::Cleanup curlpp_;
#if USE_THREAD_POOL
  thread_pool pool;
#endif
};
