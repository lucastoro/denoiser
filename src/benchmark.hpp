#pragma once

#include <chrono>

template <typename T>
class Benchmark final
{
public:
  inline Benchmark(T& ref)
    : start( clock::now() ), ref(ref) {
    ref = T();
  }
  ~Benchmark() {
    const auto now = clock::now();
    const auto ret = std::chrono::duration_cast<T>(now - start);
    ref = std::chrono::duration_cast<T>(now - start);
  }

private:
  using clock = std::chrono::steady_clock;
  using time_point = clock::time_point;
  time_point start;
  T& ref;

};
