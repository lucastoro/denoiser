#pragma once

#include <chrono>
#include <string_view>

#include "logging.hpp"

extern bool enable_profile;

template <typename R, typename T>
static inline R profile(const std::string_view& name, const T& lambda) {

  if (not enable_profile) {
      return lambda();
  }

  using namespace std::chrono;
  using clock = steady_clock;

  const auto a = clock::now();
  R r = lambda();
  const auto b = clock::now();

  if (duration_cast<microseconds>(b - a).count() < 1000) {
    info(name << " done in " << duration_cast<microseconds>(b - a).count() << " us");
    return r;
  }

  if (duration_cast<milliseconds>(b - a).count() < 1000) {
    const auto ms = duration_cast<microseconds>(b - a).count() / 1000;
    const auto us = duration_cast<microseconds>(b - a).count() % 1000;
    info(name << " done in " << ms << "." << us << " ms");
    return r;
  }

  if (duration_cast<seconds>(b - a).count() < 60) {
    const auto sec = duration_cast<milliseconds>(b - a).count() / 1000;
    const auto ms = duration_cast<milliseconds>(b - a).count() % 1000;
    info(name << " done in " << sec << "." << ms << " sec");
    return r;
  }

  const auto min = duration_cast<seconds>(b - a).count() / 60;
  const auto sec = duration_cast<seconds>(b - a).count() % 60;

  info(name << " done in " << min << " min, " << sec << " sec");
  return r;
}
