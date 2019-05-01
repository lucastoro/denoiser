#pragma once

#include <iostream>
#include <sstream>
#include <cstddef>

#define LOG_CRITICAL 0
#define LOG_ERROR    1
#define LOG_WARNING  2
#define LOG_INFO     3
#define LOG_DEBUG    4
#define LOG_DEFAULT  LOG_WARNING

extern size_t log_level;

void set_log_level(size_t);

#define logging(file, PREFIX, lvl, ...) do { \
  if(log_level >= lvl) { \
    std::stringstream ss; \
    ss << PREFIX " " << __VA_ARGS__ << std::endl; \
    file << ss.str() << std::flush; \
  } \
} while(false)

#define critical(...) do { logging(std::cerr, "[ABORT]", LOG_CRITICAL, __VA_ARGS__); abort(); } while (false)
#define error(...)   logging(std::cerr, "[ERROR]", LOG_ERROR, __VA_ARGS__)
#define warning(...) logging(std::cerr, "[WARN.]", LOG_WARNING, __VA_ARGS__)
#define info(...)    logging(std::cerr, "[INFO.]", LOG_INFO, __VA_ARGS__)
#define debug(...)   logging(std::cerr, "[DEBUG]", LOG_DEBUG, __VA_ARGS__)

#define enforce(x, ...) do { if (not (x)) critical( "assertion error: " #x ", " << __VA_ARGS__); } while(false)
