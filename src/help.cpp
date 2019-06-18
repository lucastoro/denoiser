#include "help.hpp"

#include <iostream>
#include <cstring>

#define nl "\n"

static constexpr const char* OPTIONS =
nl "OPTIONS:"
nl "  -c, --config    read the configuration from the given filename instead from stdin"
nl "  -d, --directory change the working directory to the given path"
nl "  -n, --no-lines  do not output line numbers in the output"
nl "  -j, --jobs      use the given number of threads, defaults to the number of hw threads"
nl "  -v, --verbose   print information regarding the process to stderr"
nl "  -p, --profile   print profiling information to stderr"
nl "  -g, --debug     print even more information to stderr"
#if WITH_TESTS
nl "  -t, --test      executes the unit tests"
nl "  --gtest_*       in combination of --test will be forwarded to the gtest test suite"
#endif
;

#undef nl

static const char* my_name(const char* self) {
  const auto ptr = strrchr(self, '/');
  return ptr ? ptr + 1 : self;
}

void print_help(const char* self, std::ostream& os) {
  os << "Usage: " << my_name(self) << " [OPTIONS]" << OPTIONS <<  std::endl;
}
