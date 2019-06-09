#pragma once

#include <iostream>
#include <sstream>

namespace log {

  using level_t = size_t;
  using tid_t = pid_t;

  static constexpr level_t critical = 0x1;
  static constexpr level_t error    = 0x2;
  static constexpr level_t warning  = 0x4;
  static constexpr level_t info     = 0x8;
  static constexpr level_t profile  = 0x10;
  static constexpr level_t debug    = 0x20;

  void enable(level_t lvl);
  void disable(level_t lvl);
  bool has(level_t lvl);

  class line final {
  public:
    line(const char* file, int line, const char* func, std::ostream& os, level_t lvl);
    ~line();
    template <typename T>
    inline line& operator << (const T& t) {
      return ss << t, *this;
    }
    template <typename T>
    inline line& operator () (const T& t) {
      return (*this) << t;
    }
    template <typename T>
    inline line& operator , (const T& t) {
      return (*this) << ", " << t;
    }
  private:
    std::stringstream ss;
    std::ostream& os;
  };
} // log

#define log_cond(x) if (!log::has(x)); else log::line(__FILE__, __LINE__, __FUNCTION__, std::cerr, x)
#define log_error    log_cond(log::error)
#define log_warning  log_cond(log::warning)
#define log_info     log_cond(log::info)
#define log_profile  log_cond(log::profile)
#define log_debug    log_cond(log::debug)

#define enforce(x, ...) do { \
  if (not (x)) { \
    log_error << "assertion error: " #x ", " << __VA_ARGS__; \
    abort(); \
  } \
} while(false)
