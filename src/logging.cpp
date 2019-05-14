#include "logging.hpp"

#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>

static log_level_t log_level = LOG_DEFAULT;

void log_enable(log_level_t lvl) {
  log_level |= lvl;
}

void log_disable(log_level_t lvl) {
  log_level &= ~lvl;
}

bool log_has(log_level_t lvl) {
  return log_level & lvl;
}

const char* log_gettime() {
  thread_local char temp[128] = "";
  struct timeval tv;
  struct tm tm;
  gettimeofday(&tv, nullptr);
  localtime_r(&tv.tv_sec, &tm);
  snprintf(temp, sizeof(temp), "%02d:%02d:%02d.%03ld", tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_usec / 1000);
  return temp;
}

tid_t log_gettid() {
  return tid_t(syscall(SYS_gettid));
}
