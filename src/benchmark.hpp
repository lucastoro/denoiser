#pragma once

#include <chrono>

class Benchmark
{
public:
  inline Benchmark() : start( clock::now() ) {}

  template <typename T>
  inline T reset()
  {
    const auto now = clock::now();
    const auto ret = std::chrono::duration_cast<T>(now - start);
    start = now;
    return ret;
  }

  template <typename T>
  inline void reset( T& v )
  {
    v = reset<T>();
  }
private:
  using clock = std::chrono::steady_clock;
  using time_point = clock::time_point;
  time_point start;
};
