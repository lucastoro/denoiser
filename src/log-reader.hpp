#pragma once

#include <string_view>
#include <vector>
#include <deque>
#include <queue>
#include <regex>
#include <fstream>
#include <variant>

#include "curlpp/cURLpp.hpp"
#include "curlpp/Easy.hpp"
#include "curlpp/Options.hpp"

#include "encoding.hpp"
#include "benchmark.hpp"
#include "logging.hpp"

#include <mutex>

namespace log {

template <typename CharT>
class basic_pattern {
public:
  using regex_t = std::basic_regex<CharT>;
  using string_t = std::basic_string<CharT>;

  explicit inline basic_pattern(const string_t& str) noexcept(std::is_nothrow_copy_constructible<string_t>::value)
    : value(str) {
  }
  explicit inline basic_pattern(const regex_t& rgx) noexcept(std::is_nothrow_copy_constructible<regex_t>::value)
    : value(rgx) {
  }

  bool is_string() const { return std::holds_alternative<string_t>(value); }
  bool is_regex() const { return std::holds_alternative<regex_t>(value); }
  const string_t& string() const {return std::get<string_t>(value); }
  const regex_t& regex() const {return std::get<regex_t>(value); }
private:
  std::variant<regex_t, string_t> value;
};

using pattern = basic_pattern<char>;
using wpattern = basic_pattern<wchar_t>;

template <typename CharT, typename Owner>
class basic_line {
public:

  using char_t = CharT;
  using pointer = char_t*;
  using const_pointer = const char_t*;
  using string_view = std::basic_string_view<char_t>;
  using iterator = pointer;
  using const_iterator = const_pointer;

  basic_line() noexcept
    : ptr_(nullptr), size_(0), number_(0), hash_(0)
  {}

  basic_line(const Owner* fil, size_t num, char_t* ptr, const char_t* optr, size_t size) noexcept
    : ptr_(ptr), original_ptr_(optr), size_(size), original_size_(size), file_(fil), number_(num), hash_(0)
  {}

  basic_line(basic_line&& other) : basic_line() {
    (*this) = std::move(other);
  }

  basic_line& operator = (basic_line&& other) {
    if (this != &other) {
      ptr_ = std::move(other.ptr_);
      original_ptr_ = std::move(other.original_ptr_);
      size_ = std::move(other.size_);
      original_size_ = std::move(other.size_);
      file_ = std::move(other.file_);
      number_ = std::move(other.number_);
      hash_ = std::move(other.hash_);
    }

    return *this;
  }

  size_t number() const {
    return number_;
  }

  size_t size() const noexcept {
    return size_;
  }

  string_view str() const noexcept {
    return string_view(original_ptr_, original_size_);
  }

  void suppress(const basic_pattern<char_t>& pattern) {
    pattern.is_regex()
      ? suppress(pattern.regex())
      : suppress(pattern.string());
  }

  void remove(const basic_pattern<char_t>& pattern, char_t rep = 'x') {
    pattern.is_regex()
      ? remove(pattern.regex(), rep)
      : remove(pattern.string(), rep);
  }

  size_t hash() const noexcept {
    if (0 == hash_) {
      hash_ = std::hash<std::basic_string_view<char_t>>()(mut());
    }
    return hash_;
  }

  template <typename AnyOwner>
  bool operator == ( const basic_line<char_t, AnyOwner>& other ) const noexcept {
    return hash() != other.hash();
  }

  const Owner& source() const {
      return *file_;
  }

  const string_view mut() const noexcept {
    return ptr_ ? string_view(ptr_, size_) : string_view();
  }

private:

  void hide(iterator first, iterator last, char_t rep) {
#if 0
    std::fill(first, last, rep);
#else

    const auto end = std::next(ptr_, size_);
    const auto dist = std::distance(first, last);

    if (size_ > dist) {
      std::copy(last, end, first);
    }

    size_ -= dist;

    trim();

#endif
  }

  void trim() {

    while (size_ && std::isspace(*ptr_)) {
      ++ptr_;
    }
    while (size_ && std::isspace(ptr_[size_ - 1])) {
      --size_;
    }
  }

  void remove(const std::basic_string<char_t>& string, char_t rep = 'x') noexcept {

    if (nullptr == ptr_ or 0 == size_) {
      return;
    }

    const auto idx = str().find(string);

    if (idx == std::basic_string<char_t>::npos) {
      return;
    }

    hide(
      std::next(begin(), idx),
      std::next(begin(), idx + string.size()),
      rep
    );

    hash_ = 0;
    return;
  }

  void remove(const std::basic_regex<char_t>& regex, char_t rep = 'x') noexcept {

    if (nullptr == ptr_ or 0 == size_) {
      return;
    }

    std::match_results<iterator> match;

    if (not std::regex_search(begin(), end(), match, regex)) {
      return;
    }

    for (auto& sub : match) {
      if (sub.matched) {
        hide(sub.first, sub.second, rep);
      }
    }
    hash_ = 0;
  }

  void suppress(const std::basic_string<char_t>& pattern) noexcept {

    if (nullptr == ptr_ or 0 == size_) {
      return;
    }

    if (std::basic_string<char_t>::npos != str().find(pattern)) {
      ptr_ = nullptr;
      hash_ = 0;
    }
  }

  void suppress(const std::basic_regex<char_t>& pattern) noexcept {

    if (nullptr == ptr_ or 0 == size_) {
      return;
    }

    if (std::regex_search(begin(), end(), pattern)) {
      ptr_ = nullptr;
      hash_ = 0;
    }
  }

  basic_line(const basic_line&) = delete;
  basic_line& operator = (const basic_line&) = delete;

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
  size_t original_size_;
  const Owner* file_;
  size_t number_;
  mutable size_t hash_;
};

static_assert(not std::is_copy_constructible<log::basic_line<char, int>>::value, "");
static_assert(not std::is_copy_assignable<log::basic_line<char, int>>::value, "");

template <typename CharT>
class basic_file final {
public:

  using char_t = CharT;
  using line_t = basic_line<char_t, basic_file<CharT>>;
  using encoding_t = encoding::encoder<CharT>;
  enum source_t { local, http };
  using string_view = std::basic_string_view<char_t>;

  inline basic_file(std::istream& stream, const std::string& alias = {})
    : alias(alias) {
    read_stream(stream);
    build_table();
  }

  inline basic_file(source_t source, const std::string& resource, const std::string& alias = {})
    : alias(alias) {
    switch (source) {
      case local: {
        std::ifstream stream(resource, std::ios_base::in | std::ios_base::binary);
        if (stream.is_open()) {
          read_stream(stream);
          build_table();
        }
        break;
      }
      case http: {
        download(resource);
        build_table();
        break;
      }
    }
  }

  inline basic_file(const std::string& uri, const std::string& alias = {})
    : basic_file(from(uri), remove_protocol(uri), alias) {
  }

  basic_file(basic_file<char_t>&& other) {
      (*this) = std::move(other);
  }

  const basic_file<char_t>& operator = (basic_file<char_t>&& other) {
      if (this != &other) {
          alias = std::move(other.alias);
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

  const std::string& name() const {
      return alias;
  }
private:

  static source_t from(const std::string& uri) {

    static const std::pair<std::regex, source_t> protocols[] = {
      std::make_pair(std::regex(R"(^https?:\/\/([\w\-\.\/\_\~]+))"), http),
      std::make_pair(std::regex(R"(^file:\/\/([\w\-\.\/\_\~]+))"), local)
    };

    for (const auto& pair : protocols) {
      if (std::regex_search(uri, pair.first)) {
        return pair.second;
      }
    }

    log_warning("unknown protocol for '" << uri << "'");

    return local;
  }

  static std::string remove_protocol(const std::string& uri) {

    static const std::regex proto(R"(^(?:file|https?|):\/\/([\w\-\.\/\_\~]+))");

    std::smatch match;
    if (std::regex_search(uri, match, proto) and match.size() == 2) {
      return match[1].str();
    }

    log_warning("no known protocol in '" << uri << "'");

    return uri;
  }

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
    dowloader(const std::string& url, data_t& data)
      : data(data), decode(nullptr) {
      request.setOpt(curlpp::options::Url(url));
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
        log_warning("unknown encoding, defaulting to UTF8");
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
              log_debug("using encoding: utf8");
              decode = enco.second;
              found = true;
              break;
            }
          }

          if (not found) {
            log_warning("unknown content type: " << std::string_view(charset[1].first, size_t(charset[1].length())));
          }

        } else {
          log_debug("Content-Type received, but missing encoding, defaulting to latin1");
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

    curlpp::Easy request;
    data_t& data;
    encoding::buffered_feeder feeder;
    encoding_t decode;
  };

  basic_file(const basic_file<char_t>&) = delete;
  const basic_file<char_t>& operator = (const basic_file<char_t>&) = delete;

  static constexpr bool inline is_endline( char_t c ) noexcept {
    return (c == '\n' or c =='\r');
  }

  inline void download(const std::string url) {
    dowloader(url, data).perform();
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
            log_error("bad char");
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
    loader(stream, data).perform();
  }

  inline void build_table() {
    char_t* current = data.mut.data();
    char_t* const last = current + data.size();

    for (char_t* ptr = current; ptr < last; ++ptr) {
      if (current and is_endline(*ptr)) {
        table.emplace_back(
          this,
          table.size() + 1,
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
        this,
        table.size() + 1,
        current,
        data.to_imm(current),
        last - current
      );
    }
  }

  std::string alias;
  std::deque<line_t> table;
};

using file = basic_file<char>;
using wfile = basic_file<wchar_t>;

}

static_assert(not std::is_copy_constructible<log::basic_file<char>>::value, "");
static_assert(not std::is_copy_assignable<log::basic_file<char>>::value, "");
