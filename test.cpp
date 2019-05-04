#include "gtest/gtest.h"
//#include "gmock/gmock.h"

#include "log-reader.hpp"

#include <chrono>

using namespace std::chrono_literals;

class ArtifactDenoiserTest : public testing::Test {
public:
  virtual void SetUp() override {
    set_log_level(LOG_DEBUG);
  }
};

TEST_F(ArtifactDenoiserTest, local) {
  try {
    const auto x = log::wfile("test/utf8.txt");
    ASSERT_EQ(x.size(), 3);
    ASSERT_EQ(x.at(0).str().at(0), 'A');
    ASSERT_EQ(x.at(1).str().at(0), 0x00A9);
    ASSERT_EQ(x.at(2).str().at(0), 0x2764);
  } catch (std::exception& ex) {
    error(ex.what());
  }
}

TEST_F(ArtifactDenoiserTest, missing_local) {
  ASSERT_THROW(log::wfile("test/none.txt"), log::wfile::not_found);
}

TEST_F(ArtifactDenoiserTest, http) {
  curlpp::Easy request;
  request.setOpt(curlpp::options::Url("http://www.example.com"));
  const auto x = log::wfile(request);
  ASSERT_EQ(x.size(), 48);
}

TEST_F(ArtifactDenoiserTest, https) {
  curlpp::Easy request;
  request.setOpt(curlpp::options::Url("https://www.example.com"));
  const auto x = log::wfile(request);
  ASSERT_EQ(x.size(), 48);
}

TEST_F(ArtifactDenoiserTest, http_large) {
  curlpp::Easy request;
  request.setOpt(curlpp::options::Url("https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/drivers/gpu/drm/amd/include/asic_reg/nbio/nbio_6_1_sh_mask.h"));
  const auto x = log::wfile(request);
  ASSERT_EQ(x.size(), 133634);
}

TEST_F(ArtifactDenoiserTest, http_no_host) {
  curlpp::Easy request;
  request.setOpt(curlpp::options::Url("https://www.foo.bar/none.txt"));
  ASSERT_THROW(const auto x = log::wfile(request), log::wfile::not_found);
}

TEST_F(ArtifactDenoiserTest, http_no_resource) {
  curlpp::Easy request;
  request.setOpt(curlpp::options::Url("http://www.example.com/foo"));
  ASSERT_THROW(const auto x = log::wfile(request), log::wfile::not_found);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
