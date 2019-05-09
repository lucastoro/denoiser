#pragma once

#include <iostream>
#include <sstream>
#include <cstddef>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/time.h>

#define LOG_CRITICAL 0x1
#define LOG_ERROR    0x2
#define LOG_WARNING  0x4
#define LOG_INFO     0x8
#define LOG_PROFILE  0x10
#define LOG_DEBUG    0x20
#define LOG_DEFAULT  (LOG_CRITICAL|LOG_ERROR|LOG_WARNING)

extern size_t log_level;

using tid_t = pid_t;

#define gettid() syscall(SYS_gettid)

static inline const char* gettime() {
  thread_local char temp[128] = "";

  struct timeval tv;
  struct tm tm;
  gettimeofday(&tv, nullptr);
  localtime_r(&tv.tv_sec, &tm);

  snprintf(temp, sizeof(temp), "%02d:%02d:%02d.%03ld", tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_usec / 1000);

  return temp;
}

#define logging(file, PREFIX, lvl, ...) do { \
  if(log_level & lvl) { \
    std::stringstream ss; \
    ss << PREFIX " T" << gettid() << " " << gettime() << " | " << __VA_ARGS__ << std::endl; \
    file << ss.str() << std::flush; \
  } \
} while(false)

#define log_critical(...) do { logging(std::cerr, "[ABORT]", LOG_CRITICAL, __VA_ARGS__); abort(); } while (false)
#define log_error(...)   logging(std::cerr, "[ERROR]", LOG_ERROR, __VA_ARGS__)
#define log_warning(...) logging(std::cerr, "[WARN.]", LOG_WARNING, __VA_ARGS__)
#define log_info(...)    logging(std::cerr, "[INFO.]", LOG_INFO, __VA_ARGS__)
#define log_profile(...) logging(std::cerr, "[PROF.]", LOG_PROFILE, __VA_ARGS__)
#define log_debug(...)   logging(std::cerr, "[DEBUG]", LOG_DEBUG, __VA_ARGS__)

#define enforce(x, ...) do { if (not (x)) log_critical( "assertion error: " #x ", " << __VA_ARGS__); } while(false)
