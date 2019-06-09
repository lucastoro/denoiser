#include "logging.hpp"

#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>

namespace log {

static level_t log_level = (critical|error);

void enable(level_t lvl) {
  log_level |= lvl;
}

void disable(level_t lvl) {
  log_level &= ~lvl;
}

bool has(level_t lvl) {
  return log_level & lvl;
}

const char* log_gettime() {
  thread_local char temp[128] = "";
  struct timeval tv;
  struct tm tm;
  gettimeofday(&tv, nullptr);
  localtime_r(&tv.tv_sec, &tm);
  snprintf(
    temp,
    sizeof(temp),
    "%02d:%02d:%02d.%03ld",
    tm.tm_hour,
    tm.tm_min,
    tm.tm_sec,
    tv.tv_usec / 1000
  );
  return temp;
}

tid_t log_gettid() {
  return tid_t(syscall(SYS_gettid));
}

line::line(const char* file,
                  int line,
                  const char* func,
                  std::ostream& os,
                  level_t lvl)
    : os(os) {
  const char* prefix =
      lvl == log::critical ? "[ABORT]" :
      lvl == log::error    ? "[ERROR]" :
      lvl == log::warning  ? "[WARN.]" :
      lvl == log::info     ? "[INFO.]" :
      lvl == log::profile  ? "[PROF.]" :
      lvl == log::debug    ? "[DEBUG]" : "[?????]";

  if (lvl == log::debug) {
    ss << prefix << " T" << log_gettid()
       << " @" << file << ":" << line
       << " " << func
       << " " << log_gettime() << " | ";
  } else {
    ss << prefix << " T" << log_gettid() << " " << log_gettime() << " | ";
  }
}

line::~line() {
  ss << std::endl;
  os << ss.str();
}

} // log
