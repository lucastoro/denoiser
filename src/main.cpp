#include <iostream>
#include <unistd.h>
#include <cstdlib>
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

  if (args.have_flag("--help", "-h")) {
    print_help(argv[0], std::cout);
    return 0;
  }

  if (args.have_flag("--verbose", "-v")) {
    log::enable(log::info);
  }

  if (args.have_flag("--debug", "-d")) {
    log::enable(log::debug);
    log::enable(log::info);
  }

  if (args.have_flag("--profile", "-p")) {
    log::enable(log::profile);
  }

#ifdef WITH_THREAD_POOL
  if (args.have_flag("--jobs", "-j")) {
    const auto count = args.value<size_t>("--jobs", "-j");
    if (0 == count) {
      std::cerr << "invalid value for the --jobs option" << std::endl;
      print_help(argv[0], std::cerr);
      return 1;
    }
    thread_pool::set_max_threads(count);
  }
#endif

  if (args.have_flag("--directory", "-d")) {
    const auto path = args.value("--directory", "-d");
    if (0 != chdir(path.data())) { // is null terminated anyway
      std::cerr << strerror(errno) << std::endl;
      return 1;
    }
    log_debug << "Current directory changed to " << path;
  }

#ifdef WITH_TESTS
  if (args.have_flag("--test", "-t")) {
    return test::run(argc, argv);
  }
#endif

  try {

    using char_t = wchar_t;

    const auto config_file = args.value("--config", "-c");

    const auto config = config_file.empty()
      ? configuration<char_t>::read(std::cin)
      : configuration<char_t>::load(std::string(config_file));

    if (config.empty()) {
      log_error << "Empty configuration";
      return 1;
    }

    if (log::has(log::debug)) {
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
