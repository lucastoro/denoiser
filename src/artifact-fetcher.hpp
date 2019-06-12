#pragma once

#include <vector>
#include <string>
#include <regex>

#include "curlpp/cURLpp.hpp"
#include "curlpp/Easy.hpp"
#include "curlpp/Options.hpp"

#include "logging.hpp"
#include "encoding.hpp"

namespace artifact {

template <typename char_t>
class data_consumer {
public:
  virtual void size_hint(size_t) = 0;
  virtual void on_data(const char_t* data, size_t count) = 0;
};

template <typename char_t>
class downloader {
public:
  downloader(const std::string& url, data_consumer<char_t>& observer)
    : observer(observer), decode(nullptr) {
    request.setOpt(curlpp::options::Url(url));
    request.setOpt(curlpp::options::Header(false));
    request.setOpt(curlpp::options::NoSignal(true));
    request.setOpt(curlpp::options::NoProgress(true));

    request.setOpt(curlpp::options::HeaderFunction(
    [this](char* data, size_t size, size_t count) -> size_t {
      return this->on_header(data, size * count);
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

  using encoding_t = encoding::basic_encoder<char_t>;

  size_t on_data(char* ptr, size_t size) {

    if (not decode) {
      log_warning << "unknown encoding, defaulting to UTF8";
      decode = encoding::UTF8;
    }

    for (size_t s = 0; s < size; ++s) {
      feeder.push(ptr[s]);
      char_t c;
      switch(decode(feeder, c)) {
        case encoding::ok:
          observer.on_data(&c, 1);
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

  static constexpr bool icase(int a, int b){
    return (a == b) or (std::isalpha(a) and std::isalpha(b) and std::tolower(a) == std::tolower(b));
  }

  void parse_content_length(const std::string_view& clength) {
    char temp[128];
    if (sizeof(temp) < clength.size() + 1) {
      log_warning << "invalid Conten-Length field: '" << clength << "'";
      observer.size_hint(1024 * 1024);
    } else {
      std::copy(clength.begin(), clength.end(), temp);
      temp[clength.size()] = 0;
      observer.size_hint(atoll(temp));
    }
  }

  void parse_content_type(const std::string_view& ctype) {
    static const std::regex charset_rx(R"(charset=([^ ]+))", std::regex::optimize);
    std::cmatch charset;
    if (std::regex_search(ctype.begin(), ctype.end(), charset, charset_rx) and
        2 == charset.size()) {

      bool found = false;

      for (const auto& enco : encodings) {
        const auto begin = enco.first;
        const auto end = enco.first + strlen(enco.first);
        if (std::equal(charset[1].first,
                       charset[1].second,
                       begin,
                       end,
                       downloader<char_t>::icase)) {
          log_debug << "using encoding: utf8";
          decode = enco.second;
          found = true;
          break;
        }
      }

      if (not found) {
        log_warning << "unknown content type: " << ctype;
      }

    } else {
      log_debug << "Content-Type received, but missing encoding, defaulting to latin1";
      decode = encoding::LATIN1;
    }
  }

  size_t on_header(char* ptr, size_t size) {
    static const std::regex ctype_rx(R"(^[Cc]ontent-[Tt]ype: (.+))", std::regex::optimize);
    static const std::regex cleng_rx(R"(Content-Length: (\d+))", std::regex::optimize);

    std::cmatch match;
    std::string_view header(ptr, size);

    if (std::regex_search(header.begin(), header.end(), match, ctype_rx) and 2 == match.size()) {
      parse_content_type(std::string_view(match[1].first, match[1].length()));
    }

    if (std::regex_search(header.begin(), header.end(), match, cleng_rx) and 2 == match.size()) {
      parse_content_length(std::string_view(match[1].first, match[1].length()));
    }

    return size;
  }

  static constexpr std::pair<const char*, encoding_t> encodings[] = {
    {"utf-8", encoding::UTF8},
    {"us-ascii", encoding::ASCII},
    {"iso-8859-1", encoding::LATIN1},
  };

  curlpp::Easy request;
  data_consumer<char_t>& observer;
  encoding::buffered_feeder feeder;
  encoding_t decode;
};

template <typename char_t>
class loader {
public:
  loader(std::istream& stream, data_consumer<char_t>& observer) : feeder(stream), stream(stream), observer(observer), decode(encoding::UTF8) {}
  void perform() {
    stream.seekg(0, std::ios_base::seekdir::_S_end);
    const auto size = stream.tellg();
    stream.seekg(0);
    observer.size_hint(size_t(size));
    bool stop = false;
    while (not stop) {
      char_t c;
      switch(decode(feeder, c)) {
        case encoding::ok:
          observer.on_data(&c, 1);
          break;
        case encoding::end:
          stop = true;
          break;
        case encoding::error:
        case encoding::incomplete:
          log_error << "bad char";
          stop = true;
          break;
      }
    }
  }
private:
  using encoding_t = encoding::basic_encoder<char_t>;

  encoding::istream_feeder feeder;
  std::istream& stream;
  data_consumer<char_t>& observer;
  encoding_t decode;
};

}
