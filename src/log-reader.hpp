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
#include "logging.hpp"

namespace log {

template <typename CharT>
class basic_line {
public:

  using char_t = CharT;
  using pointer = char_t*;
  using const_pointer = const char_t*;
  using string_view = std::basic_string_view<char_t>;
  using iterator = pointer;
  using const_iterator = const_pointer;

  basic_line() noexcept
    : ptr_(nullptr), size_(0), hash_(0)
  {}

  basic_line(char_t* ptr, const char_t* optr, size_t size) noexcept
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

  bool operator == ( const basic_line<char_t>& other ) const noexcept {
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
class basic_file final {
public:

  using char_t = CharT;
  using string_view = std::basic_string_view<char_t>;
  using line_t = basic_line<char_t>;
  using encoding_t = encoding::basic_encoder<char_t>;

  class exception : std::runtime_error {
  public:
    template <typename ...Args>
    inline exception(Args... args) noexcept
      : std::runtime_error(args...) {}
  };

  class not_found : public exception {
  public:
    template <typename ...Args>
    inline not_found(Args... args) noexcept
      : exception(args...) {}
  };

  class encoding_error : public exception {
  public:
    template <typename ...Args>
    inline encoding_error(Args... args) noexcept
      : exception(args...) {}
  };

  inline basic_file(const char* filename, encoding_t decoder = encoding::UTF8) noexcept(false) {
    std::ifstream stream(filename, std::ios_base::in | std::ios_base::binary);
    if (not stream.is_open()) {
      throw not_found(filename);
    }
    read_stream(stream, decoder);
    build_table();
  }

  basic_file(curlpp::Easy& request, encoding_t decoder = nullptr) noexcept(false) {
      download(request, decoder);
      build_table();
  }

  basic_file(basic_file<char_t>&& other) {
      (*this) = std::move(other);
  }

  const basic_file<char_t>& operator = (basic_file<char_t>&& other) {
      if (this != &other) {
          data = std::move(other.data);
          table = std::move(other.table);
      }
      return *this;
  }

  inline ~basic_file() noexcept
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
    dowloader(curlpp::Easy& r, data_t& data, encoding_t decoder = nullptr)
      : request(r), data(data), decode(decoder) {
      request.setOpt(curlpp::options::NoSignal(true));
      request.setOpt(curlpp::options::Header(false));
      request.setOpt(curlpp::options::NoProgress(false));
      request.setOpt(curlpp::options::Timeout(5)); // sec
      request.setOpt(curlpp::options::FailOnError(true));

      request.setOpt(curlpp::options::HeaderFunction(
      [this](char* data, size_t size, size_t count) -> size_t {
        return this->on_header(
          const_cast<const uint8_t*>(reinterpret_cast<uint8_t*>(data)), size * count
        );
      }));

      request.setOpt(curlpp::options::ProgressFunction(
      [this](double a, double b, double, double) -> int {
        return this->on_progress(a, b);
      }));

      request.setOpt(curlpp::options::WriteFunction(
        [this](char* data, size_t size, size_t count) -> size_t {
          return this->on_data(
            const_cast<const uint8_t*>(reinterpret_cast<uint8_t*>(data)), size * count
          );
        })
      );
    }

    void perform() {
      try {
        request.perform();
      } catch (const curlpp::LibcurlRuntimeError& ex) {
        if (CURLE_COULDNT_RESOLVE_HOST == ex.whatCode()
         or CURLE_HTTP_RETURNED_ERROR == ex.whatCode()) {
          throw not_found(ex.what());
        }
        throw ex;
      }
    }

  private:

    size_t on_data(const uint8_t* ptr, size_t size) {

      if (nullptr == decode) {
        decode = encoding::UTF8; // better safe than sorry
      }

      for (size_t s = 0; s < size; ++s) {
        feeder.push(ptr[s]);
        char_t c;
        const auto result = decode(feeder, c);
        switch (result) {
          case encoding::ok:
            data.push_back(c);
            break;
          case encoding::end:
            break;
          case encoding::error:
            throw encoding_error(result.message());
            break;
          case encoding::incomplete:
            break;
        }

      }
      return size;
    }

    int on_progress(double tot, double) {
      const auto x = size_t(tot);
      if (x > data.capacity()) {
        data.reserve(x);
      }
      return 0;
    }

    size_t on_header(const uint8_t* ptr, size_t size) {
      static const std::regex ctype_rx(R"(^[Cc]ontent-[Tt]ype: (.+))", std::regex::optimize);
      std::cmatch ctype;
      std::string_view header(reinterpret_cast<const char*>(ptr), size);

      if (std::regex_search(header.begin(), header.end(), ctype, ctype_rx) and 2 == ctype.size()) {
        static const std::regex charset_rx(R"(charset=([^ ]+))", std::regex::optimize);
        std::cmatch charset;
        if (std::regex_search(ctype[1].first, ctype[1].second, charset, charset_rx) and 2 == charset.size()) {
          const auto ch = std::string_view(charset[1].first, size_t(charset[1].length()));
          decode = encoding::get<char_t>(ch);
          if (not decode) {
            warning("unknown content type: " << ch);
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

  basic_file(const basic_file<char_t>&) = delete;
  const basic_file<char_t>& operator = (const basic_file<char_t>&) = delete;

  static constexpr bool inline is_endline( char_t c ) noexcept {
    return (c == '\n' or c =='\r');
  }

  inline void download(curlpp::Easy& request, encoding_t decoder = nullptr) {
    dowloader(request, data, decoder).perform();
  }

  class loader {
  public:
    loader(std::istream& stream, data_t& data, encoding_t decoder = encoding::UTF8)
      : feeder(stream), stream(stream), data(data), decode(decoder)
    {}
    void perform() {
      stream.seekg(0, std::ios_base::seekdir::_S_end);
      const size_t size = stream.tellg();
      stream.seekg(0);
      data.clear();
      data.reserve(size);
      bool stop = false;
      while (not stop) {
        char_t c;
        const auto result = decode(feeder, c);
        switch (result) {
          case encoding::ok:
            data.push_back(c);
            break;
          case encoding::end:
            stop = true;
            break;
          case encoding::error:
            throw encoding_error(result.message());
            stop = true;
            break;
          case encoding::incomplete:
            throw encoding_error("stram closed");
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

  inline void read_stream(std::istream& stream, encoding_t encoder = encoding::UTF8) {
    loader(stream, data, encoder).perform();
  }

  inline void build_table() {

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
};

using line = basic_line<char>;
using file = basic_file<char>;
using wline = basic_line<wchar_t>;
using wfile = basic_file<wchar_t>;

}

static_assert(not std::is_copy_constructible<log::basic_file<>>::value, "");
static_assert(not std::is_copy_assignable<log::basic_file<>>::value, "");
