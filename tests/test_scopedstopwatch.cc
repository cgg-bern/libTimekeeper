#include <gtest/gtest.h>
#include <libTimekeeper/StopWatch.hh>
#include <thread>

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

TEST(ScopedStopWatch, NestedGuardsOnSameWatch) {
    StopWatch sw;

    {
        ScopedStopWatch outer{sw};
        EXPECT_TRUE(sw.running());
        size_t count_outer = sw.count();

        {
            // Inner guard sees watch is already running; does nothing.
            ScopedStopWatch inner{sw};
            EXPECT_EQ(sw.count(), count_outer);
            EXPECT_TRUE(sw.running());
        }
        // Outer guard still owns the stop; watch remains running.
        EXPECT_TRUE(sw.running());
        EXPECT_EQ(sw.count(), count_outer);
    }
    // Only outer guard's destruction fires; watch is stopped once.
    EXPECT_FALSE(sw.running());
    EXPECT_EQ(sw.count(), 1u);
}

TEST(ScopedStopWatch, MultipleGuardsOnDifferentWatches) {
    StopWatch sw_a;
    StopWatch sw_b;

    {
        ScopedStopWatch guard_a{sw_a};
        ScopedStopWatch guard_b{sw_b};

        EXPECT_TRUE(sw_a.running());
        EXPECT_TRUE(sw_b.running());
        EXPECT_EQ(sw_a.count(), 1u);
        EXPECT_EQ(sw_b.count(), 1u);
    }

    EXPECT_FALSE(sw_a.running());
    EXPECT_FALSE(sw_b.running());
}

TEST(ScopedStopWatch, AccumulatesTimeAcrossNestedScopes) {
    StopWatch sw;

    {
        ScopedStopWatch guard{sw};
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }
    // After first scope, elapsed should be ~3ms.
    auto elapsed1 = sw.elapsed();
    EXPECT_GT(elapsed1.wallclock.count(), 0);

    {
        ScopedStopWatch guard{sw};
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }
    // After second scope, total should be ~6ms.
    auto elapsed2 = sw.elapsed();
    EXPECT_GT(elapsed2.wallclock.count(), elapsed1.wallclock.count());
    EXPECT_EQ(sw.count(), 2u);
}


