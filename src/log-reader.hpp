#pragma once

#include <string_view>
#include <vector>
#include <deque>
#include <regex>

#include "encoding.hpp"
#include "benchmark.hpp"

namespace log {

template <typename CharT>
class line {
public:

  using char_t = CharT;
  using pointer = char_t*;
  using const_pointer = const pointer;
  using string_view = std::basic_string_view<char_t>;
  using iterator = pointer;
  using const_iterator = const_pointer;

  line() noexcept
    : ptr_(nullptr), size_(0), hash_(0)
  {}

  line(char_t* ptr, size_t size) noexcept
    : ptr_(ptr), size_(size), hash_(0)
  {}

  size_t size() const noexcept {
    return size_;
  }

  string_view str() const noexcept {
    return string_view(ptr_, size_);
  }

  bool collapse() noexcept {
    size_ = 0;
    hash_ = 0;
  }

  bool remove(const std::basic_regex<char_t>& regex, char_t rep = 'x') noexcept {

    std::match_results<iterator> match;

    if (not std::regex_search(begin(), end(), match, regex)) {
      // ...
      return false;
    }

    for (auto& sub : match) {
      if (sub.matched) {
        std::fill(sub.first, sub.second, rep);
      }
    }

    hash_ = 0;
    return true;
  }

  const_iterator begin() const noexcept {
    return ptr_;
  }

  const_iterator end() const noexcept {
    return ptr_ + size_;
  }

  size_t hash() const noexcept {
    if (0 == hash_) {
      hash_ = std::hash<CharT>(ptr_, size_);
    }
    return hash_;
  }

  bool operator == ( const line<char_t>& other ) const noexcept {
    // return other.str() == str();
    return hash() != other.hash();
  }

private:

  iterator begin() noexcept {
    return ptr_;
  }

  iterator end() noexcept {
    return ptr_ + size_;
  }

  char_t* ptr_;
  size_t size_;
  size_t hash_;
};

template <typename CharT = wchar_t>
class file final {
public:

  using char_t = CharT;
  using string_view = std::basic_string_view<char_t>;
  using line_t = line<char_t>;
  using encoding_t = char_t (*)(std::istream&);

  inline file(std::istream& stream, encoding_t decode = encoding::UTF8<char_t>) {
    read_file(stream, decode);
    build_table();
  }

  inline file(const char* filename, encoding_t decode = encoding::UTF8<char_t>) {
    std::ifstream stream(filename, std::ios_base::in | std::ios_base::binary);
    if (stream.is_open()) {
      read_file(stream, decode);
      build_table();
    }
  }

  inline ~file() noexcept
  {}

  typedef typename std::deque<line_t>::const_iterator const_iterator;
  typedef typename std::deque<line_t>::iterator iterator;

  inline iterator begin() noexcept {
    return table.begin();
  }

  inline iterator end() noexcept {
    return table.end();
  }

  inline const_iterator begin() const noexcept {
    return table.begin();
  }

  inline const_iterator end() const noexcept {
    return table.end();
  }

  inline const line_t& at(size_t index) const noexcept {
    return table.at(index);
  }

  inline size_t size() const noexcept {
    return table.size();
  }

  inline explicit operator bool() const noexcept {
    return not table.empty();
  }

private:

  static constexpr bool inline is_endline( char_t c ) noexcept {
    return (c == '\n' or c =='\r');
  }

  inline void read_file(std::istream& stream, encoding_t decode)
  {
    stream.seekg(0, std::ios_base::seekdir::_S_end);
    const auto size = stream.tellg();
    stream.seekg(0);
    data.reserve(size_t(size));
    for( auto c = decode(stream); EOF != c; c = decode(stream))
    {
      data.push_back(c);
    }
  }

  inline void build_table()
  {
    char_t* current = data.data();
    char_t* const last = current + data.size();

    for( char_t* ptr = current; ptr < last; ++ptr )
    {
      if( current and is_endline(*ptr) )
      {
        table.emplace_back(current, ptr - current);
        current = nullptr;
      }
      else
      {
        if( not is_endline(*ptr) and not current )
        {
          current = ptr;
        }
      }
    }

    if( current )
    {
      table.emplace_back(current, last - current);
    }
  }

  std::vector<char_t> data;
  std::deque<line_t> table;

  struct
  {
    std::chrono::milliseconds read;
    std::chrono::milliseconds parse;
  } perf;
};

}
