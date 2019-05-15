#pragma once

#include <cstring>
#include <string_view>

class arguments {
public:
  arguments(int argc, char** argv)
    : argc(argc), argv(argv) {}
  bool has_flag(const char* name) const {
    for (int i = 1; i < argc; ++i) {
      if (0 == strcmp(name, argv[i])) {
        return true;
      }
    }
    return false;
  }
  template <typename ...Args>
  bool has_flag(const char* first, Args... more) const {
    return has_flag(first) or has_flag(more...);
  }

  template <typename ...Args>
  bool operator () (const Args... args) const {
    return has_flag(args...);
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
    return opt.size() ? opt : get(more...);
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
  int argc;
  char** argv;
};
