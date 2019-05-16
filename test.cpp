#include "gtest/gtest.h"
//#include "gmock/gmock.h"

#include "log-reader.hpp"
#include "logging.hpp"
#include "thread-pool.hpp"
#include "denoiser.hpp"
#include <chrono>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>

using namespace std::chrono_literals;

class subdir {
public:
  subdir(const char* p) : path(p) {}
  subdir(const std::string& p) : path(p) {}

  class const_iterator {
  public:
    const_iterator(std::nullptr_t) : dir(nullptr), entry(nullptr), path(nullptr) {
    }

    const_iterator(const std::string& p) : dir(nullptr), entry(nullptr), path(&p) {

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
    std::string operator * () const {
      if (!entry) return {};
      return (path->back() == '/')
        ? *path + entry->d_name
        : *path + '/' + entry->d_name;
    }
    void operator ++ () {
      if (!entry) return;
      do {
        entry = readdir(dir);
      } while (
        entry and (
        std::string_view(entry->d_name) == "." or
        std::string_view(entry->d_name) == ".." or
        entry->d_type != DT_DIR)
      );
    }
    bool operator == (const const_iterator& other) const {
      return entry == other.entry;
    }
    bool operator != (const const_iterator& other) const {
      return entry != other.entry;
    }
  private:
    DIR* dir;
    struct dirent* entry;
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

class ArtifactDenoiserTest : public testing::Test {
public:
};

class DataDrivenTest : public ::testing::Test {
 public:
  explicit DataDrivenTest(const std::string& p) : path(p) {
  }
  void TestBody() override {
    const pushd dir(path);
    const auto config = configuration<wchar_t>::load("config.yaml");
    denoiser<wchar_t> denoiser(config);
    std::vector<std::wstring_view> result;
    log::wfile expected = log::wfile::load("expect.log");
    denoiser.run([&result](const log::wline& line){
      result.push_back(line.str());
    });

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
  log::wfile x;
  ASSERT_NO_THROW(x = log::wfile::load("test/utf8.txt"));
  ASSERT_EQ(x.size(), 3);
  ASSERT_EQ(x.at(0).str().at(0), 'A');
  ASSERT_EQ(x.at(1).str().at(0), 0x00A9);
  ASSERT_EQ(x.at(2).str().at(0), 0x2764);
}

TEST_F(ArtifactDenoiserTest, http) {
  const auto x = log::wfile::download("http://www.example.com");
  ASSERT_EQ(x.size(), 48);
}

TEST_F(ArtifactDenoiserTest, https) {
  const auto x = log::wfile::download("https://www.example.com");
  ASSERT_EQ(x.size(), 48);
}

TEST_F(ArtifactDenoiserTest, large) {
  const auto url = "https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/drivers/gpu/drm/amd/include/asic_reg/nbio/nbio_6_1_sh_mask.h";
  const auto x = log::wfile::download(url);
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
  log::line line(nullptr, 0, local1, local2, sizeof(local1) - 1);
  log::pattern pattern(std::regex("\\d+"));
  line.remove(pattern);
  ASSERT_EQ(line.str(), local2);
  ASSERT_EQ(line.mut(), "test  rofl");
}

TEST_F(ArtifactDenoiserTest, line_remove_regex_multi) {
  char local1[] = "test 1234 1234 rofl";
  const char local2[] = "test 1234 1234 rofl";
  log::line line(nullptr, 0, local1, local2, sizeof(local1) - 1);
  log::pattern pattern(std::regex("\\d+"));
  line.remove(pattern);
  ASSERT_EQ(line.str(), local2);
  ASSERT_EQ(line.mut(), "test   rofl");
}

TEST_F(ArtifactDenoiserTest, line_remove_string) {
  char local1[] = "test 1234 rofl";
  const char local2[] = "test 1234 rofl";
  log::line line(nullptr, 0, local1, local2, sizeof(local1) - 1);
  log::pattern pattern("1234");
  line.remove(pattern);
  ASSERT_EQ(line.str(), local2);
  ASSERT_EQ(line.mut(), "test  rofl");
}

TEST_F(ArtifactDenoiserTest, line_remove_string_multi) {
  char local1[] = "test 1234 1234 rofl";
  const char local2[] = "test 1234 1234 rofl";
  log::line line(nullptr, 0, local1, local2, sizeof(local1) - 1);
  log::pattern pattern("1234");
  line.remove(pattern);
  ASSERT_EQ(line.str(), local2);
  ASSERT_EQ(line.mut(), "test   rofl");
}

TEST_F(ArtifactDenoiserTest, line_suppress_regex) {
  char local1[] = "test 1234 rofl";
  const char local2[] = "test 1234 rofl";
  log::line line(nullptr, 0, local1, local2, sizeof(local1) - 1);
  log::pattern pattern(std::regex("\\d+"));
  line.suppress(pattern);
  ASSERT_EQ(line.str(), local2);
  ASSERT_EQ(line.mut().size(), 0);
}

TEST_F(ArtifactDenoiserTest, line_suppress_string) {
  char local1[] = "test 1234 rofl";
  const char local2[] = "test 1234 rofl";
  log::line line(nullptr, 0, local1, local2, sizeof(local1) - 1);
  log::pattern pattern("123");
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

void register_data_driven_tests() {
  for (const auto& dir : subdir("test/ddt")) {
    ::testing::RegisterTest(
      "DataDrivenTest", dir.c_str(), nullptr,
      dir.c_str(),
      __FILE__, __LINE__,
      // Important to use the fixture type as the return type here.
      [=]() -> DataDrivenTest* { return new DataDrivenTest(dir); }
    );
  }
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  register_data_driven_tests();
  return RUN_ALL_TESTS();
}
