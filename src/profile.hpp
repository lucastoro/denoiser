#pragma once

#include <chrono>
#include <string_view>

#include "logging.hpp"

using namespace std::chrono;

template <typename string_type>
class profiler final {
public:
  using clock_type = std::conditional<
    high_resolution_clock::is_steady,
    high_resolution_clock,
    steady_clock
  >::type;

  explicit profiler(const string_type& name) : name(name), start(clock_type::now()) {
  }

  ~profiler() {

    if (not log::has(log::profile)) {
      return;
    }

    const auto dur = clock_type::now() - start;

    if (duration_cast<microseconds>(dur).count() < 1000) {
      log_profile << name << " done in " << duration_cast<microseconds>(dur).count() << " us";
      return;
    }

    if (duration_cast<milliseconds>(dur).count() < 1000) {
      const auto ms = duration_cast<microseconds>(dur).count() / 1000;
      const auto us = duration_cast<microseconds>(dur).count() % 1000;
      log_profile << name << " done in " << ms << "ms " << us << " us";
      return;
    }

    if (duration_cast<seconds>(dur).count() < 60) {
      const auto sec = duration_cast<milliseconds>(dur).count() / 1000;
      const auto ms = duration_cast<milliseconds>(dur).count() % 1000;
      log_profile << name << " done in " << sec << "sec " << ms << " ms";
      return;
    }

    const auto min = duration_cast<seconds>(dur).count() / 60;
    const auto sec = duration_cast<seconds>(dur).count() % 60;

    log_profile << name << " done in " << min << " min " << sec << " sec";
  }
private:
  profiler(const profiler&) = delete;
  profiler(profiler&&) = delete;
  profiler& operator = (const profiler&) = delete;
  profiler& operator = (profiler&&) = delete;
  const string_type& name;
  clock_type::time_point start;
};

template <typename R, typename T>
static inline R profile(const std::string_view& name, const T& lambda) {
  const profiler prof(name);
  return lambda();
}

template <typename T>
static inline void profile(const std::string& name, const T& lambda) {
  const profiler prof(name);
  lambda();
}
