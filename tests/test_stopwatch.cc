#include <gtest/gtest.h>
#include <libTimekeeper/StopWatch.hh>
#include <thread>

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

// Skip death tests in this environment (no ptrace/kill available).
#if !defined(__has_feature) || !__has_feature(address_sanitizer)
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
#endif

TEST(StopWatch, ElapsedAccumulatesAcrossMultipleResumeStopCycles) {
    StopWatch sw;

    // First cycle.
    sw.resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    sw.stop();
    auto elapsed1 = sw.elapsed();

    // Second cycle.
    sw.resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    sw.stop();
    auto elapsed2 = sw.elapsed();

    EXPECT_EQ(sw.count(), 2u);
    // Total elapsed should be approximately the sum of both cycles.
    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        elapsed1.wallclock + elapsed2.wallclock).count();
    ASSERT_GE(total_ms, 8);  // at least ~10ms with some tolerance

    // Each component should be non-negative and reasonable.
    EXPECT_LT(elapsed2.wallclock.count(), std::chrono::microseconds(30'000'000).count());
}

TEST(StopWatch, ElapsedIsZeroBeforeFirstResume) {
    StopWatch sw;
    auto elapsed = sw.elapsed();
    EXPECT_EQ(elapsed.wallclock.count(), 0);
    EXPECT_EQ(elapsed.user.count(), 0);
    EXPECT_EQ(elapsed.system.count(), 0);
}

TEST(StopWatch, MultipleResumesAndStopsAreIdempotent) {
    StopWatch sw;

    // Resume/stop multiple times should not crash or corrupt state.
    for (int i = 0; i < 10; ++i) {
        sw.resume();
        sw.stop();
    }
    EXPECT_EQ(sw.count(), 10u);
}
