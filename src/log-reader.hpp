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
#include "logging.hpp"

#include "log-fetcher.hpp"

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

template <typename CharT>
class basic_file;

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
    : ptr_(nullptr), size_(0), number_(0), hash_(0)
  {}

  basic_line(const basic_file<CharT>* fil, size_t num, char_t* ptr, const char_t* optr, size_t size) noexcept
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

  void remove(const basic_pattern<char_t>& pattern) {
    pattern.is_regex()
      ? remove(pattern.regex())
      : remove(pattern.string());
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

  const basic_file<CharT>& source() const {
      return *file_;
  }

  const string_view mut() const noexcept {
    return ptr_ ? string_view(ptr_, size_) : string_view();
  }

private:

  void trim() {

    while (size_ && std::isspace(*ptr_)) {
      ++ptr_;
    }
    while (size_ && std::isspace(ptr_[size_ - 1])) {
      --size_;
    }
  }

  void remove(const std::basic_string<char_t>& string) noexcept {

    const auto sz = string.size();
    auto it = std::search(begin(), end(), string.begin(), string.end());

    while (it != end()) {
      std::copy(it + sz, end(), it); // this MUST be a forward copy
      size_ -= sz;
      it = std::search(it, end(), string.begin(), string.end());
    }
  }

  class overwriter : public std::iterator<std::output_iterator_tag, char_t> {
  public:
    inline explicit overwriter(char_t* p) : ptr(p), base(&ptr) {}
    inline overwriter(const overwriter& o) : ptr(o.ptr), base(o.base) {
    }
    inline char_t* operator ++ (int i) {
      (*base) += i;
      return (*base);
    }
    inline void operator ++ () {
      (*base)++;
    }
    inline char_t& operator * () {
      return *(*base);
    }
    inline const char_t* get() const {
      return *base;
    }
  private:
    char_t* ptr;
    char_t** base;
  };

  void remove(const std::basic_regex<char_t>& regex) noexcept {

    if (0 == size_) {
      return;
    }

    char_t null[] = {0};
    overwriter it(ptr_);

    std::regex_replace(it, begin(), end(), regex, null);

    size_ = it.get() - ptr_;

    trim();

    hash_ = 0;
  }

  void suppress(const std::basic_string<char_t>& pattern) noexcept {

    if (0 == size_) {
      return;
    }

    if (std::basic_string<char_t>::npos != str().find(pattern)) {
      size_ = 0;
      hash_ = 0;
    }
  }

  void suppress(const std::basic_regex<char_t>& pattern) noexcept {

    if (0 == size_) {
      return;
    }

    if (std::regex_search(begin(), end(), pattern)) {
      size_ = 0;
      hash_ = 0;
    }
  }

  basic_line(const basic_line&) = delete;
  basic_line& operator = (const basic_line&) = delete;

  const_iterator begin() const noexcept {
    return ptr_;
  }

  const_iterator end() const noexcept {
    return ptr_ + size_;
  }

  iterator begin() noexcept {
    return ptr_;
  }

  iterator end() noexcept {
    return ptr_ + size_;
  }

  char_t* ptr_;
  const char_t* original_ptr_;
  size_t size_;
  size_t original_size_;
  const basic_file<CharT>* file_;
  size_t number_;
  mutable size_t hash_;
};

static_assert(not std::is_copy_constructible<log::basic_line<char>>::value, "");
static_assert(not std::is_copy_assignable<log::basic_line<char>>::value, "");

template <typename CharT>
class basic_file final : data_consumer<CharT> {
public:

  using char_t = CharT;
  using line_t = basic_line<char_t>;
  using encoding_t = encoding::basic_encoder<CharT>;
  using string_view = std::basic_string_view<char_t>;

  basic_file() {
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
  enum source_t {local, http};
public:

  static basic_file<char_t> download(const std::string& url, const std::string& alias = {}) {
    return basic_file<char_t>(http, url, alias);
  }

  static basic_file<char_t> load(const std::string& url, const std::string& alias = {}) {
    return basic_file<char_t>(local, url, alias);
  }

private:

  template <typename char_t>
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
  };

  // data_consumer
  virtual void size_hint(size_t size) override {
    if (data.capacity() < size) {
      data.reserve(size);
    }
  }

  // data_consumer
  virtual void on_data(char_t* ptr, size_t count) override {
    for (size_t i = 0; i < count; ++i) {
      data.push_back(ptr[i]);
    }
  }

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
        if (not stream.is_open()) {
          throw std::runtime_error("file not found" + resource);
        }
        read_stream(stream);
        build_table();
        break;
      }
      case http: {
        curl(resource);
        build_table();
        break;
      }
    }
  }

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

  basic_file(const basic_file<char_t>&) = delete;
  const basic_file<char_t>& operator = (const basic_file<char_t>&) = delete;

  static constexpr bool inline is_endline( char_t c ) noexcept {
    return (c == '\n' or c =='\r');
  }

  inline void curl(const std::string url) {
    downloader<char_t>(url, *this).perform();
  }

  inline void read_stream(std::istream& stream) {
    loader<char_t>(stream, *this).perform();
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
  data_t<char_t> data;
  std::deque<line_t> table;
};

using pattern = basic_pattern<char>;
using wpattern = basic_pattern<wchar_t>;

using line = basic_line<char>;
using wline = basic_line<wchar_t>;

using file = basic_file<char>;
using wfile = basic_file<wchar_t>;

}

static_assert(not std::is_copy_constructible<log::basic_file<char>>::value);
static_assert(not std::is_copy_assignable<log::basic_file<char>>::value);
