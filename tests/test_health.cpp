#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

TEST(Stage3Smoke, JsonRoundTrip) {
  nlohmann::json j;
  j["stage"] = 3;
  j["ok"] = true;
  EXPECT_EQ(j["stage"], 3);
  EXPECT_TRUE(j["ok"].get<bool>());
}
