#include <iostream>

#include "artifact.hpp"
#include "profile.hpp"
#include "arguments.hpp"
#include "denoiser.hpp"
#include "config.hpp"
#include "help.hpp"
#ifdef WITH_TESTS
#  include "test/test.hpp"
#endif

int main(int argc, char** argv)
{
  const arguments args(argc, argv);
#ifdef WITH_TESTS
  if (args.has_flag("--test", "-t")) {
    return test::run(argc, argv);
  }
#endif
  if (args("--help", "-h")) {
    print_help(argv[0], std::cout);
    return 0;
  }

  if (args("--verbose", "-v")) {
    log_enable(LOG_INFO);
  }

  if (args("--debug", "-d")) {
    log_enable(LOG_DEBUG);
    log_enable(LOG_INFO);
  }

  if (args("--profile", "-p")) {
    log_enable(LOG_PROFILE);
  }

  const bool read_stdin = args("--stdin") or (args.back() == "-");
  const auto config_file = args.value("--config", "-c");

  if (not read_stdin and config_file.empty()) {
    log_error << "Missing argument, --stdin or --config must be speficied";
    print_help(argv[0], std::cerr);
    return 1;
  }

  if (read_stdin and config_file.size()) {
    log_error << "Invalid arguments, cannot specify both --stdin and --config";
    print_help(argv[0], std::cerr);
    return 1;
  }

  try {

    using char_t = wchar_t;

    const auto config = read_stdin
      ? configuration<char_t>::read(std::cin)
      : configuration<char_t>::load(std::string(config_file));

    if (config.empty()) {
      log_error << "Empty configuration";
      return 1;
    }

    if (log_has(LOG_DEBUG)) {
      log_debug << config.size() << " artifacts:";
      for (const auto& entry : config) {
        log_debug << " - " << entry.alias << "(" << entry.target << ")";
        for (const auto& ref : entry.reference) {
          log_debug << "   - " << ref;
        }
      }
    }

    std::string current = {};
    denoiser<char_t> denoiser(config);

    denoiser.run([&current](const artifact::wline& line){
      if (current != line.source().name()) {
        if (!current.empty()) {
          std::cout << "--- end " << current << " ---" << std::endl;
        }
        current = line.source().name();
        std::cout << "--- begin " << current << " ---" << std::endl;
      }
      std::wcout << line.number() << " " << line.str() << std::endl;
    });

  } catch (const std::exception& ex) {
    std::cerr << "exception got: " << ex.what() << std::endl << std::flush;
  }
  return 0;
}
