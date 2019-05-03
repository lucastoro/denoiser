#pragma once

#include <istream>
#include <queue>

#include "bit.hpp"
#include "logging.hpp"

namespace encoding {

enum result { ok, error, incomplete, end };

class feeder {
public:
  virtual int get() = 0;
  virtual void push(int) = 0;
  virtual void putback(int) = 0;
};

class istream_feeder : public feeder {
public:
  istream_feeder(std::istream& is) : is(is) {}
  virtual ~istream_feeder() {}
  virtual int get() override { return is.get(); }
  virtual void push(int) override { critical("this method does not exists!"); }
  virtual void putback(int x) override { is.putback(char(x)); }
private:
  std::istream& is;
};

class buffered_feeder : public feeder {
public:
  buffered_feeder() {}
  virtual ~buffered_feeder() {}
  virtual int get() override {
    if (queue.empty()) {
      return EOF;
    }
    const int x = queue.front();
    queue.pop_front();
    return x;
  }
  virtual void push(int x) override { queue.push_back(x); }
  virtual void putback(int x) override { queue.push_front(x); }
  size_t size() const { return queue.size(); }
  std::deque<int>::const_iterator begin() const {return queue.begin(); }
  std::deque<int>::const_iterator end() const {return queue.end(); }
private:
  std::deque<int> queue;
};

template <typename T>
using encoder = result (*)(feeder& ifs, T& out);

template <typename T>
static result ASCII(feeder& ifs, T& out) {
  const auto c = ifs.get();
  if (c > 0x7F) return error;
  if (EOF == c) return end;
  out = T(c);
  return ok;
}

template <typename T>
static result LATIN1(feeder& ifs, T& out) { // TODO: this is incomplete, need to check if latin1 -> unicode mapping is 1-1
  const auto c = ifs.get();
  if (c > 0xFF) return error;
  if (EOF == c) return end;
  out = T(c);
  return ok;
}

template <typename T>
static result UTF8(feeder& ifs, T& out) {

  const auto a = ifs.get();

  if (EOF == a) {
    return end;
  }

  if (bit<7>(a) == 0) {
    out = T(a);
    return ok;
  }

  const auto b = ifs.get();

  if (EOF == b) {
    return incomplete;
  }

  if (bit<6,2>(b) != 0b10) {
    return error;
  }

  if( bit<5,3>(a) == 0b110 ) {
    out = T((bit<0,5>(a) << 6) | bit<0,6>(b));
    return ok;
  }

  const auto c = ifs.get();

  if (EOF == c) {
    return incomplete;
  }
  if (bit<6,2>(c) != 0b10) {
    return error;
  }

  if (bit<4,4>(a)  == 0b1110) { // 1110aaaa 10bbbbbb 10cccccc
    out = T((bit<0,4>(a) << 12) | (bit<0,6>(b) << 6) | bit<0,6>(c));
    return ok;
  }

  const auto d = ifs.get();

  if (EOF == d) {
    return incomplete;
  }

  if (bit<6,2>(c) != 0b10) {
    return error;
  }

  if( bit<3,5>(a) == 0b11110 ) { // 11110aaa 10bbbbbb 10cccccc 10dddddd
    out = T((bit<0,4>(a) << 18) | (bit<0,6>(b) << 12) | (bit<0,6>(c) << 6) | bit<0,6>(d));
    return ok;
  }

  return error;
}

}
