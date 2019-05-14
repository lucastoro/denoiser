#include "gtest/gtest.h"
//#include "gmock/gmock.h"

#include "log-reader.hpp"
#include "logging.hpp"
#include <chrono>

using namespace std::chrono_literals;

class ArtifactDenoiserTest : public testing::Test {
public:
  void SetUp() {

  }
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

TEST_F(ArtifactDenoiserTest, line_remove_regex) {
  char local1[] = "test 1234 rofl";
  const char local2[] = "test 1234 rofl";
  log::basic_line<char, int> line(nullptr, 0, local1, local2, sizeof(local1) - 1);
  log::pattern pattern(std::regex("\\d+"));
  line.remove(pattern);
  ASSERT_EQ(line.str(), local2);
  ASSERT_EQ(line.mut(), "test  rofl");
}

TEST_F(ArtifactDenoiserTest, line_remove_regex_multi) {
  char local1[] = "test 1234 1234 rofl";
  const char local2[] = "test 1234 1234 rofl";
  log::basic_line<char, int> line(nullptr, 0, local1, local2, sizeof(local1) - 1);
  log::pattern pattern(std::regex("\\d+"));
  line.remove(pattern);
  ASSERT_EQ(line.str(), local2);
  ASSERT_EQ(line.mut(), "test   rofl");
}

TEST_F(ArtifactDenoiserTest, line_remove_string) {
  char local1[] = "test 1234 rofl";
  const char local2[] = "test 1234 rofl";
  log::basic_line<char, int> line(nullptr, 0, local1, local2, sizeof(local1) - 1);
  log::pattern pattern("1234");
  line.remove(pattern);
  ASSERT_EQ(line.str(), local2);
  ASSERT_EQ(line.mut(), "test  rofl");
}

TEST_F(ArtifactDenoiserTest, line_remove_string_multi) {
  char local1[] = "test 1234 1234 rofl";
  const char local2[] = "test 1234 1234 rofl";
  log::basic_line<char, int> line(nullptr, 0, local1, local2, sizeof(local1) - 1);
  log::pattern pattern("1234");
  line.remove(pattern);
  ASSERT_EQ(line.str(), local2);
  ASSERT_EQ(line.mut(), "test   rofl");
}

TEST_F(ArtifactDenoiserTest, line_suppress_regex) {
  char local1[] = "test 1234 rofl";
  const char local2[] = "test 1234 rofl";
  log::basic_line<char, int> line(nullptr, 0, local1, local2, sizeof(local1) - 1);
  log::pattern pattern(std::regex("\\d+"));
  line.suppress(pattern);
  ASSERT_EQ(line.str(), local2);
  ASSERT_EQ(line.mut().size(), 0);
}

TEST_F(ArtifactDenoiserTest, line_suppress_string) {
  char local1[] = "test 1234 rofl";
  const char local2[] = "test 1234 rofl";
  log::basic_line<char, int> line(nullptr, 0, local1, local2, sizeof(local1) - 1);
  log::pattern pattern("123");
  line.suppress(pattern);
  ASSERT_EQ(line.str(), local2);
  ASSERT_EQ(line.mut().size(), 0);
}

#include "thread-pool.hpp"

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

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
