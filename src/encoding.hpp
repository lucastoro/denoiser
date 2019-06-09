#pragma once

#include <istream>
#include <queue>

#include "bit.hpp"
#include "logging.hpp"

namespace encoding {

enum result { ok, error, incomplete, end };

class result_t {
public:
  result_t(result res) : res_(res), message_() {}
  result_t(const std::string& message) : res_(error), message_(message) {}
  result_t(const char* message) : res_(error), message_(message) {}
  const std::string& message() const { return message_; }
  result code() const { return res_; }
  operator int() const { return int(res_); }

private:
  result res_;
  std::string message_;
};

class feeder {
public:

  using value_t = int;
  static constexpr value_t eof = EOF;

  virtual value_t get() = 0;
  virtual void push(value_t) = 0;
  virtual void putback(value_t) = 0;
};

class istream_feeder : public feeder {
public:
  istream_feeder(std::istream& is) : is(is) {}
  virtual ~istream_feeder() {}
  virtual int get() override { return is.get(); }
  virtual void push(int) override { log_error << "this method does not exists!"; abort(); }
  virtual void putback(int x) override { is.putback(char(x)); }
private:
  std::istream& is;
  std::queue<value_t> queue;
};

class buffered_feeder : public feeder {
public:
  buffered_feeder() {}
  virtual ~buffered_feeder() {}
  virtual value_t get() override {
    if (queue.empty()) {
      return eof;
    }
    const value_t x = queue.front();
    queue.pop_front();
    return x;
  }
  virtual void push(value_t x) override { queue.push_back(x); }
  virtual void putback(value_t x) override { queue.push_front(x); }
  size_t size() const { return queue.size(); }
  std::deque<value_t>::const_iterator begin() const {return queue.begin(); }
  std::deque<value_t>::const_iterator end() const {return queue.end(); }
private:
  std::deque<value_t> queue;
};

template <typename T>
static result_t ASCII(feeder& ifs, T& out) {
  const auto c = ifs.get();
  if (c > 0x7F) return "invalid ASCII character";
  if (feeder::eof == c) return end;
  out = T(c);
  return ok;
}

template <typename T>
static result_t LATIN1(feeder& ifs, T& out) { // TODO: this is incomplete, need to check if latin1 -> unicode mapping is 1-1
  const auto c = ifs.get();
  if (feeder::eof == c) return end;
  out = T(c);
  return ok;
}

template <typename T>
static result_t UTF8(feeder& ifs, T& out) {

  const auto a = ifs.get();

  if (feeder::eof == a) {
    return end;
  }

  if (bit<7>(a) == 0) {
    out = T(a);
    return ok;
  }

  const auto b = ifs.get();

  if (feeder::eof == b) {
    ifs.putback(a);
    return incomplete;
  }

  if (bit<6,2>(b) != 0b10) {
    return "invalid continuation character";
  }

  if( bit<5,3>(a) == 0b110 ) {
    out = T((bit<0,5>(a) << 6) | bit<0,6>(b));
    return ok;
  }

  const auto c = ifs.get();

  if (feeder::eof == c) {
    ifs.putback(b);
    ifs.putback(a);
    return incomplete;
  }
  if (bit<6,2>(c) != 0b10) {
    return "invalid continuation character";
  }

  if (bit<4,4>(a) == 0b1110) { // 1110aaaa 10bbbbbb 10cccccc
    out = T((bit<0,4>(a) << 12) | (bit<0,6>(b) << 6) | bit<0,6>(c));
    return ok;
  }

  const auto d = ifs.get();

  if (feeder::eof == d) {
    ifs.putback(c);
    ifs.putback(b);
    ifs.putback(a);
    return incomplete;
  }

  if (bit<6,2>(c) != 0b10) {
    return "invalid continuation character";
  }

  if( bit<3,5>(a) == 0b11110 ) { // 11110aaa 10bbbbbb 10cccccc 10dddddd
    out = T((bit<0,4>(a) << 18) | (bit<0,6>(b) << 12) | (bit<0,6>(c) << 6) | bit<0,6>(d));
    return ok;
  }

  return "unexpected character";
}

template <typename T>
using basic_encoder = result_t (*)(feeder& ifs, T& out);
using encoder = basic_encoder<char>;
using wencoder = basic_encoder<wchar_t>;

static constexpr bool icase_comp(int a, int b){
  return (a == b) or (std::isalpha(a) and std::isalpha(b) and std::tolower(a) == std::tolower(b));
}

template <typename CharT, typename String>
static basic_encoder<CharT> get(const String& name) {

  static constexpr std::pair<const char*, basic_encoder<CharT>> encodings[] = {
    {"utf-8", encoding::UTF8<CharT>},
    {"us-ascii", encoding::ASCII<CharT>},
    {"iso-8859-1", encoding::LATIN1<CharT>},
  };

  for (const auto& enco : encodings) {
    const auto b = enco.first;
    const auto e = enco.first + strlen(enco.first);
    if (std::equal(name.begin(), name.end(), e, icase_comp)) {
      return enco.second;
    }
  }

  return nullptr;
}

}
