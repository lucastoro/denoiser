#include "logging.hpp"

size_t log_level = LOG_DEFAULT;

void set_log_level(size_t l) {
    log_level = l;
}
