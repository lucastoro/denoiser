#pragma once

#include <cstring>
#include <string_view>
#include <cstdlib>

class arguments {
public:

  arguments(int argc, char** argv)
    : argc(argc), argv(argv) {}

  bool have_flag(const char* name) const {
    for (int i = 1; i < argc; ++i) {
      if (0 == strcmp(name, argv[i])) {
        return true;
      }
    }
    return false;
  }

  template <typename ...Args>
  bool have_flag(const char* first, Args... more) const {
    return have_flag(first) or have_flag(more...);
  }

  std::string_view value(const char* name) const {
    for (int i = 1; i < argc; ++i) {
      if (0 == strcmp(name, argv[i])) {
        if (i != argc - 1) {
          return argv[i + 1];
        }
      }
    }
    return {};
  }

  template <typename ...Args>
  std::string_view value(const char* first, Args... more) const {
    const auto opt = value(first);
    return opt.size() ? opt : value(more...);
  }

  template <typename Ret, typename ...Args>
  Ret value(const char* first, Args... more) const {
    const std::string_view sw = value(first, more...);
    return std::is_integral<Ret>::value ? Ret(to_int(sw)) : Ret(to_double(sw));
  }

  std::string_view back() const {
    return argv[argc - 1];
  }

  std::string_view front() const {
    return argv[1];
  }

  class const_iterator {
  public:
      const_iterator(char** argv) : argv(argv) {}
      void operator ++ () { ++argv; }
      bool operator == (const const_iterator& other) const { return argv == other.argv; }
      std::string_view operator * () const { return *argv; }
  private:
    char** argv;
  };

  const_iterator begin() const { return argv + 1; }
  const_iterator end() const { return argv + argc; }

private:

  static int to_int(const std::string_view& sw) {
    if (sw.empty()) return int();
    return atoi(sw.data());
  }

  static double to_double(const std::string_view& sw) {
    if (sw.empty()) return double();
    return std::strtod(sw.data(), nullptr);
  }

  int argc;
  char** argv;
};
