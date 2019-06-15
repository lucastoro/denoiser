#include <iostream>
#include <iomanip>
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

template <typename T>
static constexpr std::basic_ostream<T>& get_cout() {
  return *reinterpret_cast<std::basic_ostream<T>*>(0xdeadbeaf);
}

template <char>
static inline constexpr std::basic_ostream<char>& get_cout() {
  return std::cout;
}

template <wchar_t>
static inline constexpr std::basic_ostream<wchar_t>& get_cout() {
  return std::wcout;
}

int main(int argc, char** argv) {
  const arguments args(argc, argv);

  if (args.have_flag("--help", "-h")) {
    print_help(argv[0], std::cout);
    return 0;
  }

  if (args.have_flag("--verbose", "-v")) {
    log::enable(log::info, log::warning);
  }

  if (args.have_flag("--debug", "-d")) {
    log::enable(log::debug);
  }

  if (args.have_flag("--profile", "-p")) {
    log::enable(log::profile);
  }

  const bool show_lines = not args.have_flag("--no-lines", "-n");

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

    denoiser<char_t> denoiser(config);

    static auto& cout = get_cout<char_t>();

    denoiser.run([show_lines](const artifact::wline& line){
      if (show_lines) {
        cout << line.number() << " " << line.str() << std::endl;
      } else {
        cout << line.str() << std::endl;
      }
    });

  } catch (const std::exception& ex) {
    std::cerr << "exception got: " << ex.what() << std::endl;
  }
  return 0;
}
