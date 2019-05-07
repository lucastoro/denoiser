#pragma once

#include <chrono>
#include <string_view>

#include "logging.hpp"

using namespace std::chrono;
using profile_clock = std::conditional<
  high_resolution_clock::is_steady,
  high_resolution_clock,
  steady_clock
>::type;

template <typename N, typename A, typename B>
static void profile_eval(const N& name, const std::chrono::duration<A,B>& dur) {

  if (duration_cast<microseconds>(dur).count() < 1000) {
    log_profile(name << " done in " << duration_cast<microseconds>(dur).count() << " us");
    return;
  }

  if (duration_cast<milliseconds>(dur).count() < 1000) {
    const auto ms = duration_cast<microseconds>(dur).count() / 1000;
    const auto us = duration_cast<microseconds>(dur).count() % 1000;
    log_profile(name << " done in " << ms << "ms " << us << " us");
    return;
  }

  if (duration_cast<seconds>(dur).count() < 60) {
    const auto sec = duration_cast<milliseconds>(dur).count() / 1000;
    const auto ms = duration_cast<milliseconds>(dur).count() % 1000;
    log_profile(name << " done in " << sec << "sec " << ms << " ms");
    return;
  }

  const auto min = duration_cast<seconds>(dur).count() / 60;
  const auto sec = duration_cast<seconds>(dur).count() % 60;

  log_profile(name << " done in " << min << " min " << sec << " sec");
}

template <typename R, typename T>
static inline R profile(const std::string_view& name, const T& lambda) {

  if (0 == (log_level & LOG_PROFILE)) {
      return lambda();
  }

  const auto a = profile_clock::now();
  R r = lambda();
  profile_eval(name, profile_clock::now() - a);
  return r;
}

template <typename T>
static inline void profile(const std::string& name, const T& lambda) {

  if (0 == (log_level & LOG_PROFILE)) {
      return lambda();
  }

  const auto a = profile_clock::now();
  lambda();
  profile_eval(name, profile_clock::now() - a);
}
