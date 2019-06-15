#include "arguments.hpp"

#include <cstring>
#include <string_view>
#include <cstdlib>

arguments::arguments(int argc, char** argv)
  : argc(argc), argv(argv) {
}

bool arguments::have_flag(const char* name) const {
  for (int i = 1; i < argc; ++i) {
    if (0 == strcmp(name, argv[i])) {
      return true;
    }
  }
  return false;
}

std::string_view arguments::value(const char* name) const {
  for (int i = 1; i < argc; ++i) {
    if (0 == strcmp(name, argv[i])) {
      if (i != argc - 1) {
        return argv[i + 1];
      }
    }
  }
  return {};
}

std::string_view arguments::back() const {
  return argv[argc - 1];
}

std::string_view arguments::front() const {
  return argv[1];
}

arguments::const_iterator::const_iterator(char** argv) : argv(argv) {
}

void arguments::const_iterator::operator ++ () {
  ++argv;
}

bool arguments::const_iterator::operator == (const const_iterator& other) const {
  return argv == other.argv;
}

std::string_view arguments::const_iterator::operator * () const {
  return *argv;
}

arguments::const_iterator arguments::begin() const {
  return argv + 1;
}

arguments::const_iterator arguments::end() const {
  return argv + argc;
}

int arguments::to_int(const std::string_view& sw) {
  if (sw.empty()) return int();
  return atoi(sw.data());
}

double arguments::to_double(const std::string_view& sw) {
  if (sw.empty()) return double();
  return std::strtod(sw.data(), nullptr);
}
