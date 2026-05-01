#include <gtest/gtest.h>
#include <libTimekeeper/StopWatch.hh>

using namespace Timekeeper;

TEST(StopWatch, ResetZeroesElapsedAndCount) {
    StopWatch sw;
    EXPECT_EQ(sw.elapsed().wallclock.count(), 0);
    EXPECT_EQ(sw.count(), 0);

    // Simulate some timing activity
    sw.resume();
    sw.stop();
    auto elapsed_before = sw.elapsed();
    size_t count_before = sw.count();
    ASSERT_GE(count_before, 1u);

    // Reset should clear everything
    sw.reset();
    EXPECT_EQ(sw.elapsed().wallclock.count(), 0);
    EXPECT_EQ(sw.count(), 0);
}

TEST(StopWatch, ResumeIncrementsCount) {
    StopWatch sw;
    EXPECT_FALSE(sw.running());
    EXPECT_EQ(sw.count(), 0);

    sw.resume();
    EXPECT_TRUE(sw.running());
    // Count is incremented on resume.
    ASSERT_GE(sw.count(), 1u);
}

TEST(StopWatch, ResumeAssertsIfAlreadyRunning) {
    StopWatch sw;
    sw.resume();
    // This should trigger an assertion failure (assert(!running_))
    EXPECT_DEATH(sw.resume(), "");
}

TEST(StopWatch, StopAssertsIfNotRunning) {
    StopWatch sw;
    // This should trigger an assertion failure (assert(running_))
    EXPECT_DEATH(sw.stop(), "");
}
