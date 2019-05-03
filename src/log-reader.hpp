#pragma once

#include <string_view>
#include <vector>
#include <deque>
#include <queue>
#include <regex>
#include <fstream>

#include "curlpp/cURLpp.hpp"
#include "curlpp/Easy.hpp"
#include "curlpp/Options.hpp"

#include "encoding.hpp"
#include "benchmark.hpp"
#include "logging.hpp"

namespace log {

template <typename CharT>
class line {
public:

  using char_t = CharT;
  using pointer = char_t*;
  using const_pointer = const char_t*;
  using string_view = std::basic_string_view<char_t>;
  using iterator = pointer;
  using const_iterator = const_pointer;

  line() noexcept
    : ptr_(nullptr), size_(0), hash_(0)
  {}

  line(char_t* ptr, const char_t* optr, size_t size) noexcept
    : ptr_(ptr), original_ptr_(optr), size_(size), hash_(0)
  {}

  size_t size() const noexcept {
    return size_;
  }

  string_view str() const noexcept {
    return string_view(original_ptr_, size_);
  }

  void suppress() noexcept {
    ptr_ = nullptr;
    hash_ = 0;
  }

  bool contains(const std::basic_regex<char_t>& regex) const noexcept {
    return std::regex_search(begin(), end(), regex);
  }

  bool remove(const std::basic_regex<char_t>& regex, char_t rep = 'x') noexcept {

    std::match_results<iterator> match;

    if (not std::regex_search(begin(), end(), match, regex)) {
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


  size_t hash() const noexcept {
    if (0 == hash_) {
      hash_ = std::hash<std::basic_string_view<char_t>>()(mut());
    }
    return hash_;
  }

  bool operator == ( const line<char_t>& other ) const noexcept {
    // return other.str() == str();
    return hash() != other.hash();
  }

private:

  const string_view mut() const noexcept {
    return ptr_ ? string_view(ptr_, size_) : string_view();
  }

  const_iterator begin() const noexcept {
    return ptr_ ? ptr_ : reinterpret_cast<const_pointer>(this);
  }

  const_iterator end() const noexcept {
    return ptr_ ? (ptr_ + size_) : reinterpret_cast<const_pointer>(this);
  }

  iterator begin() noexcept {
    return ptr_ ? ptr_ : reinterpret_cast<pointer>(this);
  }

  iterator end() noexcept {
    return ptr_ ? (ptr_ + size_) : reinterpret_cast<pointer>(this);
  }

  char_t* ptr_;
  const char_t* original_ptr_;
  size_t size_;
  mutable size_t hash_;
};

template <typename CharT = wchar_t>
class file final {
public:

  using char_t = CharT;
  using string_view = std::basic_string_view<char_t>;
  using line_t = line<char_t>;
  using encoding_t = encoding::encoder<CharT>;

  inline file(std::istream& stream) {
    read_stream(stream);
    build_table();
  }

  inline file(const char* filename) {
    std::ifstream stream(filename, std::ios_base::in | std::ios_base::binary);
    if (stream.is_open()) {
      read_stream(stream);
      build_table();
    }
  }

  file(curlpp::Easy& request) {
      download(request);
      build_table();
  }

  file(file<char_t>&& other) {
      (*this) = std::move(other);
  }

  const file<char_t>& operator = (file<char_t>&& other) {
      if (this != &other) {
          data = std::move(other.data);
          table = std::move(other.table);
          perf = std::move(other.perf);
      }
      return *this;
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

  void profile() const {
    const auto MB = data.size() / 1048576.0;
    const auto sec = perf.read.count() / 1000.0;
    info("read : " << perf.read.count() << " ms (" << MB/sec << " MB/sec)");
    info("parse: " << perf.parse.count() << " ms");
  }

private:

  struct data_t {
    std::vector<char_t> mut, imm;
    inline size_t size() const {
      enforce(mut.size() == imm.size(), "male male");
      return mut.size();
    }
    inline size_t capacity() const {
      enforce(mut.capacity() == imm.capacity(), "male male");
      return mut.capacity();
    }
    inline void reserve(size_t sz) {
      mut.reserve(sz);
      imm.reserve(sz);
    }
    inline void push_back(char_t c) {
      mut.push_back(c);
      imm.push_back(c);
    }
    inline void clear() {
      mut.clear();
      imm.clear();
    }
    inline const char_t* to_imm(const char_t* mptr) const {
      return std::next(imm.data(), std::distance(mut.data(), mptr));
    }
  } data;

  class dowloader {
  public:
    dowloader(curlpp::Easy& r, data_t& data)
      : request(r), data(data), decode(nullptr) {
      request.setOpt(curlpp::options::NoSignal(true));
      request.setOpt(curlpp::options::Header(false));
      request.setOpt(curlpp::options::NoProgress(false));

      request.setOpt(curlpp::options::HeaderFunction(
      [this](char* data, size_t size, size_t count) -> size_t {
        return this->onHeader(data, size * count);
      }));

      request.setOpt(curlpp::options::ProgressFunction(
      [this](double a, double b, double, double) -> int {
        return this->onProgress(a, b);
      }));

      request.setOpt(curlpp::options::WriteFunction(
        [this](char* ptr, size_t size, size_t count) -> size_t {
          return this->onData(ptr, size * count);
        })
      );
    }

    void perform() {
      request.perform();
    }

  private:

    size_t onData(char* ptr, size_t size) {

      if (not decode) {
        warning("unknown encoding, defaulting to UTF8");
        decode = encoding::UTF8;
      }

      for (size_t s = 0; s < size; ++s) {
        feeder.push(ptr[s]);
        CharT c;
        switch(decode(feeder, c)) {
          case encoding::ok:
            data.push_back(c);
            break;
          case encoding::end:
            break;
          case encoding::error:
            enforce(false, "bad");
            break;
          case encoding::incomplete:
            break;
        }

      }
      return size;
    }

    int onProgress(double tot, double) {
      const auto x = size_t(tot);
      if (x > data.capacity()) {
        data.reserve(x);
      }
      return 0;
    }

    static constexpr bool icase(int a, int b){
      return (a == b) or (std::isalpha(a) and std::isalpha(b) and std::tolower(a) == std::tolower(b));
    }

    size_t onHeader(char* ptr, size_t size) {
      static const std::regex ctype_rx(R"(^[Cc]ontent-[Tt]ype: (.+))", std::regex::optimize);
      std::cmatch ctype;
      std::string_view header(ptr, size);

      if (std::regex_search(header.begin(), header.end(), ctype, ctype_rx) and 2 == ctype.size()) {
        static const std::regex charset_rx(R"(charset=([^ ]+))", std::regex::optimize);
        std::cmatch charset;
        if (std::regex_search(ctype[1].first, ctype[1].second, charset, charset_rx) and 2 == charset.size()) {

          bool found = false;

          for (const auto& enco : encodings) {
            const auto b = enco.first;
            const auto e = enco.first + strlen(enco.first);
            if (std::equal(charset[1].first, charset[1].second, b, e, icase)) {
              debug("using encoding: utf8");
              decode = enco.second;
              found = true;
              break;
            }
          }

          if (not found) {
            warning("unknown content type: " << std::string_view(charset[1].first, size_t(charset[1].length())));
          }

        } else {
          debug("Content-Type received, but missing encoding, defaulting to latin1");
          decode = encoding::LATIN1;
        }
      }
      return size;
    }

    static constexpr std::pair<const char*, encoding_t> encodings[] = {
      {"utf-8", encoding::UTF8},
      {"us-ascii", encoding::ASCII},
      {"iso-8859-1", encoding::LATIN1},
    };

    curlpp::Easy& request;
    data_t& data;
    encoding::buffered_feeder feeder;
    encoding_t decode;
  };

  file(const file<char_t>&) = delete;
  const file<char_t>& operator = (const file<char_t>&) = delete;

  static constexpr bool inline is_endline( char_t c ) noexcept {
    return (c == '\n' or c =='\r');
  }

  inline void download(curlpp::Easy& request) {
    Benchmark bench(perf.read);
    dowloader(request, data).perform();
  }

  class loader {
  public:
    loader(std::istream& stream, data_t& data) : feeder(stream), stream(stream), data(data), decode(encoding::UTF8) {}
    void perform() {
      stream.seekg(0, std::ios_base::seekdir::_S_end);
      const auto size = stream.tellg();
      stream.seekg(0);
      data.clear();
      data.reserve(size_t(size));
      bool stop = false;
      while (not stop) {
        CharT c;
        switch(decode(feeder, c)) {
          case encoding::ok:
            data.push_back(c);
            break;
          case encoding::end:
            stop = true;
            break;
          case encoding::error:
          case encoding::incomplete:
            error("bad char");
            stop = true;
            break;
        }
      }
    }
  private:
    encoding::istream_feeder feeder;
    std::istream& stream;
    data_t& data;
    encoding_t decode;
  };

  inline void read_stream(std::istream& stream) {
    Benchmark bench(perf.read);
    loader(stream, data).perform();
  }

  inline void build_table() {

    Benchmark bench(perf.parse);
    char_t* current = data.mut.data();
    char_t* const last = current + data.size();

    for (char_t* ptr = current; ptr < last; ++ptr) {
      if (current and is_endline(*ptr)) {
        table.emplace_back(
          current,
          data.to_imm(current),
          ptr - current
        );
        current = nullptr;
      }
      else {
        if (not is_endline(*ptr) and not current) {
          current = ptr;
        }
      }
    }

    if (current) {
      table.emplace_back(
        current,
        data.to_imm(current),
        last - current
      );
    }
  }

  std::deque<line_t> table;

  struct {
    std::chrono::milliseconds read;
    std::chrono::milliseconds parse;
  } perf;
};

}

static_assert(not std::is_copy_constructible<log::file<>>::value, "");
static_assert(not std::is_copy_assignable<log::file<>>::value, "");
