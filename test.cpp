#include "gtest/gtest.h"
//#include "gmock/gmock.h"

#include "log-reader.hpp"

#include <chrono>

using namespace std::chrono_literals;

size_t log_level = LOG_DEBUG;

class ArtifactDenoiserTest : public testing::Test {
public:
};

TEST_F(ArtifactDenoiserTest, local) {
  const auto x = log::wfile::load("test/utf8.txt");
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

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
