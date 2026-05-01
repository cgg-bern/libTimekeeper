#include <gtest/gtest.h>
#include <libTimekeeper/StopWatch.hh>

using namespace Timekeeper;

TEST(ScopedStopWatch, ResumesStoppedWatchAndStopsOnDestruction) {
    StopWatch sw;
    EXPECT_FALSE(sw.running());
    EXPECT_EQ(sw.count(), 0);

    {
        ScopedStopWatch guard{sw};
        // After construction, the watch should be running.
        EXPECT_TRUE(sw.running());
        ASSERT_GE(sw.count(), 1u);
    }
    // After destruction, the watch should be stopped again.
    EXPECT_FALSE(sw.running());
}

TEST(ScopedStopWatch, DoesNothingIfWatchAlreadyRunning) {
    StopWatch sw;
    sw.resume();
    size_t count_before = sw.count();

    {
        ScopedStopWatch guard{sw};
        // Watch was already running, so guard should not resume or stop.
        EXPECT_TRUE(sw.running());
        // Count should not have been incremented by the guard.
        EXPECT_EQ(sw.count(), count_before);
    }
    // Still running after guard destruction (guard didn't stop it).
    EXPECT_TRUE(sw.running());

    sw.stop();  // Clean up the outer resume.
}

TEST(ScopedStopWatch, MustBeBoundToVariable) {
    StopWatch sw;

    // This should produce a [[nodiscard]] warning:
    // ScopedStopWatch{sw};  // temporary destroyed at semicolon!
    // The test below proves the guard actually works when properly bound.
    auto guard = ScopedStopWatch{sw};
    EXPECT_TRUE(sw.running());
}
