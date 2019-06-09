#pragma once

#include <iostream>
#include <sstream>

using log_level_t = size_t;
using tid_t = pid_t;

#define LOG_CRITICAL log_level_t(0x1)
#define LOG_ERROR    log_level_t(0x2)
#define LOG_WARNING  log_level_t(0x4)
#define LOG_INFO     log_level_t(0x8)
#define LOG_PROFILE  log_level_t(0x10)
#define LOG_DEBUG    log_level_t(0x20)
#define LOG_DEFAULT  log_level_t(LOG_CRITICAL|LOG_ERROR)
#define LOG_ALL      log_level_t(0xFFFFFFFF)
#define LOG_NONE     log_level_t(0)

void log_enable(log_level_t lvl);
void log_disable(log_level_t lvl);
bool log_has(log_level_t lvl);
const char* log_gettime();
tid_t log_gettid();

template <typename C>
class log_line {
public:
  explicit log_line(const char* /* file */,
                    int /* line */,
                    const char* /* func */,
                    std::basic_ostream<C>& os,
                    log_level_t lvl) : os(os), enabled(log_has(lvl)) {
    if(enabled) {
      const char* prefix =
          lvl == LOG_CRITICAL ? "[ABORT]" :
          lvl == LOG_ERROR    ? "[ERROR]" :
          lvl == LOG_WARNING  ? "[WARN.]" :
          lvl == LOG_INFO     ? "[INFO.]" :
          lvl == LOG_PROFILE  ? "[PROF.]" :
          lvl == LOG_DEBUG    ? "[DEBUG]" : "[?????]";

      ss << prefix << " T" << log_gettid() << " " << log_gettime() << " | ";
    }
  }
  ~log_line() {
    if(enabled) {
      ss << std::endl;
      os << ss.str();
    }
  }
  template <typename T>
  log_line& operator << (const T& t) {
    if(enabled) {
      ss << t;
    }
    return *this;
  }
  template <typename T>
  log_line& operator () (const T& t) {
    return (*this) << t;
  }
  template <typename T>
  log_line& operator , (const T& t) {
    return (*this) << ", " << t;
  }
private:
  std::basic_stringstream<C> ss;
  std::basic_ostream<C>& os;
  bool enabled;
};

#define log_error    log_line<char>(__FILE__, __LINE__, __FUNCTION__, std::cerr, LOG_ERROR)
#define log_warning  log_line<char>(__FILE__, __LINE__, __FUNCTION__, std::cerr, LOG_WARNING)
#define log_info     log_line<char>(__FILE__, __LINE__, __FUNCTION__, std::cerr, LOG_INFO)
#define log_profile  log_line<char>(__FILE__, __LINE__, __FUNCTION__, std::cerr, LOG_PROFILE)
#define log_debug    log_line<char>(__FILE__, __LINE__, __FUNCTION__, std::cerr, LOG_DEBUG)

#define enforce(x, ...) do { \
  if (not (x)) { \
    log_error << "assertion error: " #x ", " << __VA_ARGS__; \
    abort(); \
  } \
} while(false)
