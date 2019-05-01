#include "gtest/gtest.h"
//#include "gmock/gmock.h"

#include "log-reader.hpp"

#include <chrono>

using namespace std::chrono_literals;

class ArtifactDenoiserTest : public testing::Test {
};

TEST(ArtifactDenoiserTest, local) {
  const auto x = log::file<wchar_t>("test/config.yaml");
  ASSERT_EQ(x.size(), 12);
  x.profile();
}

TEST(ArtifactDenoiserTest, http) {
  curlpp::Easy request;
  request.setOpt(curlpp::options::Url("http://www.example.com"));
  const auto x = log::file<wchar_t>(request);
  ASSERT_EQ(x.size(), 48);
  x.profile();
}

TEST(ArtifactDenoiserTest, https) {
  curlpp::Easy request;
  request.setOpt(curlpp::options::Url("https://www.example.com"));
  const auto x = log::file<wchar_t>(request);
  ASSERT_EQ(x.size(), 48);
  x.profile();
}

TEST(ArtifactDenoiserTest, large) {
  curlpp::Easy request;
  request.setOpt(curlpp::options::Url("http://jenkins.brightcomputing.com/view/tpsi%20trunk/job/trunk-c7u5-tpsi-ceph/lastSuccessfulBuild/artifact/logs/node-installer.log"));
  const auto x = log::file<wchar_t>(request);
  ASSERT_EQ(x.size(), 48564);
  x.profile();
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
