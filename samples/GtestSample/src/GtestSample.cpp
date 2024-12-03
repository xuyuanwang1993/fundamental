
#include <gtest/gtest.h>
#include "fundamental/basic/log.h"
// Demonstrate some basic assertions.
TEST(GtestSample, BasicAssertions) {
  // Expect two strings not to be equal.
  EXPECT_STRNE("hello", "world");
  // Expect equality.
  EXPECT_EQ(7 * 6, 42);
}

int main(int argc, char **argv) {

    ::testing::InitGoogleTest(&argc, argv);
    FDEBUG("start test");
    return RUN_ALL_TESTS();

}