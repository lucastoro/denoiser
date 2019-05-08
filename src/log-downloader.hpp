#pragma once

#include <vector>
#include <string>
#include <regex>

#include "curlpp/cURLpp.hpp"
#include "curlpp/Easy.hpp"
#include "curlpp/Options.hpp"

#include "logging.hpp"
#include "encoding.hpp"

namespace log {

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

template <typename char_t>
class fetcher {
public:
  virtual void size_hint(size_t) = 0;
  virtual void on_data(char_t* data, size_t count) = 0;
};

template <typename char_t>
class dowloader {
public:
  dowloader(const std::string& url, data_t<char_t>& data)
    : data(data), decode(nullptr) {
    request.setOpt(curlpp::options::Url(url));
    request.setOpt(curlpp::options::NoSignal(true));
    request.setOpt(curlpp::options::Header(false));
    request.setOpt(curlpp::options::NoProgress(false));

    request.setOpt(curlpp::options::HeaderFunction(
    [this](char* data, size_t size, size_t count) -> size_t {
      return this->on_header(data, size * count);
    }));

    request.setOpt(curlpp::options::ProgressFunction(
    [this](double a, double b, double, double) -> int {
      return this->on_progress(a, b);
    }));

    request.setOpt(curlpp::options::WriteFunction(
      [this](char* ptr, size_t size, size_t count) -> size_t {
        return this->on_data(ptr, size * count);
      })
    );
  }

  void perform() {
    request.perform();
  }

private:

  using encoding_t = encoding::encoder<char_t>;

  size_t on_data(char* ptr, size_t size) {

    if (not decode) {
      log_warning("unknown encoding, defaulting to UTF8");
      decode = encoding::UTF8;
    }

    for (size_t s = 0; s < size; ++s) {
      feeder.push(ptr[s]);
      char_t c;
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

  int on_progress(double tot, double) {
    const auto x = size_t(tot);
    if (x > data.capacity()) {
      data.reserve(x);
    }
    return 0;
  }

  static constexpr bool icase(int a, int b){
    return (a == b) or (std::isalpha(a) and std::isalpha(b) and std::tolower(a) == std::tolower(b));
  }

  size_t on_header(char* ptr, size_t size) {
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
  data_t<char_t>& data;
  encoding::buffered_feeder feeder;
  encoding_t decode;
};

template <typename char_t>
class loader {
public:
  loader(std::istream& stream, data_t<char_t>& data) : feeder(stream), stream(stream), data(data), decode(encoding::UTF8) {}
  void perform() {
    stream.seekg(0, std::ios_base::seekdir::_S_end);
    const auto size = stream.tellg();
    stream.seekg(0);
    data.clear();
    data.reserve(size_t(size));
    bool stop = false;
    while (not stop) {
      char_t c;
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
  using encoding_t = encoding::encoder<char_t>;

  encoding::istream_feeder feeder;
  std::istream& stream;
  data_t<char_t>& data;
  encoding_t decode;
};

}
