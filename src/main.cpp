#include <iostream>

#include <gflags/gflags.h>

#include "yaml-cpp/yaml.h"
#include "curlpp/cURLpp.hpp"
#include "curlpp/Easy.hpp"
#include "curlpp/Options.hpp"

#include "log-reader.hpp"

namespace gflags {

class Context final {
public:
  inline Context(int argc, char** argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
  }
  inline ~Context() {
    gflags::ShutDownCommandLineFlags();
  }
};
}

DEFINE_string(config, "config.yaml", "Configuration file");

int main(int argc, char** argv)
{
#if 0
  if (0) {
    auto config = YAML::LoadFile(FLAGS_config);
    const auto username = config["username"].as<std::string>();
    const auto password = config["password"].as<std::string>();
  }

  if (0) {
    curlpp::options::Url("http://example.com");
    curlpp::Easy request;
    request.setOpt(curlpp::options::Url("http://example.com"));
    request.setOpt(new curlpp::options::WriteStream(&std::cout));
    request.perform();
  }
#endif

  const gflags::Context gfc_(argc, argv);
  const curlpp::Cleanup curlpp_;

  log::file file("/home/luca/Projects/ArtifactDenoiser/test.log");

  assert(file.size() == 3);

  std::wregex rx(L"lo wo");

  for (auto& line : file) {
    std::wcout << line.str() << std::endl;
    if (line.remove(rx)) {
      std::wcout << line.str() << std::endl;
    }
  }

  return 0;
}
