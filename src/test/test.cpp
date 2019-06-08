#include "gtest/gtest.h"
//#include "gmock/gmock.h"

#include "artifact.hpp"
#include "logging.hpp"
#include "thread-pool.hpp"
#include "denoiser.hpp"
#include <chrono>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>

using namespace std::chrono_literals;

class directory {
public:

  class entry {
  public:
    entry() : path(nullptr), dirent(nullptr) {}
    entry(const std::string* p, struct dirent* e) : path(p), dirent(e) {}
    std::string name() const {
      return nullptr == path
        ? std::string()
        : (path->back() == '/')
          ? *path + dirent->d_name
          : *path + '/' + dirent->d_name;
    }
    bool is_directory() const {
      return DT_DIR == dirent->d_type;
    }
    bool is_file() const {
      return DT_REG == dirent->d_type;
    }
    bool contains(const char* filename) const {
      const auto p = name();
      const auto full = p + ((p.back() != '/' and filename[0] != '/') ? "/" : "") + filename;
      struct stat info;
      if (0 != stat(full.c_str(), &info)) {
        return false;
      }
      return true;
    }
  private:
    const std::string* path;
    struct dirent* dirent;
  };

  directory(const char* p) : path(p) {}
  directory(const std::string& p) : path(p) {}

  class const_iterator {
  public:
    const_iterator(std::nullptr_t) : dir(nullptr), dirent(nullptr), path(nullptr) {
    }

    const_iterator(const std::string& p) : dir(nullptr), dirent(nullptr), path(&p) {

      dir = opendir(p.c_str());

      if(!dir) {
        return;
      }

      ++(*this);
    }
    ~const_iterator() {
      if (dir) {
        closedir(dir);
      }
    }
    entry operator * () const {
      return entry(path, dirent);
    }
    void operator ++ () {
      if (!dir) return;
      do {
        dirent = readdir(dir);
      } while (
        dirent and (
        std::string_view(dirent->d_name) == "." or
        std::string_view(dirent->d_name) == ".." or
        dirent->d_type != DT_DIR)
      );
    }
    bool operator == (const const_iterator& other) const {
      return dirent == other.dirent;
    }
    bool operator != (const const_iterator& other) const {
      return dirent != other.dirent;
    }
  private:
    DIR* dir;
    struct dirent* dirent;
    const std::string* path;
  };

  const_iterator begin() const { return const_iterator(path); }
  const_iterator end() const { return const_iterator(nullptr); }

private:
  std::string path;
};

class pushd final {
public:
  pushd(const char* newpath) noexcept (false) : path(cwd()) {
    if (0 != chdir(newpath)) {
      throw std::runtime_error(strerror(errno));
    }
  }
  pushd(const std::string& newpath) noexcept (false) : pushd(newpath.c_str()) {
  }
  ~pushd() noexcept (false) {
    if (0 != chdir(path.c_str())) {
      throw std::runtime_error(strerror(errno));
    }
  }
  static std::string cwd() {
    char* const ptr = get_current_dir_name();
    const std::string result(ptr);
    free(ptr);
    return result;
  }
private:
  std::string path;
};

template <typename C>
class log_line {
public:
  explicit log_line(std::basic_ostream<C>& os) : os(os) {
  }
  template <typename T>
  explicit log_line(std::basic_ostream<C>& os, const T& t) : os(os) {
    os << t;
  }
  ~log_line() {
    os << std::endl;
  }
  template <typename T>
  log_line& operator << (const T& t) {
    os << t;
    return *this;
  }
private:
  std::basic_ostream<C>& os;
};

#define info log_line<char>(std::cout, "[   INFO   ] ")
#define warn log_line<char>(std::cerr, "[ WARNING  ] ")
#define winfo log_line<wchar_t>(std::wcout, L"[   INFO   ] ")
#define wwarn log_line<wchar_t>(std::wcerr, L"[ WARNING  ] ")

class ArtifactDenoiserTest : public testing::Test {
public:
};

class DataDrivenTest : public ::testing::Test, public ::testing::WithParamInterface<const char*> {
public:
  DataDrivenTest(const std::string& path) : path(path) {}
protected:
  void TestBody() override {
    const pushd dir(path);
    const auto config = configuration<wchar_t>::load("config.yaml");
    denoiser<wchar_t> denoiser(config);
    std::vector<std::wstring_view> result;
    artifact::wfile expected = artifact::wfile::load("expect.log");
    denoiser.run([&result](const artifact::wline& line){
      result.push_back(line.str());
    });

    if (expected.size() != result.size()) {
      wwarn << "Expected:";
      for (size_t i = 0; i < expected.size(); ++i) {
        wwarn << "+" << i << " " << expected.at(i).str();
      }
      wwarn << "Result:";
      for (size_t i = 0; i < result.size(); ++i) {
        wwarn << "+" << i << " " << result.at(i);
      }
    }

    ASSERT_EQ(expected.size(), result.size());

    auto it = result.begin();
    for (const auto& line : expected) {
      ASSERT_EQ(*it, line.str()); ++it;
    }
  }
private:
  std::string path;
};

TEST_F(ArtifactDenoiserTest, local) {
  artifact::wfile x;
  ASSERT_NO_THROW(x = artifact::wfile::load("test/utf8.txt"));
  ASSERT_EQ(x.size(), 3);
  ASSERT_EQ(x.at(0).str().at(0), 'A');
  ASSERT_EQ(x.at(1).str().at(0), 0x00A9);
  ASSERT_EQ(x.at(2).str().at(0), 0x2764);
}

TEST_F(ArtifactDenoiserTest, http) {
  const auto x = artifact::wfile::download("http://www.example.com");
  ASSERT_EQ(x.size(), 48);
}

TEST_F(ArtifactDenoiserTest, https) {
  const auto x = artifact::wfile::download("https://www.example.com");
  ASSERT_EQ(x.size(), 48);
}

TEST_F(ArtifactDenoiserTest, large) {
  const auto url = "https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/drivers/gpu/drm/amd/include/asic_reg/nbio/nbio_6_1_sh_mask.h";
  const auto x = artifact::wfile::download(url);
  ASSERT_EQ(x.size(), 133634);
}

TEST_F(ArtifactDenoiserTest, load_config_missing) {
  ASSERT_THROW(configuration<wchar_t>::load("nope.yaml"), std::runtime_error);
}

TEST_F(ArtifactDenoiserTest, load_config) {
  const auto config = configuration<wchar_t>::load("test/test1.yaml");
  ASSERT_EQ(config.size(), 5);
  for (const auto& entry : config) {
    ASSERT_EQ(entry.reference.size(), 3);
  }
}

TEST_F(ArtifactDenoiserTest, line_remove_regex) {
  char local1[] = "test 1234 rofl";
  const char local2[] = "test 1234 rofl";
  artifact::line line(nullptr, 0, local1, local2, sizeof(local1) - 1);
  artifact::pattern pattern(std::regex("\\d+"));
  line.remove(pattern);
  ASSERT_EQ(line.str(), local2);
  ASSERT_EQ(line.mut(), "test  rofl");
}

TEST_F(ArtifactDenoiserTest, line_remove_regex_multi) {
  char local1[] = "test 1234 1234 rofl";
  const char local2[] = "test 1234 1234 rofl";
  artifact::line line(nullptr, 0, local1, local2, sizeof(local1) - 1);
  artifact::pattern pattern(std::regex("\\d+"));
  line.remove(pattern);
  ASSERT_EQ(line.str(), local2);
  ASSERT_EQ(line.mut(), "test   rofl");
}

TEST_F(ArtifactDenoiserTest, line_remove_string) {
  char local1[] = "test 1234 rofl";
  const char local2[] = "test 1234 rofl";
  artifact::line line(nullptr, 0, local1, local2, sizeof(local1) - 1);
  artifact::pattern pattern("1234");
  line.remove(pattern);
  ASSERT_EQ(line.str(), local2);
  ASSERT_EQ(line.mut(), "test  rofl");
}

TEST_F(ArtifactDenoiserTest, line_remove_string_multi) {
  char local1[] = "test 1234 1234 rofl";
  const char local2[] = "test 1234 1234 rofl";
  artifact::line line(nullptr, 0, local1, local2, sizeof(local1) - 1);
  artifact::pattern pattern("1234");
  line.remove(pattern);
  ASSERT_EQ(line.str(), local2);
  ASSERT_EQ(line.mut(), "test   rofl");
}

TEST_F(ArtifactDenoiserTest, line_suppress_regex) {
  char local1[] = "test 1234 rofl";
  const char local2[] = "test 1234 rofl";
  artifact::line line(nullptr, 0, local1, local2, sizeof(local1) - 1);
  artifact::pattern pattern(std::regex("\\d+"));
  line.suppress(pattern);
  ASSERT_EQ(line.str(), local2);
  ASSERT_EQ(line.mut().size(), 0);
}

TEST_F(ArtifactDenoiserTest, line_suppress_string) {
  char local1[] = "test 1234 rofl";
  const char local2[] = "test 1234 rofl";
  artifact::line line(nullptr, 0, local1, local2, sizeof(local1) - 1);
  artifact::pattern pattern("123");
  line.suppress(pattern);
  ASSERT_EQ(line.str(), local2);
  ASSERT_EQ(line.mut().size(), 0);
}

TEST(ThreadPoolTest, single) {
  thread_pool pool(1);
  int x = 0;
  pool.wait(pool.submit([&x](){
    x = 1;
  }));

  ASSERT_EQ(x, 1);
}

TEST(ThreadPoolTest, double) {
  thread_pool pool(2);
  int x = 0;
  std::vector<thread_pool::id_t> jobs;
  jobs.emplace_back(pool.submit([&x](){
    x += 1;
  }));
  jobs.emplace_back(pool.submit([&x](){
    x += 1;
  }));
  pool.wait(jobs);
  ASSERT_EQ(x, 2);
}

bool register_data_driven_tests() {
  size_t count = 0;
  for (auto entry : directory("test/ddt")) {
    if (entry.is_directory() and entry.contains("config.yaml")) {
      if (!entry.contains("expect.log")) {
        warn << "Directory " << entry.name() << " is missing the expect.log file";
        continue;
      }
      ++count;
      const auto name = entry.name();
      ::testing::RegisterTest(
        "DataDrivenTest", name.c_str(), nullptr,
        name.c_str(),
        __FILE__, __LINE__,
        // Important to use the fixture type as the return type here.
        [name]() -> DataDrivenTest* { return new DataDrivenTest(name); }
      );

      info << "Registered DDT " << name;
    }
  }
  if (0 == count) {
    warn << "No DDT found, please check the current directory";
  }

  return 0 != count;
}

namespace test {
  int run(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    register_data_driven_tests();
    return RUN_ALL_TESTS();
  }
}