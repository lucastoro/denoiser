#pragma once

#include <cstring>
#include <string_view>
#include <cstdlib>

class arguments {
public:

  arguments(int argc, char** argv);

  bool have_flag(const char* name) const;

  template <typename ...Args>
  bool have_flag(const char* first, Args... more) const {
    return have_flag(first) or have_flag(more...);
  }

  std::string_view value(const char* name) const;

  template <typename ...Args>
  std::string_view value(const char* first, Args... more) const {
    const auto opt = value(first);
    return opt.empty() ? value(more...) : opt;
  }

  template <typename Ret, typename ...Args>
  Ret value(const char* first, Args... more) const {
    const std::string_view sw = value(first, more...);
    return std::is_integral<Ret>::value ? Ret(to_int(sw)) : Ret(to_double(sw));
  }

  std::string_view back() const;
  std::string_view front() const;

  class const_iterator {
  public:
      const_iterator(char** argv);
      void operator ++ ();
      bool operator == (const const_iterator& other) const;
      std::string_view operator * () const;
  private:
    char** argv;
  };

  const_iterator begin() const;
  const_iterator end() const;

private:

  static int to_int(const std::string_view& sw);
  static double to_double(const std::string_view& sw);

  int argc;
  char** argv;
};
